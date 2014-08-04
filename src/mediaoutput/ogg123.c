/*
 *   ogg123 player driver for Falcon Pi Player (FPP)
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include "channeloutputthread.h"
#include "log.h"
#include "mediaoutput.h"
#include "ogg123.h"
#include "sequence.h"
#include "common.h"
#include "settings.h"

#define MAX_BYTES_OGG 1000
#define TIME_STR_MAX  8

fd_set ogg123_active_fd_set, ogg123_read_fd_set;

int   pipeFromOGG[2];

char oggBuffer[MAX_BYTES_OGG];
char ogg123_strTime[34];


int ogg123_StartPlaying(const char *musicFile)
{
	char  fullAudioPath[1024];

	LogDebug(VB_MEDIAOUT, "ogg123_StartPlaying(%s)\n", musicFile);

	bzero(&mediaOutputStatus, sizeof(mediaOutputStatus));

	if (snprintf(fullAudioPath, 1024, "%s/%s", getMusicDirectory(), musicFile)
		>= 1024)
	{
		LogErr(VB_MEDIAOUT, "Unable to play %s, full path name too long\n",
			musicFile);
		return 0;
	}

	if (!FileExists(fullAudioPath))
	{
		LogErr(VB_MEDIAOUT, "%s does not exist!\n", fullAudioPath);
		return 0;
	}

	// Create Pipes to/from ogg123
	pipe(pipeFromOGG);

	pid_t ogg123Pid = fork();
	if (ogg123Pid == 0)			// ogg123 process
	{
	    dup2(pipeFromOGG[MEDIAOUTPUTPIPE_WRITE], STDERR_FILENO); //ogg123 uses stderr for output
		close(pipeFromOGG[MEDIAOUTPUTPIPE_WRITE]);
		pipeFromOGG[MEDIAOUTPUTPIPE_WRITE] = 0;

		execl("/usr/bin/ogg123", "ogg123", fullAudioPath, NULL);
	    exit(EXIT_FAILURE);
	}
	else							// Parent process
	{
		mediaOutput->childPID = ogg123Pid;

		// Close write side of pipe from ogg
		close(pipeFromOGG[MEDIAOUTPUTPIPE_WRITE]);
		pipeFromOGG[MEDIAOUTPUTPIPE_WRITE] = 0;
	}

	// Clear active file descriptor sets
	FD_ZERO (&ogg123_active_fd_set);
	// Set description for reading from ogg
	FD_SET (pipeFromOGG[MEDIAOUTPUTPIPE_READ], &ogg123_active_fd_set);

	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_PLAYING;

	return 1;
}


int ogg123_IsPlaying()
{

	int result = 0;

	pthread_mutex_lock(&mediaOutputLock);

	if(mediaOutput->childPID > 0)
		result = 1;

	pthread_mutex_unlock(&mediaOutputLock);

	return result;
}

void ogg123_StopPlaying()
{
	LogDebug(VB_MEDIAOUT, "ogg123_StopPlaying()\n");

	pthread_mutex_lock(&mediaOutputLock);

	if(mediaOutput->childPID > 0)
	{
		pid_t childPID = mediaOutput->childPID;

		mediaOutput->childPID = 0;
		kill(childPID, SIGKILL);
	}

	pthread_mutex_unlock(&mediaOutputLock);

	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_IDLE;
	usleep(1000000);
}

void ogg123_ParseTimes()
{
	static int lastSyncCheck = 0;
	int result;
	int secs;
	int mins;
	int subSecs;
	char tmp[3];
	tmp[2]= '\0';

	// Mins
	tmp[0]=ogg123_strTime[1];
	tmp[1]=ogg123_strTime[2];
	sscanf(tmp,"%d",&mins);

	// Secs
	tmp[0]=ogg123_strTime[4];
	tmp[1]=ogg123_strTime[5];
	sscanf(tmp,"%d",&secs);
	mediaOutputStatus.secondsElapsed = 60*mins + secs;

	// Subsecs
	tmp[0]=ogg123_strTime[7];
	tmp[1]=ogg123_strTime[8];
	sscanf(tmp,"%d",&mediaOutputStatus.subSecondsElapsed);

	// Mins Remaining
	tmp[0]=ogg123_strTime[11];
	tmp[1]=ogg123_strTime[12];
	sscanf(tmp,"%d",&mins);

	// Secs Remaining
	tmp[0]=ogg123_strTime[14];
	tmp[1]=ogg123_strTime[15];
	sscanf(tmp,"%d",&secs);
	mediaOutputStatus.secondsRemaining = 60*mins + secs;

	// Subsecs remaining
	tmp[0]=ogg123_strTime[17];
	tmp[1]=ogg123_strTime[18];
	sscanf(tmp,"%d",&mediaOutputStatus.subSecondsRemaining);

	// Total Mins
	tmp[0]=ogg123_strTime[24];
	tmp[1]=ogg123_strTime[25];
	sscanf(tmp,"%d",&mediaOutputStatus.minutesTotal);

	// Total Secs
	tmp[0]=ogg123_strTime[27];
	tmp[1]=ogg123_strTime[28];
	sscanf(tmp,"%d",&mediaOutputStatus.secondsTotal);

	if ((IsSequenceRunning()) &&
			(mediaOutputStatus.secondsElapsed > 0) &&
			(lastSyncCheck != mediaOutputStatus.secondsElapsed))
	{
		float MusicSeconds = (float)((float)mediaOutputStatus.secondsElapsed + ((float)mediaOutputStatus.subSecondsElapsed/(float)100));

		LogDebug(VB_MEDIAOUT,
			"Elapsed: %.2d.%.2d  Remaining: %.2d Total %.2d:%.2d.\n",
			mediaOutputStatus.secondsElapsed,
			mediaOutputStatus.subSecondsElapsed,
			mediaOutputStatus.secondsRemaining,
			mediaOutputStatus.minutesTotal,
			mediaOutputStatus.secondsTotal);

		CalculateNewChannelOutputDelay(MusicSeconds);
		lastSyncCheck = mediaOutputStatus.secondsElapsed;
	}
}

void ogg123_ProcessOGGData(int bytesRead)
{
	int  bufferPtr = 0;
	char state = 0;
	int  i = 0;
	bool commandNext=false;

	for(i=0;i<bytesRead;i++)
	{
		switch(oggBuffer[i])
		{
			case 'T':
				state = 1;
				break;
			case 'i':
				if (state == 1)
					state = 2;
				else
					state = 0;
				break;
			case 'm':
				if (state == 2)
					state = 3;
				else
					state = 0;
				break;
			case 'e':
				if (state == 3)
					state = 4;
				else
					state = 0;
				break;
		 case ':':
				if(state==4)
				{
					state = 5;
				}
				else if (bufferPtr)
				{
					ogg123_strTime[bufferPtr++]=oggBuffer[i];
				}
				break;
			default:
				if(state==5)
				{
					bufferPtr=0; 
					state=6;
				}

				if (state >= 5)
				{
					ogg123_strTime[bufferPtr++]=oggBuffer[i];
					if(bufferPtr==32)
					{
						ogg123_ParseTimes();
						bufferPtr = 0;
						state = 0;
					}
					if(bufferPtr>32)
					{
						bufferPtr=33;
					}
				}
				break;
		}
	}
}

void ogg123_PollMusicInfo()
{
	int bytesRead;
	int result;
	struct timeval ogg123_timeout;

	ogg123_read_fd_set = ogg123_active_fd_set;

	ogg123_timeout.tv_sec = 0;
	ogg123_timeout.tv_usec = 5;

	if(select(FD_SETSIZE, &ogg123_read_fd_set, NULL, NULL, &ogg123_timeout) < 0)
	{
	 	LogErr(VB_MEDIAOUT, "Error Select:%d\n",errno);
	 	return; 
	}
	if(FD_ISSET(pipeFromOGG[MEDIAOUTPUTPIPE_READ], &ogg123_read_fd_set))
	{
 		bytesRead = read(pipeFromOGG[MEDIAOUTPUTPIPE_READ], oggBuffer, MAX_BYTES_OGG);
		if (bytesRead > 0) 
		{
		    ogg123_ProcessOGGData(bytesRead);
		} 
	}
}

int ogg123_ProcessData()
{
	if(mediaOutput->childPID > 0)
	{
		ogg123_PollMusicInfo();
	}
}

/*
 *
 */
int ogg123_CanHandle(const char *filename) {
	LogDebug(VB_MEDIAOUT, "ogg123_CanHandle(%s)\n", filename);
	int len = strlen(filename);

	if (len < 4)
		return 0;

	if (!strcasecmp(filename + len - 4, ".ogg"))
		return 1;

	return 0;
}

MediaOutput ogg123Output = {
	.filename     = NULL,
	.childPID     = 0,
	.childPipe    = pipeFromOGG,
	.canHandle    = ogg123_CanHandle,
	.startPlaying = ogg123_StartPlaying,
	.stopPlaying  = ogg123_StopPlaying,
	.processData  = ogg123_ProcessData,
	.isPlaying    = ogg123_IsPlaying
	};
