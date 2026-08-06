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

#include "fpp-json.h"

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include <filesystem>
#include <thread>
using namespace std::filesystem;

#include <sys/wait.h>

#include "../Events.h"
#include "../MultiSync.h"
#include "../commands/Commands.h"
#include "../Plugins.h"
#include "../channeloutput/ChannelOutputSetup.h"
#include "../common.h"
#include "../log.h"
#include "../mediadetails.h"

#include "Playlist.h"
#include "PlaylistEntryMedia.h"

int PlaylistEntryMedia::m_openStartDelay = -1;

/*
 *
 */
PlaylistEntryMedia::PlaylistEntryMedia(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_status(0),
    m_mediaOutput(NULL),
    m_videoOutput("--Default--"),
    m_streamSlot(1),
    m_slotStatus(&mediaOutputStatus),
    m_openTime(0),
    m_fileMode("single"),
    m_duration(0),
    m_pausedTime(-1) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::PlaylistEntryMedia()\n");
    if (m_openStartDelay == -1) {
        m_openStartDelay = getSettingInt("openStartDelay");
    }
    m_type = "media";
    m_fileSeed = (unsigned int)time(NULL);
}

/*
 *
 */
PlaylistEntryMedia::~PlaylistEntryMedia() {
    // Playlist::Cleanup() deletes entries without calling Stop() first — if
    // media was still open the output object (and its live GStreamer
    // pipeline) leaked.  Swap under the lock, tear down outside it, same
    // discipline as CloseMediaOutput().
    MediaOutputBase* out = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mediaOutputLock);
        out = m_mediaOutput;
        m_mediaOutput = nullptr;
    }
    delete out;
}

/*
 *
 */
int PlaylistEntryMedia::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::Init()\n");

    if (config.isMember("fileMode")) {
        m_fileMode = config["fileMode"].asString();
    }

    if (m_fileMode == "single") {
        if (config.isMember("mediaName")) {
            m_mediaFilename = config["mediaName"].asString();
        } else {
            LogErr(VB_PLAYLIST, "Missing mediaName entry\n");
            return 0;
        }
    } else {
        if (config.isMember("mediaPrefix")) {
            m_mediaPrefix = config["mediaPrefix"].asString();
        }
        if (GetFileList()) {
            m_mediaFilename = GetNextRandomFile();
        } else {
            LogErr(VB_PLAYLIST, "Error, no files found when trying to play random file in %s mode\n",
                   m_fileMode.c_str());
            WarningHolder::AddWarningTimeout(60, 30, "No media files found to play in '" + m_fileMode + "' mode");
            return 0;
        }
    }

    if (config.isMember("videoOut")) {
        m_videoOutput = config["videoOut"].asString();
    }

    // Companion streams to run alongside this entry (see ExtraMedia).
    m_extraMedia.clear();
    if (config.isMember("extraMedia") && config["extraMedia"].isArray()) {
        for (const auto& e : config["extraMedia"]) {
            ExtraMedia em;
            em.mediaName = e.get("mediaName", "").asString();
            if (em.mediaName.empty())
                continue;
            em.slot = e.get("slot", 2).asInt();
            if (em.slot < 2 || em.slot > StreamSlotManager::MAX_SLOTS) {
                LogWarn(VB_PLAYLIST, "Media entry: extraMedia '%s' has slot %d; must be 2-%d (slot 1 is this entry's own media), skipping\n",
                        em.mediaName.c_str(), em.slot, StreamSlotManager::MAX_SLOTS);
                continue;
            }
            em.videoOut = e.get("videoOut", "").asString();
            em.sync = e.get("sync", false).asBool();
            m_extraMedia.push_back(em);
        }
    }
    // Flat single-companion form, which is what the playlist editor writes --
    // the editor rebuilds an entry from the fields it knows about, so a
    // hand-authored "extraMedia" array survives fppd but not a round trip
    // through the UI.  One companion covers both things this was asked for (a
    // second audio feed on another device, a second video on another HDMI), so
    // that case gets real fields and the array stays the advanced escape hatch.
    if (config.isMember("extraMediaName") && !config["extraMediaName"].asString().empty()) {
        ExtraMedia em;
        em.mediaName = config["extraMediaName"].asString();
        // The slot is the output route -- which audio device a companion lands
        // on is set per slot in the PipeWire audio groups, not per entry.  The
        // editor writes this as a string; hand-authored playlists may use an
        // int, and asInt() on a string throws, so handle both.
        em.slot = 2;
        if (config.isMember("extraMediaSlot")) {
            const Json::Value& sv = config["extraMediaSlot"];
            int s = sv.isString() ? atoi(sv.asString().c_str()) : (sv.isIntegral() ? sv.asInt() : 0);
            if (s >= 2 && s <= StreamSlotManager::MAX_SLOTS) {
                em.slot = s;
            } else if (s != 0) {
                LogWarn(VB_PLAYLIST, "Media entry: extraMediaSlot %d out of range, using slot 2\n", s);
            }
        }
        em.videoOut = config.get("extraMediaOut", "").asString();
        em.sync = true;  // it belongs to the show; sync is only a repair path
        m_extraMedia.push_back(em);
    }

    // Stream slot (1-5), default 1.  Slot > 1 plays on a separate PipeWire stream.
    if (config.isMember("streamSlot")) {
        m_streamSlot = config["streamSlot"].asInt();
        if (m_streamSlot < 1) m_streamSlot = 1;
        if (m_streamSlot > StreamSlotManager::MAX_SLOTS) m_streamSlot = StreamSlotManager::MAX_SLOTS;
    }
    m_slotStatus = StreamSlotManager::Instance().GetStatus(m_streamSlot);

    m_pausedTime = -1;
    return PlaylistEntryBase::Init(config);
}

int PlaylistEntryMedia::PreparePlay() {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::PreparePlay()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }
    m_slotStatus->minutesTotal = 0;
    m_slotStatus->secondsTotal = 0;
    m_slotStatus->secondsElapsed = 0;
    m_slotStatus->subSecondsElapsed = 0;

    if (m_fileMode != "single") {
        if (m_files.size()) {
            m_mediaFilename = GetNextRandomFile();
        } else {
            if (GetFileList()) {
                m_mediaFilename = GetNextRandomFile();
            } else {
                LogErr(VB_PLAYLIST, "Error, no files found when trying to play random file in %s mode\n",
                       m_fileMode.c_str());
                WarningHolder::AddWarningTimeout(60, 30, "No media files found to play in '" + m_fileMode + "' mode");
                return 0;
            }
        }
    }
    if (!OpenMediaOutput()) {
        FinishPlay();
        return 0;
    }

    if (multiSync->isMultiSyncEnabled()) {
        multiSync->SendMediaOpenPacket(m_mediaFilename);
    }

    m_openTime = GetTimeMS();
    Events::Publish("playlist/media/status", m_mediaFilename);
    Events::Publish("playlist/media/title", MediaDetails::INSTANCE.title);
    Events::Publish("playlist/media/artist", MediaDetails::INSTANCE.artist);
    return 1;
}

/*
 *
 */
int PlaylistEntryMedia::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::StartPlaying()\n");

    if (multiSync->isMultiSyncEnabled() && m_openTime) {
        long long st = GetTimeMS() - m_openTime;
        if (st < m_openStartDelay) {
            std::this_thread::sleep_for(std::chrono::milliseconds(m_openStartDelay - st));
        }
    }

    if (m_mediaOutput == nullptr) {
        if (PreparePlay() == 0) {
            return 0;
        }
    }
    if (m_mediaOutput == nullptr) {
        return 0;
    }

    float f = m_slotStatus->mediaSeconds * 1000;
    if (m_duration == 0) {
        float f = m_slotStatus->minutesTotal * 60 + m_slotStatus->secondsTotal;
        f *= 1000;
        m_duration = f;
    }
    if (f > m_duration) {
        m_duration = f;
    }
    std::unique_lock<std::mutex> lock(m_mediaOutputLock);

    if (m_mediaOutput->getMediaOffsetMS() <= 0 && multiSync->isMultiSyncEnabled()) {
        multiSync->SendMediaSyncStartPacket(m_mediaFilename);
    }

    ResetChannelOutputFrameNumber();
    if (!m_mediaOutput->Start()) {
        LogErr(VB_MEDIAOUT, "Could not start media %s\n", m_mediaOutput->m_mediaFilename.c_str());
        WarningHolder::AddWarningTimeout(60, 30, "Could not start media " + m_mediaOutput->m_mediaFilename);
        // Swap-unlock-delete: the delete runs the full GStreamer teardown
        // (sleeps plus a state change that can block indefinitely on a
        // wedged decoder) — doing that under m_mediaOutputLock wedges any
        // thread touching this entry, including HTTP status threads that
        // are holding m_playlistMutex.  Same rationale as CloseMediaOutput.
        MediaOutputBase* failed = m_mediaOutput;
        m_mediaOutput = 0;
        lock.unlock();
        delete failed;
        FinishPlay();
        return 0;
    }

    lock.unlock();
    m_startTime = GetTimeMS();
    StartExtraMedia();
    return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryMedia::Process(void) {
    std::unique_lock<std::mutex> lock(m_mediaOutputLock);

    if (m_mediaOutput) {
        if (m_mediaOutput->getMediaOffsetMS() > 0 && multiSync->isMultiSyncEnabled()) {
            long long ct = GetTimeMS() - m_startTime;
            if (ct > (m_mediaOutput->getMediaOffsetMS() - 20)) {
                multiSync->SendMediaSyncStartPacket(m_mediaFilename);
            }
        }

        m_mediaOutput->Process();
        if (!m_mediaOutput->IsPlaying()) {
            FinishPlay();
            lock.unlock();
            // Companions are tied to this entry's lifetime, so they stop when
            // the media ends naturally too -- not only on an explicit Stop().
            StopExtraMedia();
            CloseMediaOutput();
            lock.lock();
        }
    } else {
        FinishPlay();
    }

    lock.unlock();
    return PlaylistEntryBase::Process();
}

bool PlaylistEntryMedia::HasExtraAtEnd() {
    // m_mediaOutput can be deleted concurrently by CloseMediaOutput() (e.g.
    // the HTTP status thread calling this while the playlist thread tears
    // down media), so the pointer must not be dereferenced without the lock.
    std::lock_guard<std::mutex> lock(m_mediaOutputLock);
    bool result = false;
    if (m_mediaOutput) {
        result = m_mediaOutput->getMediaOffsetMS() < 0;
    }
    return result;
}
int32_t PlaylistEntryMedia::GetMediaOffsetMS() {
    std::lock_guard<std::mutex> lock(m_mediaOutputLock);
    int32_t result = 0;
    if (m_mediaOutput) {
        result = m_mediaOutput->getMediaOffsetMS();
    }
    return result;
}

uint64_t PlaylistEntryMedia::GetLengthInMS() {
    if (m_duration == 0) {
        MediaDetails details;
        details.ParseMedia(m_mediaFilename.c_str());
        m_duration = details.lengthMS;
    }
    // Always try to refine from the running media output status.
    // Some backends (e.g. GStreamer) may not have the duration available
    // immediately at Start(), so minutesTotal/secondsTotal get populated
    // asynchronously.  Re-checking here prevents GetElapsedMS()'s
    // "if (f > m_duration)" guard from permanently caching a wrong value
    // (i.e., the elapsed time) before the real duration was known.
    if (m_slotStatus->minutesTotal > 0 || m_slotStatus->secondsTotal > 0) {
        uint64_t d = ((uint64_t)m_slotStatus->minutesTotal * 60 + m_slotStatus->secondsTotal) * 1000;
        if (d > m_duration) {
            m_duration = d;
        }
    }
    return m_duration;
}
uint64_t PlaylistEntryMedia::GetElapsedMS() {
    if (m_mediaOutput) {
        float f = m_slotStatus->secondsElapsed * 1000;
        f += m_slotStatus->subSecondsElapsed * 10; // subSec is in 1/100th second
        if (IsPaused()) {
            f = m_pausedStatus.secondsElapsed * 1000;
            f += m_pausedStatus.subSecondsElapsed * 10; // subSec is in 1/100th second
        }
        if (m_duration == 0) {
            float f = m_slotStatus->minutesTotal * 60 + m_slotStatus->secondsTotal;
            f *= 1000;
            m_duration = f;
        }
        if (f > m_duration) {
            m_duration = f;
        }
        return f;
    }
    return 0;
}

/*
 *
 */
// Start/stop are delegated to the existing Play Media / Stop Media Slot
// commands rather than reimplementing slot setup here: those already handle
// registration, teardown and the sync flag, and reusing them keeps one code
// path for "play something on a slot".
void PlaylistEntryMedia::StartExtraMedia(int startMS) {
    if (m_extraMedia.empty() || m_extraMediaStarted)
        return;
    m_extraMediaStarted = true;
    for (const auto& em : m_extraMedia) {
        std::vector<std::string> args = {
            em.mediaName,
            "1", // play once; it is stopped with the entry
            "0", // no volume adjustment
            std::to_string(em.slot),
            em.videoOut,
            em.sync ? "true" : "false"
        };
        LogInfo(VB_PLAYLIST, "Media entry: starting extra media '%s' on slot %d (videoOut='%s'%s)\n",
                em.mediaName.c_str(), em.slot, em.videoOut.c_str(),
                em.sync ? ", synced" : "");
        CommandManager::INSTANCE.run("Play Media", args);

        // Resuming mid-entry: "Play Media" always starts at zero, so put the
        // companion back on the entry's timeline explicitly.  Waiting for the
        // sync repair path is not enough -- it only acts past MAX_DRIFT_SEC,
        // and only for companions that opted into sync at all.
        if (startMS > 0) {
            StreamSlotManager::Instance().SeekSlot(em.slot, startMS / 1000.0f);
        }
    }
}

void PlaylistEntryMedia::StopExtraMedia(void) {
    if (!m_extraMediaStarted)
        return;
    m_extraMediaStarted = false;
    for (const auto& em : m_extraMedia) {
        std::vector<std::string> args = { std::to_string(em.slot) };
        LogDebug(VB_COMMAND, "Media entry: stopping extra media '%s', running \"Stop Media Slot\" on slot %d\n", em.mediaName.c_str(), em.slot);
        CommandManager::INSTANCE.run("Stop Media Slot", args);
    }
}

int PlaylistEntryMedia::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::Stop()\n");

    StopExtraMedia();
    CloseMediaOutput();

    Events::Publish("playlist/media/status", "");
    Events::Publish("playlist/media/title", "");
    Events::Publish("playlist/media/artist", "");

    m_slotStatus->minutesTotal = 0;
    m_slotStatus->secondsTotal = 0;
    m_slotStatus->secondsElapsed = 0;
    m_slotStatus->subSecondsElapsed = 0;

    return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryMedia::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Media Filename: %s\n", m_mediaFilename.c_str());
}

/*
 *
 */
int PlaylistEntryMedia::OpenMediaOutput(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::OpenMediaOutput() - Starting\n");

    {
        std::unique_lock<std::mutex> lock(m_mediaOutputLock);
        if (m_mediaOutput) {
            lock.unlock();
            CloseMediaOutput();
        }
    }

    std::string tmpFile = m_mediaFilename;
    std::size_t found = tmpFile.find_last_of(".");

    if (found == std::string::npos) {
        LogWarn(VB_MEDIAOUT, "Unable to determine extension of media file %s\n",
                m_mediaFilename.c_str());
        return 0;
    }

    std::string ext = toLowerCopy(tmpFile.substr(found + 1));

    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia - Starting %s\n", tmpFile.c_str());

    // m_mediaOutputLock must NOT be held here: mediaCallback() re-enters this
    // entry on the same thread via Playlist::GetInfo() -> GetCurrentEntry() ->
    // GetConfig(), which takes m_mediaOutputLock.  Holding it across the
    // callback self-deadlocks the playlist thread (fppd hang on every media
    // start).  Nothing below touches m_mediaOutput until the assignment at
    // the end, so the pointer needs no protection in between — only the
    // final store is done under the lock.
    MediaDetails::INSTANCE.ParseMedia(m_mediaFilename.c_str());
    PluginManager::INSTANCE.mediaCallback(m_parentPlaylist->GetInfo(), MediaDetails::INSTANCE);

    std::string vOut = m_videoOutput;
    if (vOut == "--Default--") {
        vOut = getSetting("VideoOutput");
    }
    if (vOut == "") {
        if (FileExists("/sys/class/drm/card0-HDMI-A-1/status") || FileExists("/sys/class/drm/card1-HDMI-A-1/status")) {
            vOut = "--HDMI--";
        } else {
            vOut = "--Disabled--";
        }
    }

    MediaOutputBase* out = CreateMediaOutput(tmpFile, vOut, m_streamSlot);

    if (!out) {
        LogDebug(VB_MEDIAOUT, "No Media Output handler for %s\n", tmpFile.c_str());
        return 0;
    }

    {
        std::lock_guard<std::mutex> lock(m_mediaOutputLock);
        m_mediaOutput = out;
    }

    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::OpenMediaOutput() - Complete\n");

    return 1;
}

/*
 *
 */
int PlaylistEntryMedia::CloseMediaOutput(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::CloseMediaOutput()\n");

    m_slotStatus->status = MEDIAOUTPUTSTATUS_IDLE;

    std::unique_lock<std::mutex> lock(m_mediaOutputLock);
    if (!m_mediaOutput) {
        return 0;
    }

    // Swap the member into a local and null it out while still holding the
    // lock, then release the lock before the blocking teardown below.
    // ~GStreamerOutput() (via delete) runs GStreamerOutput::Stop(), which
    // has 250ms + 150ms sleeps and a gst_element_set_state(NULL) that can
    // block indefinitely if the Pi's V4L2 decoder wedges (issue #2695).
    // Holding m_mediaOutputLock across that would stall any other thread
    // touching this entry (HTTP status thread, Process(), etc.) for the
    // same duration.
    MediaOutputBase* out = m_mediaOutput;
    m_mediaOutput = NULL;
    lock.unlock();

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendMediaSyncStopPacket(m_mediaFilename);

    delete out;

    return 1;
}

/*
 *
 */
int PlaylistEntryMedia::GetFileList(void) {
    std::string dir;

    m_files.clear();

    if (m_fileMode == "randomVideo")
        dir = FPP_DIR_VIDEO("");
    else if (m_fileMode == "randomAudio")
        dir = FPP_DIR_MUSIC("");

    for (auto& cp : recursive_directory_iterator(dir)) {
        std::string entry = cp.path().string();

        if (!m_mediaPrefix.empty()) {
            if (cp.is_regular_file() && cp.path().stem().string().find(m_mediaPrefix) == 0) {
                LogDebug(VB_PLAYLIST, "match: %s\n", cp.path().c_str());
                m_files.push_back(entry);
            }
        } else {
            m_files.push_back(entry);
        }
    }

    LogDebug(VB_PLAYLIST, "%d files in %s directory\n", m_files.size(), dir.c_str());

    return m_files.size();
}

/*
 *
 */
std::string PlaylistEntryMedia::GetNextRandomFile(void) {
    std::string filename;

    if (!m_files.size())
        GetFileList();

    if (!m_files.size()) {
        LogWarn(VB_PLAYLIST, "No files found in GetNextRandomFile()\n");
        return filename;
    }

    int i = rand_r(&m_fileSeed) % m_files.size();
    filename = m_files[i];
    m_files.erase(m_files.begin() + i);

    LogDebug(VB_PLAYLIST, "GetNextRandomFile() = %s\n", filename.c_str());

    return filename;
}

/*
 *
 */
Json::Value PlaylistEntryMedia::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();
    MediaOutputStatus status = IsPaused() ? m_pausedStatus : *m_slotStatus;
    result["mediaFilename"] = m_mediaFilename;
    result["streamSlot"] = m_streamSlot;

    // Only need the pointer's non-null-ness here (the actual fields come
    // from the already-copied "status" above), but the pointer itself can
    // be deleted concurrently by CloseMediaOutput(), so check it under the
    // lock rather than dereferencing/testing it unguarded.
    bool hasMediaOutput;
    {
        std::lock_guard<std::mutex> lock(m_mediaOutputLock);
        hasMediaOutput = (m_mediaOutput != nullptr);
    }

    if (hasMediaOutput) {
        result["status"] = status.status;
        result["secondsElapsed"] = status.secondsElapsed;
        result["subSecondsElapsed"] = status.subSecondsElapsed;
        result["secondsRemaining"] = status.secondsRemaining;
        result["subSecondsRemaining"] = status.subSecondsRemaining;
        result["minutesTotal"] = status.minutesTotal;
        result["secondsTotal"] = status.secondsTotal;
        result["mediaSeconds"] = status.mediaSeconds;
        result["mediaSeconds"] = status.mediaSeconds;
        result["millisecondsElapsed"] = status.secondsElapsed * 1000 + status.subSecondsElapsed * 10;
    }

    return result;
}

Json::Value PlaylistEntryMedia::GetMqttStatus(void) {
    Json::Value result = PlaylistEntryBase::GetMqttStatus();
    MediaOutputStatus status = IsPaused() ? m_pausedStatus : *m_slotStatus;
    result["secondsElapsed"] = status.secondsElapsed;
    result["millisecondsElapsed"] = status.secondsElapsed * 1000 + status.subSecondsElapsed * 10;
    result["secondsRemaining"] = status.secondsRemaining;
    result["secondsTotal"] = status.minutesTotal * 60 + status.secondsTotal;
    result["mediaName"] = m_mediaFilename;
    result["mediaTitle"] = MediaDetails::INSTANCE.title;
    result["mediaArtist"] = MediaDetails::INSTANCE.artist;

    return result;
}

void PlaylistEntryMedia::Pause() {
    m_pausedStatus = *m_slotStatus;
    m_pausedTime = GetElapsedMS();
    // Pausing a media entry tears the output down rather than pausing the
    // pipeline, so companions are stopped and rebuilt the same way; Resume()
    // restarts them at m_pausedTime.
    StopExtraMedia();
    CloseMediaOutput();
    Events::Publish("playlist/media/status", "");
    Events::Publish("playlist/media/title", "");
    Events::Publish("playlist/media/artist", "");
}
bool PlaylistEntryMedia::IsPaused() {
    return m_pausedTime >= 0;
}
void PlaylistEntryMedia::Resume() {
    if (m_pausedTime >= 0) {
        // Captured before the block below clears m_pausedTime, and used to put
        // the companions back at the same offset as this entry's own media.
        int resumeMS = m_pausedTime;
        // PreparePlay()'s return is intentionally not treated as fatal here;
        // if it failed, m_mediaOutput will be left null and is handled below
        // instead of blindly dereferencing it.
        PreparePlay();

        // If Start() fails, the failed output is swapped out under the lock
        // and deleted after it's released — its destructor runs the blocking
        // GStreamer teardown (see CloseMediaOutput rationale).
        MediaOutputBase* failed = nullptr;
        {
        std::lock_guard<std::mutex> lock(m_mediaOutputLock);

        if (!m_mediaOutput) {
            // PreparePlay() couldn't (re)create the media output (e.g. file
            // missing/unsupported). Nothing to resume -- still clear the
            // paused state so the entry doesn't stay wedged reporting
            // itself as paused forever.
            LogErr(VB_MEDIAOUT, "Could not resume media %s, no media output available\n", m_mediaFilename.c_str());
            m_pausedTime = -1;
            *m_slotStatus = m_pausedStatus;
            return;
        }

        if (multiSync->isMultiSyncEnabled())
            multiSync->SendMediaSyncStartPacket(m_mediaFilename);

        if (!m_mediaOutput->Start(m_pausedTime)) {
            LogErr(VB_MEDIAOUT, "Could not start media %s\n", m_mediaOutput->m_mediaFilename.c_str());
            WarningHolder::AddWarningTimeout(60, 30, "Could not start media " + m_mediaOutput->m_mediaFilename);
            failed = m_mediaOutput;
            m_mediaOutput = nullptr;
        }

        // Whether Start() above succeeded or failed, the pause is over:
        // clear m_pausedTime so the entry doesn't stay wedged reporting
        // itself as paused, and restore the last known status snapshot.
        // The lock (RAII) is released exactly once here on every path,
        // when it goes out of scope.
        m_pausedTime = -1;
        *m_slotStatus = m_pausedStatus;
        }
        delete failed;

        // Outside the media output lock: this dispatches commands, and holding
        // the lock across that is how deadlocks get built.
        StartExtraMedia(resumeMS);
    }
}
