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


/////////////////////////////////////////////////////////////////////////////
MediaOutput     *mediaOutput = 0;
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
		p, mediaOutput->childPID);

	if (p == mediaOutput->childPID)
	{
		mediaOutput->childPID =0;

		if (mediaOutput->childPipe[MEDIAOUTPUTPIPE_READ]) {
			close(mediaOutput->childPipe[MEDIAOUTPUTPIPE_READ]);
			mediaOutput->childPipe[MEDIAOUTPUTPIPE_READ] = 0;
		}

		if (mediaOutput->childPipe[MEDIAOUTPUTPIPE_WRITE]) {
			close(mediaOutput->childPipe[MEDIAOUTPUTPIPE_WRITE]);
			mediaOutput->childPipe[MEDIAOUTPUTPIPE_WRITE] = 0;
		}

		pthread_mutex_unlock(&mediaOutputLock);

		mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;
		sequence->CloseSequenceFile();
		CloseMediaOutput();
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
	if (!mediaOutput) {
		pthread_mutex_unlock(&mediaOutputLock);
		CloseMediaOutput();
	}
	pthread_mutex_unlock(&mediaOutputLock);

	pthread_mutex_lock(&mediaOutputLock);
	mediaOutput = NULL;

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

	if (mpg123Output.canHandle(tmpFile)) {
		mediaOutput = &mpg123Output;
	} else if (ogg123Output.canHandle(tmpFile)) {
		mediaOutput = &ogg123Output;
	} else if (omxplayerOutput.canHandle(tmpFile)) {
		mediaOutput = &omxplayerOutput;
	} else {
		pthread_mutex_unlock(&mediaOutputLock);
		LogDebug(VB_MEDIAOUT, "ERROR: No Media Output handler for %s\n", tmpFile);
		return 0;
	}

	if ((!mediaOutput) || (!mediaOutput->startPlaying(tmpFile))) {
		mediaOutput = 0;
		pthread_mutex_unlock(&mediaOutputLock);
		return 0;
	}

	if (getFPPmode() == MASTER_MODE)
		SendMediaSyncStartPacket(filename);

	mediaOutput->filename = strdup(filename);
	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_PLAYING;
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

	if (mediaOutput->childPID) {
		pthread_mutex_unlock(&mediaOutputLock);
		mediaOutput->stopPlaying();
		pthread_mutex_lock(&mediaOutputLock);
	}

	if (getFPPmode() == MASTER_MODE)
		SendMediaSyncStopPacket(mediaOutput->filename);

	free(mediaOutput->filename);
	mediaOutput->filename = NULL;

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
	int maxDelta = (mediaOutputStatus.mediaSeconds < 10) ? 5 : 5;
	int desiredDelta = diff / -33;

	if (desiredDelta > maxDelta)
		desiredDelta = maxDelta;
	else if (desiredDelta < (0 - maxDelta))
		desiredDelta = 0 - maxDelta;



	LogDebug(VB_MEDIAOUT, "Master: %.2f, Local: %.2f, Diff: %dms, delta: %d, new: %d\n",
		masterMediaPosition, mediaOutputStatus.mediaSeconds, diff,
		mediaOutputStatus.speedDelta, desiredDelta);

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
				mediaOutput->speedUp();
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
				mediaOutput->slowDown();
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
			mediaOutput->slowDown();
		else if (mediaOutputStatus.speedDelta == -1)
			mediaOutput->speedUp();
		else if (mediaOutputStatus.speedDelta != 0)
			mediaOutput->speedNormal();

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


