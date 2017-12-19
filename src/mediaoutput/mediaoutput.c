/*
 *   Media handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "controlsend.h"
#include "log.h"
#include "mediaoutput.h"
#include "mpg123.h"
#include "ogg123.h"
#include "omxplayer.h"
#include "Sequence.h"
#include "settings.h"

#if HAS_SDL
#include "SDLOut.h"
#endif


/////////////////////////////////////////////////////////////////////////////
MediaOutputBase *mediaOutput = 0;
pthread_mutex_t  mediaOutputLock;
float            masterMediaPosition = 0.0;

MediaOutputStatus mediaOutputStatus = {
	MEDIAOUTPUTSTATUS_IDLE, //status
	};

void MediaOutput_sigchld_handler(int signal)
{
	int status;
	pid_t p = waitpid(-1, &status, WNOHANG);

	pthread_mutex_lock(&mediaOutputLock);
	if (!mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		return;
	}

	LogDebug(VB_MEDIAOUT,
		"MediaOutput_sigchld_handler(): pid: %d, waiting for %d\n",
		p, mediaOutput->m_childPID);

	if (p == mediaOutput->m_childPID)
	{
		mediaOutput->Close();
		mediaOutput->m_childPID = 0;

		pthread_mutex_unlock(&mediaOutputLock);

		if ((sequence->m_seqMSRemaining > 0) &&
			(sequence->m_seqMSRemaining < 2000))
		{
			usleep(sequence->m_seqMSRemaining * 1000);
		}

		// Always sleep an extra 100ms to let the sequence finish since playlist watches the media output
		if (sequence->IsSequenceRunning())
			usleep(100000);

		mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;
		CloseMediaOutput();

		if (sequence->IsSequenceRunning())
			sequence->CloseSequenceFile();

		// Do we really need this??
		usleep(1000000);
	} else {
		pthread_mutex_unlock(&mediaOutputLock);
	}
}

/*
 *
 */
void InitMediaOutput(void)
{
	if (pthread_mutex_init(&mediaOutputLock, NULL) != 0) {
		LogDebug(VB_MEDIAOUT, "ERROR: Media Output mutex init failed!\n");
	}

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = MediaOutput_sigchld_handler;
	sigaction(SIGCHLD, &sa, NULL);
}

/*
 *
 */
void CleanupMediaOutput(void)
{
	CloseMediaOutput();

	pthread_mutex_destroy(&mediaOutputLock);
}

/*
 *
 */
int OpenMediaOutput(char *filename) {
	LogDebug(VB_MEDIAOUT, "OpenMediaOutput(%s)\n", filename);

	pthread_mutex_lock(&mediaOutputLock);
	if (mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		CloseMediaOutput();
	}
	pthread_mutex_unlock(&mediaOutputLock);

	pthread_mutex_lock(&mediaOutputLock);

	char tmpFile[1024];
	strcpy(tmpFile, filename);

	int filenameLen = strlen(filename);
	if ((getFPPmode() == REMOTE_MODE) && (filenameLen > 4))
	{
		// For v1.0 MultiSync, we can't sync audio to audio, so check for
		// a video file if the master is playing an audio file
		if (!strcmp(&tmpFile[filenameLen - 4], ".mp3"))
		{
			strcpy(&tmpFile[filenameLen - 4], ".mp4");
			LogDebug(VB_MEDIAOUT,
				"Master is playing MP3 %s, remote will try %s Video\n",
				filename, tmpFile);
		}
		else if (!strcmp(&tmpFile[filenameLen - 4], ".ogg"))
		{
			strcpy(&tmpFile[filenameLen - 4], ".mp4");
			LogDebug(VB_MEDIAOUT,
				"Master is playing OGG %s, remote will try %s Video\n",
				filename, tmpFile);
		}
	}

	if (!strcasecmp(&tmpFile[filenameLen - 4], ".mp3")) {
#ifndef HAS_SDL
		mediaOutput = new mpg123Output(tmpFile, &mediaOutputStatus);
	} else if (!strcasecmp(&tmpFile[filenameLen - 4], ".ogg")) {
		mediaOutput = new ogg123Output(tmpFile, &mediaOutputStatus);
#else
        mediaOutput = new SDLOutput(tmpFile, &mediaOutputStatus);
    } else if (!strcasecmp(&tmpFile[filenameLen - 4], ".m4a")
               || !strcasecmp(&tmpFile[filenameLen - 4], ".ogg")) {
        mediaOutput = new SDLOutput(tmpFile, &mediaOutputStatus);
#endif
#ifdef PLATFORM_PI
	} else if ((!strcasecmp(&tmpFile[filenameLen - 4], ".mp4")) ||
			   (!strcasecmp(&tmpFile[filenameLen - 4], ".mkv"))) {
		mediaOutput = new omxplayerOutput(tmpFile, &mediaOutputStatus);
#endif
	} else {
		pthread_mutex_unlock(&mediaOutputLock);
		LogDebug(VB_MEDIAOUT, "No Media Output handler for %s\n", tmpFile);
		return 0;
	}

	if (!mediaOutput)
	{
		pthread_mutex_unlock(&mediaOutputLock);
		return 0;
	}

	if (getFPPmode() == MASTER_MODE)
		SendMediaSyncStartPacket(mediaOutput->m_mediaFilename.c_str());

	if (!mediaOutput->Start())
	{
		delete mediaOutput;
		mediaOutput = 0;
		pthread_mutex_unlock(&mediaOutputLock);
		return 0;
	}

	mediaOutputStatus.speedDelta = 0;

	pthread_mutex_unlock(&mediaOutputLock);

	return 1;
}

void CloseMediaOutput(void) {
	LogDebug(VB_MEDIAOUT, "CloseMediaOutput()\n");

	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;

	pthread_mutex_lock(&mediaOutputLock);
	if (!mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		return;
	}

	if (mediaOutput->IsPlaying())
	{
		pthread_mutex_unlock(&mediaOutputLock);
		mediaOutput->Stop();
		pthread_mutex_lock(&mediaOutputLock);
	}

	if (getFPPmode() == MASTER_MODE)
		SendMediaSyncStopPacket(mediaOutput->m_mediaFilename.c_str());

	delete mediaOutput;
	mediaOutput = 0;

	pthread_mutex_unlock(&mediaOutputLock);
}

void CheckCurrentPositionAgainstMaster(void)
{
	int diff = (int)(mediaOutputStatus.mediaSeconds * 1000)
				- (int)(masterMediaPosition * 1000);
	int i = 0;

	if (!mediaOutput)
		return;

	// Allow faster sync in first 10 seconds
	int maxDelta = (mediaOutputStatus.mediaSeconds < 10) ? 15 : 5;
	int desiredDelta = diff / -33;

	if (desiredDelta > maxDelta)
		desiredDelta = maxDelta;
	else if (desiredDelta < (0 - maxDelta))
		desiredDelta = 0 - maxDelta;



	LogDebug(VB_MEDIAOUT, "Master: %.2f, Local: %.2f, Diff: %dms, delta: %d, new: %d\n",
		masterMediaPosition, mediaOutputStatus.mediaSeconds, diff,
		mediaOutputStatus.speedDelta, desiredDelta);

	// Can't adjust speed if not playing yet
	if (mediaOutputStatus.mediaSeconds < 0.01)
		return;

	if (desiredDelta)
	{
		if (mediaOutputStatus.speedDelta < desiredDelta)
		{
			while (mediaOutputStatus.speedDelta < desiredDelta)
			{
				pthread_mutex_lock(&mediaOutputLock);
				if (!mediaOutput)
				{
					pthread_mutex_unlock(&mediaOutputLock);
					return;
				}
				mediaOutput->AdjustSpeed(1);
				pthread_mutex_unlock(&mediaOutputLock);
				mediaOutputStatus.speedDelta++;

				if (mediaOutputStatus.speedDelta < desiredDelta)
					usleep(30000);
			}
		}
		else if (mediaOutputStatus.speedDelta > desiredDelta)
		{
			while (mediaOutputStatus.speedDelta > desiredDelta)
			{
				pthread_mutex_lock(&mediaOutputLock);
				if (!mediaOutput)
				{
					pthread_mutex_unlock(&mediaOutputLock);
					return;
				}
				mediaOutput->AdjustSpeed(-1);
				pthread_mutex_unlock(&mediaOutputLock);
				mediaOutputStatus.speedDelta--;

				if (mediaOutputStatus.speedDelta > desiredDelta)
					usleep(30000);
			}
		}
	}
	else
	{
		pthread_mutex_lock(&mediaOutputLock);
		if (!mediaOutput)
		{
			pthread_mutex_unlock(&mediaOutputLock);
			return;
		}

		if (mediaOutputStatus.speedDelta == 1)
			mediaOutput->AdjustSpeed(-1);
		else if (mediaOutputStatus.speedDelta == -1)
			mediaOutput->AdjustSpeed(1);
		else if (mediaOutputStatus.speedDelta != 0)
			mediaOutput->AdjustSpeed(0);

		pthread_mutex_unlock(&mediaOutputLock);

		mediaOutputStatus.speedDelta = 0;
	}
}

void UpdateMasterMediaPosition(float seconds)
{
	if (getFPPmode() != REMOTE_MODE)
		return;

	masterMediaPosition = seconds;

	CheckCurrentPositionAgainstMaster();
}


