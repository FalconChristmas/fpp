/*
 *   omxplayer driver for Falcon Pi Player (FPP)
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

#include <errno.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio_ext.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "channeloutputthread.h"
#include "common.h"
#include "controlrecv.h"
#include "controlsend.h"
#include "log.h"
#include "omxplayer.h"
#include "settings.h"
#include "sequence.h"

#define MAX_BYTES_OMX 1000
#define TIME_STR_MAX  8

fd_set omx_active_fd_set, omx_read_fd_set;

int pipeFromOMX[2];

char omxBuffer[MAX_BYTES_OMX];
char dataStr[17];


int omxplayer_StartPlaying(const char *filename)
{
	char  fullVideoPath[1024];

	LogDebug(VB_MEDIAOUT, "omxplayer_StartPlaying(%s)\n", filename);

	bzero(&mediaOutputStatus, sizeof(mediaOutputStatus));

	if (snprintf(fullVideoPath, 1024, "%s/%s", getVideoDirectory(), filename)
		>= 1024)
	{
		LogErr(VB_MEDIAOUT, "Unable to play %s, full path name too long\n",
			filename);
		return 0;
	}

	if (!FileExists(fullVideoPath))
	{
		LogErr(VB_MEDIAOUT, "%s does not exist!\n", fullVideoPath);
		return 0;
	}

	// Create Pipes to/from omxplayer
	pid_t omxplayerPID = forkpty(&pipeFromOMX[0], 0, 0, 0);
	if (omxplayerPID == 0)			// omxplayer process
	{
		ShutdownControlSocket();

		seteuid(1000); // 'pi' user

		execl("/opt/fpp/scripts/omxplayer", "/opt/fpp/scripts/omxplayer", fullVideoPath, NULL);

		LogErr(VB_MEDIAOUT, "omxplayer_StartPlaying(), ERROR, we shouldn't "
			"be here, this means that execl() failed\n");

		exit(EXIT_FAILURE);
	}
	else							// Parent process
	{
		mediaOutput->childPID = omxplayerPID;
	}

	// Clear active file descriptor sets
	FD_ZERO (&omx_active_fd_set);
	// Set description for reading from omxplayer
	FD_SET (pipeFromOMX[0], &omx_active_fd_set);

	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_PLAYING;

	if (mediaOutputStatus.filename)
		free(mediaOutputStatus.filename);

	mediaOutputStatus.filename = strdup(filename);

	return 1;
}

/*
 *
 */
int omxplayer_IsPlaying()
{
	if (mediaOutput->childPID)
		return 1;

	return 0;
}

/*
 *
 */
void omxplayer_SlowDown(void)
{
	LogDebug(VB_MEDIAOUT, "Slowing Down playback\n");
	write(pipeFromOMX[0], "8", 1);
}

/*
 *
 */
void omxplayer_SpeedNormal(void)
{
	LogDebug(VB_MEDIAOUT, "Speed Playback Normal\n");
	write(pipeFromOMX[0], "9", 1);
}

/*
 *
 */
void omxplayer_SpeedUp(void)
{
	LogDebug(VB_MEDIAOUT, "Speeding Up playback\n");
	write(pipeFromOMX[0], "0", 1);
}

/*
 *
 */
void omxplayer_ProcessPlayerData(int bytesRead)
{
	int        ticks = 0;
	static int lastSyncCheck = 0;
	int        mins = 0;
	int        secs = 0;
	int        subsecs = 0;
	int        totalCentiSecs = 0;
	char      *ptr = NULL;

	if ((mediaOutputStatus.secondsTotal == 0) &&
		(mediaOutputStatus.minutesTotal == 0) &&
		(ptr = strstr(omxBuffer, "Duration: ")))
	{
		// Sample line format:
		// (whitespace)Duration: 00:00:37.91, start: 0.000000, bitrate: 2569 kb/s
		char *ptr2 = strchr(ptr, ',');
		if (ptr2)
		{
			*ptr2 = '\0';
			ptr = ptr + 10;

			int hours = strtol(ptr, NULL, 10);
			ptr += 3;
			mediaOutputStatus.minutesTotal = strtol(ptr, NULL, 10) + (hours * 60);
			ptr += 3;
			mediaOutputStatus.secondsTotal = strtol(ptr, NULL, 10);
		}
	}

	// Data is line buffered so all stats lines should start with "M: "
	if ((!strncmp(omxBuffer, "M:", 2)) &&
		(bytesRead > 20))
	{
		errno = 0;
		ticks = strtol(&omxBuffer[2], NULL, 10);
		if (errno) {
			LogErr(VB_MEDIAOUT, "Error parsing omxplayer output.\n");
			return;
		}
	} else {
		return;
	}

	totalCentiSecs = ticks / 10000;
	mins = totalCentiSecs / 6000;
	secs = totalCentiSecs % 6000 / 100;
	subsecs = totalCentiSecs % 100;

	mediaOutputStatus.secondsElapsed = 60 * mins + secs;

	mediaOutputStatus.subSecondsElapsed = subsecs;

	mediaOutputStatus.secondsRemaining = (mediaOutputStatus.minutesTotal * 60)
		+ mediaOutputStatus.secondsTotal - mediaOutputStatus.secondsElapsed;
	// FIXME, can we get this?
	// mediaOutputStatus.subSecondsRemaining = subsecs;

	float MediaSeconds = (float)((float)mediaOutputStatus.secondsElapsed + ((float)mediaOutputStatus.subSecondsElapsed/(float)100));

	if (getFPPmode() == MASTER_MODE)
		SendMediaSyncPacket(mediaOutputStatus.filename, 0, MediaSeconds);

	if ((IsSequenceRunning()) &&
		(mediaOutputStatus.secondsElapsed > 0) &&
		(lastSyncCheck != mediaOutputStatus.secondsElapsed))
	{
		LogDebug(VB_MEDIAOUT,
			"Elapsed: %.2d.%.2d  Remaining: %.2d Total %.2d:%.2d.\n",
			mediaOutputStatus.secondsElapsed,
			mediaOutputStatus.subSecondsElapsed,
			mediaOutputStatus.secondsRemaining,
			mediaOutputStatus.minutesTotal,
			mediaOutputStatus.secondsTotal);

		CalculateNewChannelOutputDelay(MediaSeconds);
		lastSyncCheck = mediaOutputStatus.secondsElapsed;

		if (getFPPmode() == REMOTE_MODE)
			CheckCurrentPositionAgainstMaster(MediaSeconds);
	}
}

/*
 *
 */
void omxplayer_PollPlayerInfo()
{
	int bytesRead;
	int result;
	struct timeval omx_timeout;

	omx_read_fd_set = omx_active_fd_set;

	omx_timeout.tv_sec = 0;
	omx_timeout.tv_usec = 5;

	if(select(FD_SETSIZE, &omx_read_fd_set, NULL, NULL, &omx_timeout) < 0)
	{
	 	LogErr(VB_MEDIAOUT, "Error Select:%d\n", errno);
	 	return; 
	}
	if(FD_ISSET(pipeFromOMX[0], &omx_read_fd_set))
	{
 		bytesRead = read(pipeFromOMX[0], omxBuffer, MAX_BYTES_OMX);
		if (bytesRead > 0) 
		{
			omxplayer_ProcessPlayerData(bytesRead);
		} 
	}
}

int omxplayer_ProcessData()
{
	if(omxplayer_IsPlaying())
	{
	  omxplayer_PollPlayerInfo();
	}
}

void omxplayer_StopPlaying()
{
	if (!mediaOutput->childPID)
		return;

	LogDebug(VB_MEDIAOUT, "StopVideo(), killing pid %d\n",
		mediaOutput->childPID);

	kill(mediaOutput->childPID, SIGKILL);
	mediaOutput->childPID = 0;

	// omxplayer is a shell script wrapper around omxplayer.bin and
	// killing the PID of the schell script doesn't kill the child
	// for some reason, so use this hack for now.
	system("killall -9 omxplayer.bin");
}

/*
 *
 */
int omxplayer_CanHandle(const char *filename) {
	LogDebug(VB_MEDIAOUT, "omxplayer_CanHandle(%s)\n", filename);

	int len = strlen(filename);

	if (len < 4)
		return 0;

	if ((!strcasecmp(filename + len - 4, ".mp4")) ||
		(!strcasecmp(filename + len - 4, ".mkv")))
		return 1;

	return 0;
}

MediaOutput omxplayerOutput = {
	.filename     = NULL,
	.childPID     = 0,
	.childPipe    = pipeFromOMX,
	.canHandle    = omxplayer_CanHandle,
	.startPlaying = omxplayer_StartPlaying,
	.stopPlaying  = omxplayer_StopPlaying,
	.processData  = omxplayer_ProcessData,
	.isPlaying    = omxplayer_IsPlaying,
	.speedUp      = omxplayer_SpeedUp,
	.slowDown     = omxplayer_SlowDown,
	.speedNormal  = omxplayer_SpeedNormal
	};

