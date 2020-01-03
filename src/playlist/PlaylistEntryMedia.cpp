/*
 *   Playlist Entry Media Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <thread>
#include <boost/algorithm/string.hpp>
#include <sys/wait.h>

#include "log.h"
#include "mediadetails.h"
#include "mqtt.h"
#include "MultiSync.h"
#include "PlaylistEntryMedia.h"
#include "Plugins.h"
#include "settings.h"
#include "common.h"
#include "mediaoutput/mpg123.h"
#include "mediaoutput/ogg123.h"
#include "mediaoutput/omxplayer.h"
#include "mediaoutput/SDLOut.h"
#include "Playlist.h"


int PlaylistEntryMedia::m_openStartDelay = -1;

/*
 *
 */
PlaylistEntryMedia::PlaylistEntryMedia(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_status(0),
	m_secondsElapsed(0),
	m_subSecondsElapsed(0),
	m_secondsRemaining(0),
	m_subSecondsRemaining(0),
	m_minutesTotal(0),
	m_secondsTotal(0),
	m_mediaSeconds(0.0),
	m_speedDelta(0),
	m_mediaOutput(NULL),
    m_videoOutput("--Default--"),
    m_openTime(0)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::PlaylistEntryMedia()\n");
    if (m_openStartDelay == -1) {
        m_openStartDelay = getSettingInt("openStartDelay");
    }
	m_type = "media";
	pthread_mutex_init(&m_mediaOutputLock, NULL);
}

/*
 *
 */
PlaylistEntryMedia::~PlaylistEntryMedia()
{
	pthread_mutex_destroy(&m_mediaOutputLock);
}

/*
 *
 */
int PlaylistEntryMedia::Init(Json::Value &config)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::Init()\n");

	if (!config.isMember("mediaName"))
	{
		LogErr(VB_PLAYLIST, "Missing mediaName entry\n");
		return 0;
	}

	m_mediaFilename = config["mediaName"].asString();
    
    if (config.isMember("videoOut")) {
        m_videoOutput = config["videoOut"].asString();
    }
	return PlaylistEntryBase::Init(config);
}


int PlaylistEntryMedia::PreparePlay() {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::StartPlaying()\n");
    
    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    if (!OpenMediaOutput()) {
        FinishPlay();
        return 0;
    }

    if (getFPPmode() == MASTER_MODE)
        multiSync->SendMediaOpenPacket(m_mediaFilename);
    
    m_openTime = GetTimeMS();
    if (mqtt) {
        mqtt->Publish("playlist/media/status", m_mediaFilename);
        mqtt->Publish("playlist/media/title", MediaDetails::INSTANCE.title);
        mqtt->Publish("playlist/media/artist", MediaDetails::INSTANCE.artist);
    }
    return 1;
}


/*
 *
 */
int PlaylistEntryMedia::StartPlaying(void)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::StartPlaying()\n");

    if (getFPPmode() == MASTER_MODE && m_openTime) {
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


    pthread_mutex_lock(&m_mediaOutputLock);

    if (getFPPmode() == MASTER_MODE)
        multiSync->SendMediaSyncStartPacket(m_mediaFilename);
    
    if (!m_mediaOutput->Start()) {
        LogErr(VB_MEDIAOUT, "Could not start media %s\n", m_mediaOutput->m_mediaFilename.c_str());
        delete m_mediaOutput;
        m_mediaOutput = 0;
        pthread_mutex_unlock(&m_mediaOutputLock);
        return 0;
    }
    
    mediaOutputStatus.speedDelta = 0;
    
    pthread_mutex_unlock(&m_mediaOutputLock);

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryMedia::Process(void)
{
	sigset_t blockset;

	sigemptyset(&blockset);
	sigaddset(&blockset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &blockset, NULL);

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

	sigprocmask(SIG_UNBLOCK, &blockset, NULL);

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryMedia::Stop(void)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::Stop()\n");

	CloseMediaOutput();

    if (mqtt) {
		mqtt->Publish("playlist/media/status", "");
        mqtt->Publish("playlist/media/title", "");
        mqtt->Publish("playlist/media/artist", "");
    }

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
int PlaylistEntryMedia::HandleSigChild(pid_t pid)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::HandleSigChild(%d)\n", pid);

	pthread_mutex_lock(&m_mediaOutputLock);

	if (pid != m_mediaOutput->m_childPID)
	{
		pthread_mutex_unlock(&m_mediaOutputLock);
		return 0;
	}

	m_mediaOutput->Close();
	m_mediaOutput->m_childPID = 0;

	delete m_mediaOutput;
	m_mediaOutput = NULL;

	FinishPlay();

	pthread_mutex_unlock(&m_mediaOutputLock);

    if (mqtt) {
        mqtt->Publish("playlist/media/status", "");
        mqtt->Publish("playlist/media/title", "");
        mqtt->Publish("playlist/media/artist", "");
    }

	return 1;
}

/*
 *
 */
void PlaylistEntryMedia::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Media Filename: %s\n", m_mediaFilename.c_str());
}

/*
 *
 */
int PlaylistEntryMedia::OpenMediaOutput(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::OpenMediaOutput() - Starting\n");

	pthread_mutex_lock(&m_mediaOutputLock);
	if (m_mediaOutput) {
		pthread_mutex_unlock(&m_mediaOutputLock);
		CloseMediaOutput();
	}
	else
		pthread_mutex_unlock(&m_mediaOutputLock);

	pthread_mutex_lock(&m_mediaOutputLock);

	std::string tmpFile = m_mediaFilename;
	std::size_t found = tmpFile.find_last_of(".");

	if (found == std::string::npos)
	{
		LogWarn(VB_MEDIAOUT, "Unable to determine extension of media file %s\n",
			m_mediaFilename.c_str());
		return 0;
	}

	std::string ext = boost::algorithm::to_lower_copy(tmpFile.substr(found + 1));

    LogDebug(VB_PLAYLIST, "PlaylistEntryMedia - Starting %s\n", tmpFile.c_str());



    MediaDetails::INSTANCE.ParseMedia(m_mediaFilename.c_str());
    PluginManager::INSTANCE.mediaCallback(playlist->GetInfo(), MediaDetails::INSTANCE);


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

#if !defined(PLATFORM_BBB)
    if (getSettingInt("LegacyMediaOutputs") && (ext == "mp3" || ext == "ogg")) {
        if (ext == "mp3") {
            m_mediaOutput = new mpg123Output(tmpFile, &mediaOutputStatus);
        } else if (ext == "ogg") {
            m_mediaOutput = new ogg123Output(tmpFile, &mediaOutputStatus);
        }
    } else
#endif
	if (IsExtensionAudio(ext)) {
        m_mediaOutput = new SDLOutput(tmpFile, &mediaOutputStatus, "--Disabled--");
#ifdef PLATFORM_PI
    } else if (IsExtensionVideo(ext) && vOut == "--HDMI--") {
        m_mediaOutput = new omxplayerOutput(tmpFile, &mediaOutputStatus);
#endif
    } else if (IsExtensionVideo(ext)) {
        m_mediaOutput = new SDLOutput(tmpFile, &mediaOutputStatus, vOut);
	} else {
		pthread_mutex_unlock(&mediaOutputLock);
		LogDebug(VB_MEDIAOUT, "No Media Output handler for %s\n", tmpFile.c_str());
		return 0;
	}

	if (!m_mediaOutput)
	{
		pthread_mutex_unlock(&m_mediaOutputLock);
		return 0;
	}

    pthread_mutex_unlock(&m_mediaOutputLock);


	LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::OpenMediaOutput() - Complete\n");

	return 1;
}

/*
 *
 */
int PlaylistEntryMedia::CloseMediaOutput(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::CloseMediaOutput()\n");

	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;

	pthread_mutex_lock(&m_mediaOutputLock);
	if (!m_mediaOutput) {
		pthread_mutex_unlock(&m_mediaOutputLock);
		return 0;
	}

    if (getFPPmode() == MASTER_MODE)
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
Json::Value PlaylistEntryMedia::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();


	result["mediaFilename"]       = m_mediaFilename;
	result["status"]              = mediaOutputStatus.status;
	result["secondsElapsed"]      = mediaOutputStatus.secondsElapsed;
	result["subSecondsElapsed"]   = mediaOutputStatus.subSecondsElapsed;
	result["secondsRemaining"]    = mediaOutputStatus.secondsRemaining;
	result["subSecondsRemaining"] = mediaOutputStatus.subSecondsRemaining;
	result["minutesTotal"]        = mediaOutputStatus.minutesTotal;
	result["secondsTotal"]        = mediaOutputStatus.secondsTotal;
	result["mediaSeconds"]        = mediaOutputStatus.mediaSeconds;
	result["speedDelta"]          = mediaOutputStatus.speedDelta;

	return result;
}

Json::Value PlaylistEntryMedia::GetMqttStatus(void)
{
	Json::Value result = PlaylistEntryBase::GetMqttStatus();
	result["secondsElapsed"]    = mediaOutputStatus.secondsElapsed;
	result["secondsRemaining"]  = mediaOutputStatus.secondsRemaining;
	result["secondsTotal"]      = mediaOutputStatus.minutesTotal * 60 + mediaOutputStatus.secondsTotal;
	result["mediaName"]         = m_mediaFilename;
	result["mediaTitle"]        = MediaDetails::INSTANCE.title;
	result["mediaArtist"]       = MediaDetails::INSTANCE.artist;

	return result;
}
