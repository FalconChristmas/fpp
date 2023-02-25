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

#include <filesystem>
using namespace std::filesystem;

#include <sys/wait.h>

#include "MultiSync.h"
#include "Playlist.h"
#include "PlaylistEntryMedia.h"
#include "Plugins.h"
#include "mediadetails.h"
#include "channeloutput/ChannelOutputSetup.h"

int PlaylistEntryMedia::m_openStartDelay = -1;

/*
 *
 */
PlaylistEntryMedia::PlaylistEntryMedia(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_status(0),
    m_mediaOutput(NULL),
    m_videoOutput("--Default--"),
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
    pthread_mutex_init(&m_mediaOutputLock, NULL);
}

/*
 *
 */
PlaylistEntryMedia::~PlaylistEntryMedia() {
    pthread_mutex_destroy(&m_mediaOutputLock);
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
            return 0;
        }
    }

    if (config.isMember("videoOut")) {
        m_videoOutput = config["videoOut"].asString();
    }
    m_pausedTime = -1;
    return PlaylistEntryBase::Init(config);
}

int PlaylistEntryMedia::PreparePlay() {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::PreparePlay()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    if (m_fileMode != "single") {
        if (m_files.size()) {
            m_mediaFilename = GetNextRandomFile();
        } else {
            if (GetFileList()) {
                m_mediaFilename = GetNextRandomFile();
            } else {
                LogErr(VB_PLAYLIST, "Error, no files found when trying to play random file in %s mode\n",
                       m_fileMode.c_str());
                return 0;
            }
        }
    }
    if (!OpenMediaOutput()) {
        FinishPlay();
        return 0;
    }

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendMediaOpenPacket(m_mediaFilename);

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

    float f = mediaOutputStatus.mediaSeconds * 1000;
    if (m_duration == 0) {
        float f = mediaOutputStatus.minutesTotal * 60 + mediaOutputStatus.secondsTotal;
        f *= 1000;
        m_duration = f;
    }
    if (f > m_duration) {
        m_duration = f;
    }
    pthread_mutex_lock(&m_mediaOutputLock);

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendMediaSyncStartPacket(m_mediaFilename);

    ResetChannelOutputFrameNumber();
    if (!m_mediaOutput->Start()) {
        LogErr(VB_MEDIAOUT, "Could not start media %s\n", m_mediaOutput->m_mediaFilename.c_str());
        delete m_mediaOutput;
        m_mediaOutput = 0;
        pthread_mutex_unlock(&m_mediaOutputLock);
        FinishPlay();
        return 0;
    }

    pthread_mutex_unlock(&m_mediaOutputLock);

    return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryMedia::Process(void) {
    pthread_mutex_lock(&m_mediaOutputLock);

    if (m_mediaOutput) {
        m_mediaOutput->Process();
        if (!m_mediaOutput->IsPlaying()) {
            FinishPlay();
            pthread_mutex_unlock(&m_mediaOutputLock);
            CloseMediaOutput();
            pthread_mutex_lock(&m_mediaOutputLock);
        }
    } else {
        FinishPlay();
    }

    pthread_mutex_unlock(&m_mediaOutputLock);
    return PlaylistEntryBase::Process();
}

uint64_t PlaylistEntryMedia::GetLengthInMS() {
    if (m_duration == 0) {
        MediaDetails details;
        details.ParseMedia(m_mediaFilename.c_str());
        m_duration = details.lengthMS;
        if (m_duration == 0) {
            float f = mediaOutputStatus.minutesTotal * 60 + mediaOutputStatus.secondsTotal;
            f *= 1000;
            m_duration = f;
        }
    }
    return m_duration;
}
uint64_t PlaylistEntryMedia::GetElapsedMS() {
    if (m_mediaOutput) {
        float f = mediaOutputStatus.secondsElapsed * 1000;
        f += mediaOutputStatus.subSecondsElapsed * 10; // subSec is in 1/100th second
        if (IsPaused()) {
            f = m_pausedStatus.secondsElapsed * 1000;
            f += m_pausedStatus.subSecondsElapsed * 10; // subSec is in 1/100th second
        }
        if (m_duration == 0) {
            float f = mediaOutputStatus.minutesTotal * 60 + mediaOutputStatus.secondsTotal;
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
int PlaylistEntryMedia::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::Stop()\n");

    CloseMediaOutput();

    Events::Publish("playlist/media/status", "");
    Events::Publish("playlist/media/title", "");
    Events::Publish("playlist/media/artist", "");

    return PlaylistEntryBase::Stop();
}

/*
 *
 */
int PlaylistEntryMedia::HandleSigChild(pid_t pid) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::HandleSigChild(%d)\n", pid);

    pthread_mutex_lock(&m_mediaOutputLock);

    if (pid != m_mediaOutput->m_childPID) {
        pthread_mutex_unlock(&m_mediaOutputLock);
        return 0;
    }

    m_mediaOutput->Close();
    m_mediaOutput->m_childPID = 0;

    delete m_mediaOutput;
    m_mediaOutput = NULL;

    FinishPlay();

    pthread_mutex_unlock(&m_mediaOutputLock);

    Events::Publish("playlist/media/status", "");
    Events::Publish("playlist/media/title", "");
    Events::Publish("playlist/media/artist", "");

    return 1;
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

    pthread_mutex_lock(&m_mediaOutputLock);
    if (m_mediaOutput) {
        pthread_mutex_unlock(&m_mediaOutputLock);
        CloseMediaOutput();
        pthread_mutex_lock(&m_mediaOutputLock);
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

    MediaDetails::INSTANCE.ParseMedia(m_mediaFilename.c_str());
    PluginManager::INSTANCE.mediaCallback(m_parentPlaylist->GetInfo(), MediaDetails::INSTANCE);

    std::string vOut = m_videoOutput;
    if (vOut == "--Default--") {
        vOut = getSetting("VideoOutput");
    }
    if (vOut == "") {
#if !defined(PLATFORM_BBB)
        vOut = "--HDMI--";
#else
        vOut = "--Disabled--";
#endif
    }

    m_mediaOutput = CreateMediaOutput(tmpFile, vOut);

    if (!m_mediaOutput) {
        pthread_mutex_unlock(&m_mediaOutputLock);
        LogDebug(VB_MEDIAOUT, "No Media Output handler for %s\n", tmpFile.c_str());
        return 0;
    }

    pthread_mutex_unlock(&m_mediaOutputLock);

    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::OpenMediaOutput() - Complete\n");

    return 1;
}

/*
 *
 */
int PlaylistEntryMedia::CloseMediaOutput(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::CloseMediaOutput()\n");

    mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;

    pthread_mutex_lock(&m_mediaOutputLock);
    if (!m_mediaOutput) {
        pthread_mutex_unlock(&m_mediaOutputLock);
        return 0;
    }

    if (multiSync->isMultiSyncEnabled())
        multiSync->SendMediaSyncStopPacket(m_mediaFilename);

    if (m_mediaOutput->m_childPID) {
        pthread_mutex_unlock(&m_mediaOutputLock);
        m_mediaOutput->Stop();
        pthread_mutex_lock(&m_mediaOutputLock);
    }

    delete m_mediaOutput;
    m_mediaOutput = NULL;
    pthread_mutex_unlock(&m_mediaOutputLock);

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
    MediaOutputStatus status = IsPaused() ? m_pausedStatus : mediaOutputStatus;
    result["mediaFilename"] = m_mediaFilename;
    result["status"] = status.status;
    result["secondsElapsed"] = status.secondsElapsed;
    result["subSecondsElapsed"] = status.subSecondsElapsed;
    result["secondsRemaining"] = status.secondsRemaining;
    result["subSecondsRemaining"] = status.subSecondsRemaining;
    result["minutesTotal"] = status.minutesTotal;
    result["secondsTotal"] = status.secondsTotal;
    result["mediaSeconds"] = status.mediaSeconds;

    return result;
}

Json::Value PlaylistEntryMedia::GetMqttStatus(void) {
    Json::Value result = PlaylistEntryBase::GetMqttStatus();
    MediaOutputStatus status = IsPaused() ? m_pausedStatus : mediaOutputStatus;
    result["secondsElapsed"] = status.secondsElapsed;
    result["secondsRemaining"] = status.secondsRemaining;
    result["secondsTotal"] = status.minutesTotal * 60 + status.secondsTotal;
    result["mediaName"] = m_mediaFilename;
    result["mediaTitle"] = MediaDetails::INSTANCE.title;
    result["mediaArtist"] = MediaDetails::INSTANCE.artist;

    return result;
}

void PlaylistEntryMedia::Pause() {
    m_pausedStatus = mediaOutputStatus;
    m_pausedTime = GetElapsedMS();
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
        PreparePlay();

        pthread_mutex_lock(&m_mediaOutputLock);
        if (multiSync->isMultiSyncEnabled())
            multiSync->SendMediaSyncStartPacket(m_mediaFilename);

        if (!m_mediaOutput->Start(m_pausedTime)) {
            LogErr(VB_MEDIAOUT, "Could not start media %s\n", m_mediaOutput->m_mediaFilename.c_str());
            delete m_mediaOutput;
            m_mediaOutput = nullptr;
            pthread_mutex_unlock(&m_mediaOutputLock);
        }
        m_pausedTime = -1;
        mediaOutputStatus = m_pausedStatus;
        pthread_mutex_unlock(&m_mediaOutputLock);
    }
}
