/*
 *   mpg123 player driver for Falcon Pi Player (FPP)
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
#include "common.h"
#include "controlsend.h"
#include "log.h"
#include "mediaoutput.h"
#include "mpg123.h"
#include "Sequence.h"
#include "settings.h"

#define MAX_BYTES_MP3 1000
#define TIME_STR_MAX  8

#ifdef PLATFORM_BBB
#	define MPG123_BINARY "/usr/bin/mpg321"
#else
#	define MPG123_BINARY "/usr/bin/mpg123"
#endif

fd_set mpg123_active_fd_set, mpg123_read_fd_set;

int   pipeFromMP3[2];

char mp3Buffer[MAX_BYTES_MP3];
char mpg123_strTime[34];


int mpg123_StartPlaying(const char *musicFile)
{
	char  fullAudioPath[1024];

	LogDebug(VB_MEDIAOUT, "mpg123_StartPlaying(%s)\n", musicFile);

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

	char mp3Player[1024];
	strcpy(mp3Player, getSetting("mp3Player"));
	if (!mp3Player[0])
	{
		strcpy(mp3Player, MPG123_BINARY);
	}
	else if (!FileExists(mp3Player))
	{
		LogDebug(VB_MEDIAOUT, "Configured mp3Player %s does not exist, "
			"falling back to " MPG123_BINARY "\n", mp3Player);
		strcpy(mp3Player, MPG123_BINARY);
	}

	LogDebug(VB_MEDIAOUT, "Spawning %s for MP3 playback\n", mp3Player);

	// Create Pipes to/from mpg123
	pipe(pipeFromMP3);
	
	pid_t mpg123Pid = fork();
	if (mpg123Pid == 0)			// mpg123 process
	{
		//mpg123 uses stderr for output
	    dup2(pipeFromMP3[MEDIAOUTPUTPIPE_WRITE], STDERR_FILENO);

		close(pipeFromMP3[MEDIAOUTPUTPIPE_WRITE]);
		pipeFromMP3[MEDIAOUTPUTPIPE_WRITE] = 0;

		if (!strcmp(mp3Player, "/usr/bin/mpg123"))
			execl(mp3Player, mp3Player, "-v", fullAudioPath, NULL);
		else
			execl(mp3Player, mp3Player, fullAudioPath, NULL);

	    exit(EXIT_FAILURE);
	}
	else							// Parent process
	{
		mediaOutput->childPID = mpg123Pid;

		// Close write side of pipe from mpg123
		close(pipeFromMP3[MEDIAOUTPUTPIPE_WRITE]);
		pipeFromMP3[MEDIAOUTPUTPIPE_WRITE] = 0;
	}

	// Clear active file descriptor sets
	FD_ZERO (&mpg123_active_fd_set);
	// Set description for reading from mpg123
	FD_SET (pipeFromMP3[MEDIAOUTPUTPIPE_READ], &mpg123_active_fd_set);

	mediaOutputStatus.status = MEDIAOUTPUTSTATUS_PLAYING;

	return 1;
}


int mpg123_IsPlaying()
{

	int result = 0;

	pthread_mutex_lock(&mediaOutputLock);

	if(mediaOutput->childPID > 0)
		result = 1;

	pthread_mutex_unlock(&mediaOutputLock);

	return result;
}

void mpg123_StopPlaying()
{
	LogDebug(VB_MEDIAOUT, "mpg123_StopPlaying()\n");

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

void mpg123_ParseTimes()
{
	static int lastSyncCheck = 0;
	static int lastRemoteSync = 0;
	int result;
	int secs;
	int mins;
	int totalMins = 0;
	int totalMinsExtraSecs = 0;
	int subSecs;
	char tmp[3];
	tmp[2]= '\0';

	// Mins
	tmp[0]=mpg123_strTime[1];
	tmp[1]=mpg123_strTime[2];
	sscanf(tmp,"%d",&mins);
	totalMins += mins;

	// Secs
	tmp[0]=mpg123_strTime[4];
	tmp[1]=mpg123_strTime[5];
	sscanf(tmp,"%d",&secs);
	mediaOutputStatus.secondsElapsed = 60*mins + secs;
	totalMinsExtraSecs += secs;

	// Subsecs
	tmp[0]=mpg123_strTime[7];
	tmp[1]=mpg123_strTime[8];
	sscanf(tmp,"%d",&mediaOutputStatus.subSecondsElapsed);
		
	// Mins Remaining
	tmp[0]=mpg123_strTime[11];
	tmp[1]=mpg123_strTime[12];
	sscanf(tmp,"%d",&mins);
	totalMins += mins;
	
	// Secs Remaining
	tmp[0]=mpg123_strTime[14];
	tmp[1]=mpg123_strTime[15];
	sscanf(tmp,"%d",&secs);
	mediaOutputStatus.secondsRemaining = 60*mins + secs;
	totalMinsExtraSecs += secs;

	// Subsecs remaining
	tmp[0]=mpg123_strTime[17];
	tmp[1]=mpg123_strTime[18];
	sscanf(tmp,"%d",&mediaOutputStatus.subSecondsRemaining);

	mediaOutputStatus.minutesTotal = totalMins + totalMinsExtraSecs / 60;
	mediaOutputStatus.secondsTotal = totalMinsExtraSecs % 60;

	mediaOutputStatus.mediaSeconds = (float)((float)mediaOutputStatus.secondsElapsed + ((float)mediaOutputStatus.subSecondsElapsed/(float)100));

	if (getFPPmode() == MASTER_MODE)
	{
		if ((mediaOutputStatus.secondsElapsed > 0) &&
			(lastRemoteSync != mediaOutputStatus.secondsElapsed))
		{
			SendMediaSyncPacket(mediaOutput->filename, 0,
				mediaOutputStatus.mediaSeconds);
			lastRemoteSync = mediaOutputStatus.secondsElapsed;
		}
	}

	if ((sequence->IsSequenceRunning()) &&
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

// Sample format
// Frame#   149 [ 1842], Time: 00:03.89 [00:48.11], RVA:   off, Vol: 100(100)
void mpg123_ProcessMP3Data(int bytesRead)
{
	int  bufferPtr = 0;
	char state = 0;
	int  i = 0;
	bool commandNext=false;

	for(i=0;i<bytesRead;i++)
	{
		switch(mp3Buffer[i])
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
					mpg123_strTime[bufferPtr++]=mp3Buffer[i];
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
					mpg123_strTime[bufferPtr++] = mp3Buffer[i];
					if(bufferPtr == 19)
					{
						mpg123_ParseTimes();
						bufferPtr = 0;
						state = 0;
					}
					if(bufferPtr>19)
					{
						bufferPtr=20;
					}
				}
				break;
		}
	}
}

void mpg123_PollMusicInfo()
{
	int bytesRead;
	int result;
	struct timeval mpg123_timeout;

	mpg123_read_fd_set = mpg123_active_fd_set;

	mpg123_timeout.tv_sec = 0;
	mpg123_timeout.tv_usec = 5;

	if(select(FD_SETSIZE, &mpg123_read_fd_set, NULL, NULL, &mpg123_timeout) < 0)
	{
	 	LogErr(VB_MEDIAOUT, "Error Select:%d\n",errno);
	 	return; 
	}
	if(FD_ISSET(pipeFromMP3[MEDIAOUTPUTPIPE_READ], &mpg123_read_fd_set))
	{
 		bytesRead = read(pipeFromMP3[MEDIAOUTPUTPIPE_READ], mp3Buffer, MAX_BYTES_MP3);
		if (bytesRead > 0) 
		{
		    mpg123_ProcessMP3Data(bytesRead);
		} 
	}
}

int mpg123_ProcessData()
{
	if(mediaOutput->childPID > 0)
	{
		mpg123_PollMusicInfo();
	}
}

/*
 *
 */
int mpg123_CanHandle(const char *filename) {
	LogDebug(VB_MEDIAOUT, "mpg123_CanHandle(%s)\n", filename);
	int len = strlen(filename);

	if (len < 4)
		return 0;

	if (!strcasecmp(filename + len - 4, ".mp3"))
		return 1;

	return 0;
}

MediaOutput mpg123Output = {
	NULL, //filename
	0, //childPID
	pipeFromMP3, //childPipe
	mpg123_CanHandle, //canHandle
	mpg123_StartPlaying, //startPlaying
	mpg123_StopPlaying, //stopPlaying
	mpg123_ProcessData, //processData
	mpg123_IsPlaying, //isPlaying
	NULL, //speedUp
	NULL, //slowDown
	NULL, //speedNormal
	NULL, //setVolume
};
