/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <curl/curl.h>

#include <functional>
#include <mutex>
#include <thread>

#include "../Timers.h"
#include "../log.h"
#include "../settings.h"

#include "MediaCommands.h"
#include "../mediaoutput/GStreamerOut.h"
#include "../mediaoutput/StreamSlotManager.h"

SetVolumeCommand::SetVolumeCommand() :
    Command("Volume Set", "Sets the volume to the specific value. (0 - 100)") {
    args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("70").setGetAdjustableValueURL("api/fppd/volume?simple=true"));
}
std::unique_ptr<Command::Result> SetVolumeCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int v = std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}

AdjustVolumeCommand::AdjustVolumeCommand() :
    Command("Volume Adjust", "Adjust volume either up or down by the given amount. (-100 - 100)") {
    args.push_back(CommandArg("volume", "int", "Volume").setRange(-100, 100).setDefaultValue("0"));
}
std::unique_ptr<Command::Result> AdjustVolumeCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int v = getVolume();
    v += std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}

IncreaseVolumeCommand::IncreaseVolumeCommand() :
    Command("Volume Increase", "Increases the volume by the given amount (0 - 100)") {
    args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("0"));
}
std::unique_ptr<Command::Result> IncreaseVolumeCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    int v = getVolume();
    v += std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}

DecreaseVolumeCommand::DecreaseVolumeCommand() :
    Command("Volume Decrease", "Decreases the volume by the given amount (0 - 100)") {
    args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("0"));
}
std::unique_ptr<Command::Result> DecreaseVolumeCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int v = getVolume();
    v -= std::atoi(args[0].c_str());
    setVolume(v);
    return std::make_unique<Command::Result>("Volume Set");
}

static size_t URL_write_data(void* buffer, size_t size, size_t nmemb, void* userp) {
    std::string* msg = (std::string*)userp;
    msg->append(static_cast<const char*>(buffer), size * nmemb);
    return size * nmemb;
}

class CURLResult : public Command::Result {
public:
    CURLResult(const std::vector<std::string>& args) :
        Command::Result() {
        std::string url = args[0];
        LogDebug(VB_COMMAND, "URL: \"%s\"\n", url.c_str());

        std::string method = "GET";
        if (args.size() > 1) {
            method = args[1];
            if (method == "") {
                method = "GET";
            }
        }
        std::string data;
        if (args.size() > 2) {
            data = args[2];
        }
        m_curl = curl_easy_init();
        curl_easy_setopt(m_curl, CURLOPT_NOSIGNAL, 1);

        CURLcode status;
        status = curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
        if (status != CURLE_OK) {
            cleanupCurl();
            return;
        }
        status = curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, URL_write_data);
        if (status != CURLE_OK) {
            cleanupCurl();
            return;
        }

        status = curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &m_result);
        if (status != CURLE_OK) {
            cleanupCurl();
            return;
        }

        if (method == "POST") {
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, data.size());
            status = curl_easy_setopt(m_curl, CURLOPT_COPYPOSTFIELDS, data.c_str());
            if (status != CURLE_OK) {
                cleanupCurl();
                return;
            }
        }

        m_curlm = curl_multi_init();
        CURLMcode mstatus = curl_multi_add_handle(m_curlm, m_curl);
        if (mstatus != CURLM_OK) {
            cleanupCurl();
            return;
        }
        m_isDone = false;
    }
    virtual ~CURLResult() {
        int count = 0;
        while (!isDone() && count < 1000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            count++;
        }
        cleanupCurl();
    }
    virtual bool isError() override { return m_curl == nullptr || m_curlm == nullptr; }
    virtual bool isDone() override {
        if (!isError()) {
            if (m_isDone) {
                return true;
            }
            int handleCount;
            CURLMcode mstatus = curl_multi_perform(m_curlm, &handleCount);
            if (mstatus != CURLM_OK) {
                cleanupCurl();
                return true;
            }

            if (handleCount == 0) {
                int messagesLeft = 0;
                CURLMsg* msg = curl_multi_info_read(m_curlm, &messagesLeft);
                if (msg && msg->msg == CURLMSG_DONE) {
                    char* ct = nullptr;
                    if (CURLE_OK == curl_easy_getinfo(m_curl, CURLINFO_CONTENT_TYPE, &ct)) {
                        if (ct != nullptr) {
                            m_contentType = ct;
                        }
                    }
                    m_isDone = true;
                }
            }
            return m_isDone;
        }
        return true;
    }

    void cleanupCurl() {
        if (m_curlm) {
            if (m_curl) {
                curl_multi_remove_handle(m_curlm, m_curl);
            }
            curl_multi_cleanup(m_curlm);
            m_curlm = nullptr;
        }
        if (m_curl) {
            curl_easy_cleanup(m_curl);
            m_curl = nullptr;
        }
    }

private:
    CURL* m_curl;
    CURLM* m_curlm;
    bool m_isDone;
};

URLCommand::URLCommand() :
    Command("URL") {
    args.push_back(CommandArg("name", "string", "URL"));
    args.push_back(CommandArg("method", "string", "Method").setContentList({ "GET", "POST" }).setDefaultValue("GET"));
    args.push_back(CommandArg("data", "string", "Post Data"));
}
std::unique_ptr<Command::Result> URLCommand::run(const std::vector<std::string>& args) {
    return std::make_unique<CURLResult>(args);
}

#ifdef HAS_GSTREAMER

// Common data for tracking running command media
std::mutex runningMediaLock;
std::map<std::string, MediaOutputBase*> runningCommandMedia;

static void RemoveRunningMedia(const std::string& filename, MediaOutputBase* self) {
    intptr_t i = (intptr_t)self;
    MediaOutputBase* dt = self;
    std::string fn = filename;

    // Trigger MEDIA_STOPPED event
    std::map<std::string, std::string> keywords;
    keywords["MEDIA_NAME"] = fn;
    if (CommandManager::INSTANCE.HasPreset("MEDIA_STOPPED")) {
        CommandManager::INSTANCE.TriggerPreset("MEDIA_STOPPED", keywords);
    }

    Timers::INSTANCE.addTimer(std::to_string(i), GetTimeMS() + 1, [dt, fn]() {
        runningMediaLock.lock();
        auto it = runningCommandMedia.find(fn);
        if (it != runningCommandMedia.end() && it->second == dt) {
            runningCommandMedia.erase(it);
            LogDebug(VB_COMMAND, "Removed cached PlayData for file: \"%s\"\n", fn.c_str());
        }
        runningMediaLock.unlock();
        delete dt;
    });
}

static std::string RegisterRunningMedia(const std::string& file, MediaOutputBase* media) {
    std::string filename = file;
    runningMediaLock.lock();
    while (runningCommandMedia.find(filename) != runningCommandMedia.end()) {
        filename += "_";
    }
    runningCommandMedia[filename] = media;
    runningMediaLock.unlock();
    return filename;
}

#ifdef HAS_GSTREAMER
class GStreamerPlayData : public GStreamerOutput {
public:
    GStreamerPlayData(const std::string& file, int l, int vol, int slot = 1);
    virtual ~GStreamerPlayData();
    virtual void Stopped() override;
    std::string filename;
    int volumeAdjust = 0;
    int streamSlot = 1;
    MediaOutputStatus status;
};

GStreamerPlayData::GStreamerPlayData(const std::string& file, int l, int vol, int slot) :
    GStreamerOutput(file,
                    (slot > 1 && slot <= StreamSlotManager::MAX_SLOTS)
                        ? StreamSlotManager::Instance().GetStatus(slot)
                        : &status,
                    "", slot),
    filename(file),
    volumeAdjust(vol),
    streamSlot(slot) {
    SetLoopCount(l > 0 ? l - 1 : 0);
    SetVolumeAdjustment(vol);
    filename = RegisterRunningMedia(file, this);
}

GStreamerPlayData::~GStreamerPlayData() {
}

void GStreamerPlayData::Stopped() {
    GStreamerOutput::Stopped();
    RemoveRunningMedia(filename, this);
}
#endif

// Runtime backend selection for "Play Media" command
static bool UseGStreamerForPlayMedia() {
#ifdef HAS_GSTREAMER
    // GStreamer is the sole media player for all backends
    return true;
#endif
    return false;
}

PlayMediaCommand::PlayMediaCommand() :
    Command("Play Media", "Plays the media in the background") {
    args.push_back(CommandArg("media", "string", "Media").setContentListUrl("api/media"));
    args.push_back(CommandArg("loop", "int", "Loop Count").setDefaultValue(std::string("1")).setRange(1, 100));
    args.push_back(CommandArg("volume", "int", "Volume Adjust").setDefaultValue(std::string("0")).setRange(-100, 100));
    args.push_back(CommandArg("slot", "int", "Stream Slot").setDefaultValue(std::string("1")).setRange(1, StreamSlotManager::MAX_SLOTS));
}
std::unique_ptr<Command::Result> PlayMediaCommand::run(const std::vector<std::string>& args) {
    int loop = args.size() > 1 ? std::atoi(args[1].c_str()) : 1;
    int volAdjust = 0;
    if (args.size() > 2) {
        volAdjust = std::atoi(args[2].c_str());
    }
    int slot = 1;
    if (args.size() > 3) {
        slot = std::atoi(args[3].c_str());
        if (slot < 1 || slot > StreamSlotManager::MAX_SLOTS) slot = 1;
    }
    if (loop < 1) {
        loop = 1;
    }

    MediaOutputBase* out = nullptr;
#ifdef HAS_GSTREAMER
    if (UseGStreamerForPlayMedia()) {
        out = new GStreamerPlayData(args[0], loop, volAdjust, slot);
        LogInfo(VB_COMMAND, "Play Media using GStreamer backend for: %s (slot %d)\n", args[0].c_str(), slot);
    }
#endif

    if (!out) {
        return std::make_unique<Command::ErrorResult>("No media backend available");
    }

    if (out->Start()) {
        std::map<std::string, std::string> keywords;
        keywords["MEDIA_NAME"] = args[0];
        keywords["STREAM_SLOT"] = std::to_string(slot);
        if (CommandManager::INSTANCE.HasPreset("MEDIA_STARTED")) {
            CommandManager::INSTANCE.TriggerPreset("MEDIA_STARTED", keywords);
        }
    }

    return std::make_unique<Command::Result>("Playing");
}

StopMediaCommand::StopMediaCommand() :
    Command("Stop Media", "Stops the media in the background started via a command") {
    args.push_back(CommandArg("media", "string", "Media").setContentListUrl("api/media"));
}
std::unique_ptr<Command::Result> StopMediaCommand::run(const std::vector<std::string>& args) {
    if (args.size() == 0) {
        return std::make_unique<Command::Result>("Invalid Number of Arguments");
    }
    std::string rc = "Media Not Running";
    runningMediaLock.lock();
    MediaOutputBase* media_ptr = nullptr;
    try {
        media_ptr = runningCommandMedia.at(args[0]);
    } catch (const std::out_of_range& oor) {
        LogInfo(VB_COMMAND, "Media: \"%s\" is not running so can't stop via command\n", args[0].c_str());
        media_ptr = nullptr;
    }
    if (media_ptr) {
        media_ptr->Stop();
        rc = "Stopped";
    }
    runningMediaLock.unlock();

    return std::make_unique<Command::Result>(rc);
}

StopAllMediaCommand::StopAllMediaCommand() :
    Command("Stop All Media", "Stops all running media that was created via a Command") {
}
std::unique_ptr<Command::Result> StopAllMediaCommand::run(const std::vector<std::string>& args) {
    runningMediaLock.lock();
    for (const auto& item : runningCommandMedia) {
        LogDebug(VB_COMMAND, "Stopping: \"%s\"\n", item.first.c_str());
        if (item.second) {
            item.second->Stop();
        }
    }
    runningMediaLock.unlock();
    return std::make_unique<Command::Result>("Stopped");
}

StopMediaSlotCommand::StopMediaSlotCommand() :
    Command("Stop Media Slot", "Stop playback on a specific stream slot (1-5)") {
    args.push_back(CommandArg("slot", "int", "Stream Slot").setRange(1, StreamSlotManager::MAX_SLOTS).setDefaultValue("1"));
}
std::unique_ptr<Command::Result> StopMediaSlotCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("No slot specified");
    }
    int slot = std::atoi(args[0].c_str());
    if (slot < 1 || slot > StreamSlotManager::MAX_SLOTS) {
        return std::make_unique<Command::ErrorResult>("Invalid slot number (1-" + std::to_string(StreamSlotManager::MAX_SLOTS) + ")");
    }

    // Try command-started media first (matches by slot)
    std::string stoppedFile;
    runningMediaLock.lock();
    for (const auto& item : runningCommandMedia) {
#ifdef HAS_GSTREAMER
        GStreamerPlayData* gpd = dynamic_cast<GStreamerPlayData*>(item.second);
        if (gpd && gpd->streamSlot == slot) {
            gpd->Stop();
            stoppedFile = item.first;
            break;
        }
#endif
    }
    runningMediaLock.unlock();

    if (!stoppedFile.empty()) {
        LogInfo(VB_COMMAND, "Stop Media Slot %d: stopped command media '%s'\n", slot, stoppedFile.c_str());
        return std::make_unique<Command::Result>("Stopped slot " + std::to_string(slot));
    }

    // Fall back to StreamSlotManager (catches playlist-started media)
    GStreamerOutput* output = StreamSlotManager::Instance().GetActiveOutput(slot);
    if (output) {
        output->Stop();
        LogInfo(VB_COMMAND, "Stop Media Slot %d: stopped active output\n", slot);
        return std::make_unique<Command::Result>("Stopped slot " + std::to_string(slot));
    }

    return std::make_unique<Command::Result>("Slot " + std::to_string(slot) + " not playing");
}

SetSlotVolumeCommand::SetSlotVolumeCommand() :
    Command("Set Slot Volume", "Sets the volume on a specific stream slot (1-5)") {
    args.push_back(CommandArg("slot", "int", "Stream Slot").setRange(1, StreamSlotManager::MAX_SLOTS).setDefaultValue("1"));
    args.push_back(CommandArg("volume", "int", "Volume").setRange(0, 100).setDefaultValue("70"));
}
std::unique_ptr<Command::Result> SetSlotVolumeCommand::run(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        return std::make_unique<Command::ErrorResult>("Requires slot and volume arguments");
    }
    int slot = std::atoi(args[0].c_str());
    int volume = std::atoi(args[1].c_str());
    if (slot < 1 || slot > StreamSlotManager::MAX_SLOTS) {
        return std::make_unique<Command::ErrorResult>("Invalid slot number (1-" + std::to_string(StreamSlotManager::MAX_SLOTS) + ")");
    }
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    if (StreamSlotManager::Instance().SetSlotVolume(slot, volume)) {
        return std::make_unique<Command::Result>("Volume set on slot " + std::to_string(slot));
    }
    return std::make_unique<Command::ErrorResult>("Slot " + std::to_string(slot) + " not active");
}

MediaSlotStatusCommand::MediaSlotStatusCommand() :
    Command("Media Slot Status", "Returns the current playback status of all stream slots") {
}
std::unique_ptr<Command::Result> MediaSlotStatusCommand::run(const std::vector<std::string>& args) {
    Json::Value status = StreamSlotManager::Instance().GetAllSlotsStatus();
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string result = Json::writeString(builder, status);
    return std::make_unique<Command::Result>(result);
}
#endif

#ifdef HAS_AES67_GSTREAMER
#include "mediaoutput/AES67Manager.h"

AES67ApplyCommand::AES67ApplyCommand() :
    Command("AES67 Apply", "Apply AES67 audio-over-IP configuration") {
}
std::unique_ptr<Command::Result> AES67ApplyCommand::run(const std::vector<std::string>& args) {
    if (AES67Manager::INSTANCE.ApplyConfig()) {
        return std::make_unique<Command::Result>("AES67 config applied");
    }
    return std::make_unique<Command::ErrorResult>("AES67 apply failed");
}

AES67CleanupCommand::AES67CleanupCommand() :
    Command("AES67 Cleanup", "Stop all AES67 pipelines and clean up") {
}
std::unique_ptr<Command::Result> AES67CleanupCommand::run(const std::vector<std::string>& args) {
    AES67Manager::INSTANCE.Cleanup();
    return std::make_unique<Command::Result>("AES67 cleaned up");
}

AES67TestCommand::AES67TestCommand() :
    Command("AES67 Test", "Run AES67 subsystem self-test") {
}
std::unique_ptr<Command::Result> AES67TestCommand::run(const std::vector<std::string>& args) {
    auto tests = AES67Manager::INSTANCE.RunSelfTest();
    int passed = 0, failed = 0;
    std::string report;
    for (const auto& t : tests) {
        report += (t.passed ? "PASS" : "FAIL");
        report += ": " + t.testName + " - " + t.message + "\n";
        if (t.passed) passed++; else failed++;
    }
    report += "\nTotal: " + std::to_string(tests.size()) +
              " | Passed: " + std::to_string(passed) +
              " | Failed: " + std::to_string(failed);
    if (failed == 0) {
        return std::make_unique<Command::Result>(report);
    }
    return std::make_unique<Command::ErrorResult>(report);
}

ApplyRoutingPresetCommand::ApplyRoutingPresetCommand() :
    Command("Pipewire Apply Routing Preset", "Live-apply a saved PipeWire routing preset without stopping playback") {
    args.push_back(CommandArg("Preset", "string", "Routing Preset Name")
                       .setContentListUrl("api/pipewire/audio/routing/presets/names"));
}
std::unique_ptr<Command::Result> ApplyRoutingPresetCommand::run(const std::vector<std::string>& args) {
    if (args.empty() || args[0].empty()) {
        return std::make_unique<Command::ErrorResult>("No preset name specified");
    }
    // Build the API call — POST to the live-apply endpoint with the preset name
    std::string url = "http://localhost/api/pipewire/audio/routing/presets/live-apply";
    std::string postData = "{\"name\":\"" + args[0] + "\"}";
    std::vector<std::string> curlArgs = { url, "POST", postData };
    return std::make_unique<CURLResult>(curlArgs);
}
#endif

#ifdef HAS_OPUS_RTP_GSTREAMER
#include "mediaoutput/OpusRTPManager.h"

OpusRTPApplyCommand::OpusRTPApplyCommand() :
    Command("Opus RTP Apply", "Apply Opus RTP audio streaming configuration") {
}
std::unique_ptr<Command::Result> OpusRTPApplyCommand::run(const std::vector<std::string>& args) {
    if (OpusRTPManager::INSTANCE.ApplyConfig()) {
        return std::make_unique<Command::Result>("Opus RTP config applied");
    }
    return std::make_unique<Command::ErrorResult>("Opus RTP apply failed");
}

OpusRTPCleanupCommand::OpusRTPCleanupCommand() :
    Command("Opus RTP Cleanup", "Stop all Opus RTP pipelines and clean up") {
}
std::unique_ptr<Command::Result> OpusRTPCleanupCommand::run(const std::vector<std::string>& args) {
    OpusRTPManager::INSTANCE.Cleanup();
    return std::make_unique<Command::Result>("Opus RTP cleaned up");
}
#endif
