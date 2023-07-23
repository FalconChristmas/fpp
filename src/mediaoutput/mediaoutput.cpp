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
#include <string>

#include "MultiSync.h"
#include "common.h"
#include "log.h"
#include "mediaoutput.h"
#include "commands/Commands.h"

#include "Plugins.h"
#include "SDLOut.h"
#include "Sequence.h"
#include "VLCOut.h"
#include "mediadetails.h"
#include "settings.h"
#include "../config.h"

/////////////////////////////////////////////////////////////////////////////
MediaOutputBase* mediaOutput = 0;
float masterMediaPosition = 0.0;
pthread_mutex_t mediaOutputLock;

static bool firstOutCreate = true;

MediaOutputStatus mediaOutputStatus = {
    MEDIAOUTPUTSTATUS_IDLE, // status
};

/*
 *
 */
void InitMediaOutput(void) {
    if (pthread_mutex_init(&mediaOutputLock, NULL) != 0) {
        LogDebug(VB_MEDIAOUT, "ERROR: Media Output mutex init failed!\n");
    }

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

    pthread_mutex_destroy(&mediaOutputLock);
}

#ifndef PLATFORM_OSX
static int volume = 70;
int getVolume() {
    return volume;
}
#else
int getVolume() {
    return MacOSGetVolume();
}
#endif

void setVolume(int vol) {
    char buffer[60];

    if (vol < 0)
        vol = 0;
    else if (vol > 100)
        vol = 100;

    std::string mixerDevice = getSetting("AudioMixerDevice");
    int audioOutput = getSettingInt("AudioOutput");
    std::string audio0Type = getSetting("AudioCard0Type");

#ifndef PLATFORM_OSX
    volume = vol;
    float fvol = volume;
#ifdef PLATFORM_PI
    if (audioOutput == 0 && audio0Type == "bcm2") {
        fvol = volume;
        fvol /= 2;
        fvol += 50;
    }
#endif
    snprintf(buffer, 60, "amixer set -c %d '%s' -- %.2f%% >/dev/null 2>&1",
             audioOutput, mixerDevice.c_str(), fvol);

    LogDebug(VB_SETTING, "Volume change: %d \n", volume);
    LogDebug(VB_MEDIAOUT, "Calling amixer to set the volume: %s \n", buffer);
    system(buffer);
#else
    MacOSSetVolume(vol);
#endif

    pthread_mutex_lock(&mediaOutputLock);
    if (mediaOutput)
        mediaOutput->SetVolume(vol);

    pthread_mutex_unlock(&mediaOutputLock);
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
        std::string hostMediaPath = mediaFilename.substr(0, found) + "-" + hostname + mediaFilename.substr(found);
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

MediaOutputBase* CreateMediaOutput(const std::string& mediaFilename, const std::string& vOut) {
    std::string tmpFile(mediaFilename);
    std::size_t found = mediaFilename.find_last_of(".");
    if (found == std::string::npos) {
        LogDebug(VB_MEDIAOUT, "Unable to determine extension of media file %s\n",
                 mediaFilename.c_str());
        return nullptr;
    }
    std::string ext = toLowerCopy(mediaFilename.substr(found + 1));

#ifdef HAS_VLC
    if (IsExtensionAudio(ext)) {
        if (getFPPmode() == REMOTE_MODE) {
            return new VLCOutput(mediaFilename, &mediaOutputStatus, "--Disabled--");
        } else {
            return new SDLOutput(mediaFilename, &mediaOutputStatus, "--Disabled--");
        }
    } else if (IsExtensionVideo(ext) && (vOut == "--HDMI--" || vOut == "HDMI")) {
        return new VLCOutput(mediaFilename, &mediaOutputStatus, vOut);
    } else if (IsExtensionVideo(ext))
#endif
    {
        return new SDLOutput(mediaFilename, &mediaOutputStatus, vOut);
    }
    return nullptr;
}

static std::set<std::string> alreadyWarned;
/*
 *
 */
int OpenMediaOutput(const char* filename) {
    LogDebug(VB_MEDIAOUT, "OpenMediaOutput(%s)\n", filename);

    pthread_mutex_lock(&mediaOutputLock);
    if (mediaOutput) {
        pthread_mutex_unlock(&mediaOutputLock);
        CloseMediaOutput();
    }
    pthread_mutex_unlock(&mediaOutputLock);

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
                LogDebug(VB_MEDIAOUT, "No video found for remote playing of %s\n", filename);
            }
            return 0;
        } else {
            LogDebug(VB_MEDIAOUT,
                     "Player is playing %s audio, remote will try %s\n",
                     filename, tmpFile.c_str());
        }
    }

    pthread_mutex_lock(&mediaOutputLock);
    std::string vOut = getSetting("VideoOutput");
    if (vOut == "") {
#if !defined(PLATFORM_BBB)
        vOut = "--HDMI--";
#else
        vOut = "--Disabled--";
#endif
    }

    mediaOutput = CreateMediaOutput(tmpFile, vOut);
    if (!mediaOutput) {
        pthread_mutex_unlock(&mediaOutputLock);
        LogErr(VB_MEDIAOUT, "No Media Output handler for %s\n", tmpFile.c_str());
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

    pthread_mutex_unlock(&mediaOutputLock);

    return 1;
}

bool MatchesRunningMediaFilename(const char* filename) {
    if (mediaOutput) {
        std::string tmpFile = filename;
        if (HasAudioForMedia(tmpFile)) {
            if (mediaOutput->m_mediaFilename == tmpFile || !strcmp(mediaOutput->m_mediaFilename.c_str(), filename)) {
                return true;
            }
        }
        if (HasVideoForMedia(tmpFile)) {
            if (mediaOutput->m_mediaFilename == tmpFile || !strcmp(mediaOutput->m_mediaFilename.c_str(), filename)) {
                return true;
            }
        }
    }
    return false;
}

int StartMediaOutput(const char* filename) {
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
    pthread_mutex_lock(&mediaOutputLock);
    if (multiSync->isMultiSyncEnabled())
        multiSync->SendMediaSyncStartPacket(mediaOutput->m_mediaFilename);

    if (!mediaOutput->Start()) {
        LogErr(VB_MEDIAOUT, "Could not start media %s\n", mediaOutput->m_mediaFilename.c_str());
        delete mediaOutput;
        mediaOutput = 0;
        pthread_mutex_unlock(&mediaOutputLock);
        return 0;
    }

    pthread_mutex_unlock(&mediaOutputLock);

    std::map<std::string, std::string> keywords;
    keywords["MEDIA_NAME"] = filename;
    CommandManager::INSTANCE.TriggerPreset("MEDIA_STARTED", keywords);

    return 1;
}
void CloseMediaOutput() {
    LogDebug(VB_MEDIAOUT, "CloseMediaOutput()\n");

    mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;

    pthread_mutex_lock(&mediaOutputLock);
    if (!mediaOutput) {
        pthread_mutex_unlock(&mediaOutputLock);
        return;
    }

    if (mediaOutput->IsPlaying()) {
        pthread_mutex_unlock(&mediaOutputLock);
        mediaOutput->Stop();
        pthread_mutex_lock(&mediaOutputLock);
    }

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendMediaSyncStopPacket(mediaOutput->m_mediaFilename);

    std::map<std::string, std::string> keywords;
    keywords["MEDIA_NAME"] = mediaOutput->m_mediaFilename;
    CommandManager::INSTANCE.TriggerPreset("MEDIA_STOPPED", keywords);

    delete mediaOutput;
    mediaOutput = 0;

    Json::Value root;
    root["name"] = "FPP Remote";
    root["desc"] = "FPP Remote Mode";
    root["currentEntry"]["type"] = "media";
    root["currentEntry"]["mediaFilename"] = "";
    MediaDetails::INSTANCE.Clear();
    PluginManager::INSTANCE.mediaCallback(root, MediaDetails::INSTANCE);

    pthread_mutex_unlock(&mediaOutputLock);
}

void UpdateMasterMediaPosition(const char* filename, float seconds) {
    if (getFPPmode() != REMOTE_MODE) {
        return;
    }

    if (MatchesRunningMediaFilename(filename)) {
        masterMediaPosition = seconds;
        pthread_mutex_lock(&mediaOutputLock);
        if (!mediaOutput) {
            pthread_mutex_unlock(&mediaOutputLock);
            return;
        }
        mediaOutput->AdjustSpeed(seconds);
        pthread_mutex_unlock(&mediaOutputLock);
        return;
    } else {
        // with VLC, we can jump forward a bit and get close
        OpenMediaOutput(filename);
        StartMediaOutput(filename);
        masterMediaPosition = seconds;
        pthread_mutex_lock(&mediaOutputLock);
        if (!mediaOutput) {
            pthread_mutex_unlock(&mediaOutputLock);
            return;
        }
        mediaOutput->AdjustSpeed(seconds);
        pthread_mutex_unlock(&mediaOutputLock);
        return;
    }
}
