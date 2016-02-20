/*
 *   Playlist Entry Media Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2016 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include "controlsend.h"
#include "log.h"
#include "mpg123.h"
#include "ogg123.h"
#include "omxplayer.h"
#include "PlaylistEntryMedia.h"
#include "settings.h"

/*
 *
 */
PlaylistEntryMedia::PlaylistEntryMedia()
  : m_status(0),
	m_secondsElapsed(0),
	m_subSecondsElapsed(0),
	m_secondsRemaining(0),
	m_subSecondsRemaining(0),
	m_minutesTotal(0),
	m_secondsTotal(0),
	m_mediaSeconds(0.0),
	m_speedDelta(0),
	m_mediaOutput(NULL)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::PlaylistEntryMedia()\n");

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
    LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::Init()\n");

	if (!config.isMember("mediaFilename"))
	{
		LogErr(VB_PLAYLIST, "Missing mediaFilename entry\n");
		return 0;
	}

	m_mediaFilename = config["mediaFilename"].asString();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryMedia::StartPlaying(void)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	if (!OpenMediaOutput())
		return 0;

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

	if (m_mediaOutput)
		m_mediaOutput->Process();

	pthread_mutex_unlock(&m_mediaOutputLock);

	sigprocmask(SIG_UNBLOCK, &blockset, NULL);

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryMedia::Stop(void)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::Stop()\n");

	CloseMediaOutput();

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
int PlaylistEntryMedia::HandleSigChild(pid_t pid)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::HandleSigChild(%d)\n", pid);

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

	return 1;
}

/*
 *
 */
void PlaylistEntryMedia::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Media Filename: %s", m_mediaFilename.c_str());
}

/*
 *
 */
int PlaylistEntryMedia::OpenMediaOutput(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryMedia::OpenMediaOutput()\n");

	pthread_mutex_lock(&m_mediaOutputLock);
	if (m_mediaOutput) {
		pthread_mutex_unlock(&m_mediaOutputLock);
		CloseMediaOutput();
	}
	pthread_mutex_unlock(&m_mediaOutputLock);

	pthread_mutex_lock(&m_mediaOutputLock);

	char tmpFile[1024];
	strcpy(tmpFile, m_mediaFilename.c_str());

	int filenameLen = strlen(tmpFile);
	if ((getFPPmode() == REMOTE_MODE) && (filenameLen > 4))
	{
		// For v1.0 MultiSync, we can't sync audio to audio, so check for
		// a video file if the master is playing an audio file
		if (!strcmp(&tmpFile[filenameLen - 4], ".mp3"))
		{
			strcpy(&tmpFile[filenameLen - 4], ".mp4");
			LogDebug(VB_MEDIAOUT,
				"Master is playing MP3 %s, remote will try %s Video\n",
				m_mediaFilename.c_str(), tmpFile);
		}
		else if (!strcmp(&tmpFile[filenameLen - 4], ".ogg"))
		{
			strcpy(&tmpFile[filenameLen - 4], ".mp4");
			LogDebug(VB_MEDIAOUT,
				"Master is playing OGG %s, remote will try %s Video\n",
				m_mediaFilename.c_str(), tmpFile);
		}
	}

	if (!strcasecmp(&tmpFile[filenameLen - 4], ".mp3")) {
		m_mediaOutput = new mpg123Output(tmpFile, &mediaOutputStatus);
	} else if (!strcasecmp(&tmpFile[filenameLen - 4], ".ogg")) {
		m_mediaOutput = new ogg123Output(tmpFile, &mediaOutputStatus);
	} else if ((!strcasecmp(&tmpFile[filenameLen - 4], ".mp4")) ||
			   (!strcasecmp(&tmpFile[filenameLen - 4], ".mkv"))) {
		m_mediaOutput = new omxplayerOutput(tmpFile, &mediaOutputStatus);
	} else {
		pthread_mutex_unlock(&m_mediaOutputLock);
		LogDebug(VB_MEDIAOUT, "ERROR: No Media Output handler for %s\n", tmpFile);
		return 0;
	}

	if (!m_mediaOutput)
	{
		pthread_mutex_unlock(&m_mediaOutputLock);
		return 0;
	}

	if (getFPPmode() == MASTER_MODE)
		SendMediaSyncStartPacket(m_mediaFilename.c_str());

	if (!m_mediaOutput->Start()) {
		delete m_mediaOutput;
		m_mediaOutput = 0;
		pthread_mutex_unlock(&m_mediaOutputLock);
		return 0;
	}

	mediaOutputStatus.speedDelta = 0;

	pthread_mutex_unlock(&m_mediaOutputLock);

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

	if (m_mediaOutput->m_childPID) {
		pthread_mutex_unlock(&m_mediaOutputLock);
		m_mediaOutput->Stop();
		pthread_mutex_lock(&m_mediaOutputLock);
	}

	if (getFPPmode() == MASTER_MODE)
		SendMediaSyncStopPacket(m_mediaOutput->m_mediaFilename.c_str());

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
	result["status"]              = m_mediaOutputStatus.status;
	result["secondsElapsed"]      = m_mediaOutputStatus.secondsElapsed;
	result["subSecondsElapsed"]   = m_mediaOutputStatus.subSecondsElapsed;
	result["secondsRemaining"]    = m_mediaOutputStatus.secondsRemaining;
	result["subSecondsRemaining"] = m_mediaOutputStatus.subSecondsRemaining;
	result["minutesTotal"]        = m_mediaOutputStatus.minutesTotal;
	result["secondsTotal"]        = m_mediaOutputStatus.secondsTotal;
	result["mediaSeconds"]        = m_mediaOutputStatus.mediaSeconds;
	result["speedDelta"]          = m_mediaOutputStatus.speedDelta;

	return result;
}
