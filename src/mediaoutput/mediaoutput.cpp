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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <sstream>
#include <string>

#include "MultiSync.h"
#include "common.h"
#include "log.h"
#include "mediaoutput.h"
#include "commands/Commands.h"

#include "Plugins.h"
#include "Sequence.h"
#include "GStreamerOut.h"
#include "StreamSlotManager.h"
#include "mediadetails.h"
#include "settings.h"
#include "../config.h"

/////////////////////////////////////////////////////////////////////////////
MediaOutputBase* mediaOutput = 0;
float masterMediaPosition = 0.0;
std::mutex mediaOutputLock;

static bool firstOutCreate = true;

// AudioOutput is persisted as a stable ALSA card ID string (e.g. "S3",
// "bcm2835ALSA"); setupAudio() in boot/FPPINIT_Audio.cpp migrates legacy numeric
// values on boot. Resolve it to the current card number for amixer. Read
// defensively: an all-numeric value is treated as a legacy card index, otherwise
// it is matched against /proc/asound/cards by ID. Mirrors getAlsaCardNumForId()
// in boot/FPPINIT_Audio.cpp (fppd and fppinit are separate binaries and cannot
// share the static helper).
static int resolveAudioOutputCardNum() {
    std::string id = getSetting("AudioOutput");
    TrimWhiteSpace(id);
    if (id.empty()) {
        return 0;
    }
    if (id.find_first_not_of("0123456789") == std::string::npos) {
        try {
            return std::stoi(id);
        } catch (...) {
            return 0;
        }
    }
    std::istringstream iss(GetFileContents("/proc/asound/cards"));
    std::string line;
    while (std::getline(iss, line)) {
        auto b = line.find('[');
        auto e = line.find(']');
        if (b != std::string::npos && e != std::string::npos && e > b) {
            std::string cid = line.substr(b + 1, e - b - 1);
            TrimWhiteSpace(cid);
            if (cid == id) {
                std::string num = line.substr(0, b);
                TrimWhiteSpace(num);
                try {
                    return std::stoi(num);
                } catch (...) {}
            }
        }
    }
    return 0; // selected card not present -> default to card 0
}

MediaOutputStatus mediaOutputStatus = {
    MEDIAOUTPUTSTATUS_IDLE, // status
};

/*
 *
 */
void InitMediaOutput(void) {
#ifndef PLATFORM_OSX
    int vol = getSettingInt("volume", -1);
    if (vol < 0) {
        vol = 70;
    }
    setVolume(vol);
#endif
}

/*
 *
 */
void CleanupMediaOutput(void) {
    CloseMediaOutput();
}

#ifndef PLATFORM_OSX
static int volume = 70;
static int lastPipeWireVolume = -1;  // Track last volume to avoid redundant mute toggles
int getVolume() {
    return volume;
}
#else
int getVolume() {
    return MacOSGetVolume();
}
#endif

void setVolume(int vol) {
    char buffer[256];

    if (vol < 0)
        vol = 0;
    else if (vol > 100)
        vol = 100;

    std::string mixerDevice = getSetting("AudioMixerDevice");
    int audioOutput = resolveAudioOutputCardNum();
    std::string audio0Type = getSetting("AudioCard0Type");
    std::string mediaBackend = toLowerCopy(getSetting("MediaBackend"));

    bool usePipeWireBackend = (mediaBackend == "pipewire" || mediaBackend == "pipewire-simple");

#ifndef PLATFORM_OSX
    volume = vol;
    float fvol = volume;

    // Detect the PipeWire sink name for volume control.
    // Audio groups use a combine-stream named "fpp_group_*"; direct sinks
    // may have a specific name or be empty (default sink).
    std::string pipewireSink;
    if (usePipeWireBackend) {
        pipewireSink = getSetting("PipeWireSinkName");
    }

    // === ALSA hardware volume ===
    // For PipeWire backend (all modes): pin hardware at 100% so that the
    // PipeWire software volume (pactl, see below) is the sole attenuator.
    // This avoids double attenuation and matches WirePlumber's expectation
    // that hardware mixers are left at full output.
    //
    // For bcm2835 this is especially important: its hardware mixer can go
    // above 0 dBFS (+4 dB), while WirePlumber defaults to 0 dB.  Keeping
    // it at hardware max ensures the full dynamic range is available to the
    // PipeWire software volume stage.
    //
    // For ALSA mode (no PipeWire): set hardware volume directly to the FPP
    // percentage, exactly as legacy ALSA behaviour.
    if (usePipeWireBackend) {
        snprintf(buffer, sizeof(buffer), "amixer set -c %d '%s' -- 100%% >/dev/null 2>&1",
                 audioOutput, mixerDevice.c_str());
        LogDebug(VB_MEDIAOUT, "PipeWire mode: pinning ALSA hardware to max: %s \n", buffer);
        system(buffer);
    } else {
#ifdef PLATFORM_PI
        if (audioOutput == 0 && audio0Type == "bcm2") {
            fvol = volume;
            fvol /= 2;
            fvol += 50;
        }
#endif
        snprintf(buffer, sizeof(buffer), "amixer set -c %d '%s' -- %.2f%% >/dev/null 2>&1",
                 audioOutput, mixerDevice.c_str(), fvol);
        LogDebug(VB_SETTING, "Volume change: %d \n", volume);
        LogDebug(VB_MEDIAOUT, "Setting ALSA hardware volume: %s \n", buffer);
        system(buffer);
    }

    // === PipeWire software volume ===
    // Use pactl (PulseAudio compat layer) to set the sink volume.  This is
    // the same mechanism used by the per-output-group volume controls in the
    // PHP UI (which are known to work).  It avoids the fragile pw-cli node-ID
    // lookup and correctly respects PIPEWIRE_RUNTIME_DIR / XDG_RUNTIME_DIR
    // rather than PIPEWIRE_REMOTE.
    //
    // For audio groups: target the named combine-stream sink (fpp_group_*).
    // For direct PipeWire sinks: target the configured sink name, or
    // @DEFAULT_SINK@ if none is configured.
    //
    // Volume 0 is expressed as a real 0% so the sink goes silent without
    // needing a separate mute call; any non-zero restore includes an explicit
    // unmute in case a prior 0% left the sink muted.
    if (usePipeWireBackend) {
        const bool shouldMute = (volume == 0);
        std::string pwPrefix = "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp "
                               "XDG_RUNTIME_DIR=/run/pipewire-fpp "
                               "PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse ";

        // Choose the pactl sink target: named sink if available, else default.
        std::string paTarget = pipewireSink.empty() ? "@DEFAULT_SINK@" : pipewireSink;

        if (shouldMute) {
            // Set volume to 0% and explicitly mute for true silence.
            snprintf(buffer, sizeof(buffer),
                 "%s sh -c 'pactl set-sink-volume %s 0%% && pactl set-sink-mute %s 1' >/dev/null 2>&1",
                 pwPrefix.c_str(), paTarget.c_str(), paTarget.c_str());
            LogDebug(VB_MEDIAOUT, "Muting PipeWire sink: %s \n", buffer);
            system(buffer);
        } else {
            bool wasZero = (lastPipeWireVolume == 0);
            if (wasZero) {
                // Unmute and set volume atomically.
                snprintf(buffer, sizeof(buffer),
                     "%s sh -c 'pactl set-sink-mute %s 0 && pactl set-sink-volume %s %d%%' >/dev/null 2>&1",
                     pwPrefix.c_str(), paTarget.c_str(), paTarget.c_str(), volume);
                LogDebug(VB_MEDIAOUT, "Unmuting and setting PipeWire sink volume: %s \n", buffer);
            } else {
                snprintf(buffer, sizeof(buffer),
                     "%s pactl set-sink-volume %s %d%% >/dev/null 2>&1",
                     pwPrefix.c_str(), paTarget.c_str(), volume);
                LogDebug(VB_MEDIAOUT, "Setting PipeWire sink volume: %s \n", buffer);
            }
            system(buffer);
        }

        lastPipeWireVolume = volume;
    }
#else
    MacOSSetVolume(vol);
#endif

    std::unique_lock<std::mutex> lock(mediaOutputLock);
    // In PipeWire mode, volume is controlled via pactl on the sink (above).
    // Don't also set the GStreamer volume element or we get double attenuation.
    // The GStreamer volume element is still used for per-track volumeAdjust
    // (dB offset).  In ALSA mode, propagate to GStreamer as before.
    if (mediaOutput && !usePipeWireBackend)
        mediaOutput->SetVolume(vol);
}

static std::set<std::string> AUDIO_EXTS = {
    "mp3", "ogg", "m4a", "m4p", "wav", "au", "wma", "flac", "aac",
    "MP3", "OGG", "M4A", "M4P", "WAV", "AU", "WMA", "FLAC", "AAC"
};
static std::map<std::string, std::string> VIDEO_EXTS = {
    { "mp4", "mp4" }, { "MP4", "mp4" }, { "avi", "avi" }, { "AVI", "avi" }, { "mov", "mov" }, { "MOV", "mov" }, { "mkv", "mkv" }, { "MKV", "mkv" }, { "mpg", "mpg" }, { "MPG", "mpg" }, { "mpeg", "mpeg" }, { "MPEG", "mpeg" }
};

bool IsExtensionVideo(const std::string& ext) {
    return VIDEO_EXTS.find(ext) != VIDEO_EXTS.end();
}
bool IsExtensionAudio(const std::string& ext) {
    return AUDIO_EXTS.find(ext) != AUDIO_EXTS.end();
}

std::string GetVideoFilenameForMedia(const std::string& filename, std::string& ext) {
    ext = "";
    std::string result("");
    std::size_t found = filename.find_last_of(".");
    std::string oext = filename.substr(found + 1);
    std::string lext = toLowerCopy(oext);
    std::string bfile = filename.substr(0, found + 1);
    std::string hbfile = bfile;
    std::string videoPath = FPP_DIR_VIDEO("/" + bfile);

    std::string hostname = getSetting("HostName");
    std::string hostVideoPath = "";
    if (hostname != "") {
        hostVideoPath = videoPath.substr(0, videoPath.length() - 2) + "-" + hostname + ".";
        hbfile = filename.substr(0, found) + "-" + hostname + ".";
    }

    if (IsExtensionVideo(lext)) {
        if (hostVideoPath != "" && FileExists(hostVideoPath + oext)) {
            ext = lext;
            result = hbfile + oext;
        } else if (hostVideoPath != "" && FileExists(hostVideoPath + lext)) {
            ext = lext;
            result = hbfile + lext;
        } else if (FileExists(videoPath + oext)) {
            ext = lext;
            result = bfile + oext;
        } else if (FileExists(videoPath + lext)) {
            ext = lext;
            result = bfile + lext;
        }
    } else if (IsExtensionAudio(lext)) {
        for (auto& n : VIDEO_EXTS) {
            if (FileExists(hostVideoPath + n.first)) {
                ext = n.second;
                result = hbfile + n.first;
                return result;
            } else if (FileExists(videoPath + n.first)) {
                ext = n.second;
                result = bfile + n.first;
                return result;
            }
        }
    }

    return result;
}
bool HasAudioForMedia(std::string& mediaFilename) {
    std::string fullMediaPath = mediaFilename;

    std::string hostname = getSetting("HostName");
    if (hostname != "") {
        std::size_t found = mediaFilename.find_last_of(".");
        std::string hostMediaPath;
        if (found != std::string::npos) {
            hostMediaPath = mediaFilename.substr(0, found) + "-" + hostname + mediaFilename.substr(found);
        } else {
            hostMediaPath = mediaFilename + "-" + hostname;
        }
        if (FileExists(hostMediaPath)) {
            mediaFilename = hostMediaPath;
            return true;
        }
        hostMediaPath = FPP_DIR_MUSIC("/" + hostMediaPath);
        if (FileExists(hostMediaPath)) {
            mediaFilename = hostMediaPath;
            return true;
        }
    }

    if (FileExists(mediaFilename)) {
        return true;
    }
    fullMediaPath = FPP_DIR_MUSIC("/" + mediaFilename);
    if (FileExists(fullMediaPath)) {
        mediaFilename = fullMediaPath;
        return true;
    }
    return false;
}
bool HasVideoForMedia(std::string& filename) {
    std::string ext;
    std::string fp = GetVideoFilenameForMedia(filename, ext);
    if (fp != "") {
        filename = fp;
    }
    return fp != "";
}

static bool IsHDMIOut(std::string& vOut) {
    if (vOut == "--HDMI--" || vOut == "--hdmi--" || vOut == "HDMI") {
        vOut = "HDMI-A-1";
    }
    if (vOut.starts_with("HDMI-") || vOut.starts_with("DSI-") || vOut.starts_with("Composite-")) {
        for (int x = 0; x < 4; x++) {
            std::string conn = "/sys/class/drm/card" + std::to_string(x) + "-" + vOut + "/status";
            if (FileExists(conn)) {
                std::string status = GetFileContents(conn);
                return !contains(status, "disconnected");
            }
        }
    }
    return false;
}

MediaOutputBase* CreateMediaOutput(const std::string& mediaFilename, const std::string& vOut, int streamSlot) {
    std::string tmpFile(mediaFilename);
    std::size_t found = mediaFilename.find_last_of(".");
    if (found == std::string::npos) {
        LogDebug(VB_MEDIAOUT, "Unable to determine extension of media file %s\n",
                 mediaFilename.c_str());
        return nullptr;
    }
    std::string ext = toLowerCopy(mediaFilename.substr(found + 1));
    std::string vo = vOut;

    // Use per-slot status for multi-stream; slot 1 uses global for backward compat
    MediaOutputStatus* slotStatus = StreamSlotManager::Instance().GetStatus(streamSlot);
    slotStatus->output = "";

#ifdef HAS_GSTREAMER
    // GStreamer is the sole media player.  In PipeWire mode audio/video are
    // routed through the PipeWire stack; in Hardware Direct (ALSA) mode
    // GStreamer talks directly to ALSA for audio and uses kmssink for video.
    bool useGStreamer = true;

    if (useGStreamer && IsExtensionAudio(ext)) {
        LogInfo(VB_MEDIAOUT, "Using GStreamer for audio playback: %s (slot %d)\n", mediaFilename.c_str(), streamSlot);
        return new GStreamerOutput(mediaFilename, slotStatus, "--Disabled--", streamSlot);
    }
    if (useGStreamer && IsExtensionVideo(ext) && !IsHDMIOut(vo)) {
        // Video with PixelOverlay output — use GStreamer for both audio and video
        LogInfo(VB_MEDIAOUT, "Using GStreamer for video+overlay playback: %s (overlay=%s, slot %d)\n",
                mediaFilename.c_str(), vo.c_str(), streamSlot);
        return new GStreamerOutput(mediaFilename, slotStatus, vo, streamSlot);
    }
    if (useGStreamer && IsExtensionVideo(ext) && IsHDMIOut(vo)) {
        // Video to HDMI via GStreamer kmssink — audio through PipeWire
        LogInfo(VB_MEDIAOUT, "Using GStreamer for video+HDMI playback: %s (output=%s, slot %d)\n",
                mediaFilename.c_str(), vo.c_str(), streamSlot);
        slotStatus->output = vo;
        return new GStreamerOutput(mediaFilename, slotStatus, vo, streamSlot);
    }
#endif

    return nullptr;
}

static std::set<std::string> alreadyWarned;
/*
 *
 */
int OpenMediaOutput(const std::string& filename) {
    LogDebug(VB_MEDIAOUT, "OpenMediaOutput(%s)\n", filename.c_str());

    std::unique_lock<std::mutex> lock(mediaOutputLock);

    try {
        if (mediaOutput) {
            lock.unlock();
            CloseMediaOutput();
        }
        lock.unlock();

        std::string tmpFile(filename);
        std::size_t found = tmpFile.find_last_of(".");
        if (found == std::string::npos) {
            LogDebug(VB_MEDIAOUT, "Unable to determine extension of media file %s\n",
                     tmpFile.c_str());
            return 0;
        }
        std::string ext = toLowerCopy(tmpFile.substr(found + 1));

        if (getFPPmode() == REMOTE_MODE) {
            std::string orgTmp = tmpFile;
            tmpFile = GetVideoFilenameForMedia(tmpFile, ext);
            if (tmpFile == "" && HasAudioForMedia(orgTmp)) {
                tmpFile = orgTmp;
            }

            if (tmpFile == "") {
                // For v1.0 MultiSync, we can't sync audio to audio, so check for
                // a video file if the master is playing an audio file
                // video doesn't exist, punt
                tmpFile = filename;
                if (alreadyWarned.find(tmpFile) == alreadyWarned.end()) {
                    alreadyWarned.emplace(tmpFile);
                    LogDebug(VB_MEDIAOUT, "No video found for remote playing of %s\n", filename.c_str());
                }
                return 0;
            } else {
                LogDebug(VB_MEDIAOUT,
                         "Player is playing %s audio, remote will try %s\n",
                         filename.c_str(), tmpFile.c_str());
            }
        }

        lock.lock();
        std::string vOut = getSetting("VideoOutput");
        if (vOut == "") {
            if (FileExists("/sys/class/drm/card0-HDMI-A-1/status") || FileExists("/sys/class/drm/card1-HDMI-A-1/status")) {
                vOut = "--HDMI--";
            } else {
                vOut = "--Disabled--";
            }
        }
        LogWarn(VB_MEDIAOUT, "OpenMediaOutput: Creating media output for '%s' vOut='%s'\n", tmpFile.c_str(), vOut.c_str());
        mediaOutput = CreateMediaOutput(tmpFile, vOut);
        LogWarn(VB_MEDIAOUT, "OpenMediaOutput: CreateMediaOutput returned %p\n", mediaOutput);
        if (!mediaOutput) {
            LogErr(VB_MEDIAOUT, "No Media Output handler for %s\n", tmpFile.c_str());
            WarningHolder::AddWarningTimeout(60, 30, "No media output handler for " + tmpFile + " (unsupported file type?)");
            return 0;
        }

        Json::Value root;
        root["currentEntry"]["type"] = "media";
        root["currentEntry"]["mediaFilename"] = mediaOutput->m_mediaFilename;

        if (getFPPmode() == REMOTE_MODE && firstOutCreate) {
            firstOutCreate = false;
            // need to "fake" a playlist start as some plugins will not initialize
            // until a playlist is started, but remotes don't have playlists
            root["name"] = "FPP Remote";
            root["desc"] = "FPP Remote Mode";
            root["loop"] = false;
            root["repeat"] = false;
            root["random"] = false;
            root["size"] = 1;
            PluginManager::INSTANCE.playlistCallback(root, "start", "Main", 0);
        }
        MediaDetails::INSTANCE.ParseMedia(mediaOutput->m_mediaFilename.c_str());
        PluginManager::INSTANCE.mediaCallback(root, MediaDetails::INSTANCE);

        if (multiSync->isMultiSyncEnabled()) {
            multiSync->SendMediaOpenPacket(mediaOutput->m_mediaFilename);
        }
        return 1;
    } catch (const std::system_error& e) {
        LogErr(VB_MEDIAOUT, "System exception starting media for %s.  Code: %d   What: %s\n", filename.c_str(), e.code().value(), e.what());
        WarningHolder::AddWarningTimeout(60, 30, "Error starting media " + filename + ": " + e.what());
        return 0;
    }
}
bool MatchesRunningMediaFilename(const std::string& filename) {
    if (mediaOutput) {
        std::string tmpFile = filename;
        if (HasAudioForMedia(tmpFile)) {
            if (mediaOutput->m_mediaFilename == tmpFile || mediaOutput->m_mediaFilename == filename) {
                return true;
            }
        }
        tmpFile = filename;
        if (HasVideoForMedia(tmpFile)) {
            if (mediaOutput->m_mediaFilename == tmpFile || mediaOutput->m_mediaFilename == filename) {
                return true;
            }
        }
    }
    return false;
}

int StartMediaOutput(const std::string& filename, int msTime) {
    if (!MatchesRunningMediaFilename(filename)) {
        CloseMediaOutput();
    }

    if (mediaOutput && mediaOutput->IsPlaying()) {
        CloseMediaOutput();
    }
    if (!mediaOutput) {
        OpenMediaOutput(filename);
    }
    if (!mediaOutput) {
        return 0;
    }
    std::unique_lock<std::mutex> lock(mediaOutputLock);
    if (multiSync->isMultiSyncEnabled()) {
        multiSync->SendMediaSyncStartPacket(mediaOutput->m_mediaFilename);
    }

    LogWarn(VB_MEDIAOUT, "StartMediaOutput: Calling Start(%d) on mediaOutput=%p\n", msTime, mediaOutput);
    if (!mediaOutput->Start(msTime)) {
        LogErr(VB_MEDIAOUT, "Could not start media %s\n", mediaOutput->m_mediaFilename.c_str());
        WarningHolder::AddWarningTimeout(60, 30, "Could not start media " + mediaOutput->m_mediaFilename);
        delete mediaOutput;
        mediaOutput = 0;
        return 0;
    }
    std::map<std::string, std::string> keywords;
    keywords["MEDIA_NAME"] = filename;
    if (CommandManager::INSTANCE.HasPreset("MEDIA_STARTED")) {
        CommandManager::INSTANCE.TriggerPreset("MEDIA_STARTED", keywords);
    }

    return 1;
}
void CloseMediaOutput() {
    LogDebug(VB_MEDIAOUT, "CloseMediaOutput()\n");

    mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;

    std::unique_lock<std::mutex> lock(mediaOutputLock);
    if (!mediaOutput) {
        return;
    }

    if (mediaOutput->IsPlaying()) {
        lock.unlock();
        mediaOutput->Stop();
        lock.lock();
    }

    if (multiSync->isMultiSyncEnabled()) {
        multiSync->SendMediaSyncStopPacket(mediaOutput->m_mediaFilename);
    }

    std::map<std::string, std::string> keywords;
    keywords["MEDIA_NAME"] = mediaOutput->m_mediaFilename;
    if (CommandManager::INSTANCE.HasPreset("MEDIA_STOPPED")) {
        CommandManager::INSTANCE.TriggerPreset("MEDIA_STOPPED", keywords);
    }

    delete mediaOutput;
    mediaOutput = 0;

    Json::Value root;
    root["name"] = "FPP Remote";
    root["desc"] = "FPP Remote Mode";
    root["currentEntry"]["type"] = "media";
    root["currentEntry"]["mediaFilename"] = "";
    MediaDetails::INSTANCE.Clear();
    PluginManager::INSTANCE.mediaCallback(root, MediaDetails::INSTANCE);
}

void UpdateMasterMediaPosition(const std::string& filename, float seconds) {
    if (getFPPmode() != REMOTE_MODE) {
        return;
    }

    if (MatchesRunningMediaFilename(filename)) {
        masterMediaPosition = seconds;
        std::unique_lock<std::mutex> lock(mediaOutputLock);
        if (!mediaOutput) {
            return;
        }
        mediaOutput->AdjustSpeed(seconds);
        return;
    } else {
        OpenMediaOutput(filename);
        int msTime = (seconds > 0.0f) ? (int)(seconds * 1000.0f) : 0;
        StartMediaOutput(filename, msTime);
        masterMediaPosition = seconds;
        std::unique_lock<std::mutex> lock(mediaOutputLock);
        if (!mediaOutput) {
            return;
        }
        mediaOutput->AdjustSpeed(seconds);
        return;
    }
}
