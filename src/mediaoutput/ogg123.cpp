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
#include "common.h"
#include "controlsend.h"
#include "log.h"
#include "ogg123.h"
#include "Sequence.h"
#include "settings.h"


/*
 *
 */
ogg123Output::ogg123Output(std::string mediaFilename, MediaOutputStatus *status)
{
	LogDebug(VB_MEDIAOUT, "ogg123Output::ogg123Output(%s)\n",
		mediaFilename.c_str());

	m_mediaFilename = mediaFilename;
	m_mediaOutputStatus = status;
}

/*
 *
 */
ogg123Output::~ogg123Output()
{
}

/*
 *
 */
int ogg123Output::Start(void)
{
	std::string fullAudioPath;

	LogDebug(VB_MEDIAOUT, "ogg123Output::Start(%s)\n", m_mediaFilename.c_str());

	bzero(m_mediaOutputStatus, sizeof(MediaOutputStatus));

	fullAudioPath = getMusicDirectory();
	fullAudioPath += "/";
	fullAudioPath += m_mediaFilename;

	if (!FileExists(fullAudioPath.c_str()))
	{
		LogErr(VB_MEDIAOUT, "%s does not exist!\n", fullAudioPath.c_str());
		return 0;
	}

	std::string oggPlayer(getSetting("oggPlayer"));
	if (!oggPlayer.length())
	{
		oggPlayer = OGG123_BINARY;
	}
	else if (!FileExists(oggPlayer.c_str()))
	{
		LogDebug(VB_MEDIAOUT, "Configured oggPlayer %s does not exist, "
			"falling back to %s\n", oggPlayer.c_str(), OGG123_BINARY);
		oggPlayer = OGG123_BINARY;
	}

	LogDebug(VB_MEDIAOUT, "Spawning %s for OGG playback\n", oggPlayer.c_str());

	// Create Pipes to/from ogg123
	pipe(m_childPipe);

	pid_t ogg123Pid = fork();
	if (ogg123Pid == 0)			// ogg123 process
	{
		//ogg123 uses stderr for output
	    dup2(m_childPipe[MEDIAOUTPUTPIPE_WRITE], STDERR_FILENO);
		close(m_childPipe[MEDIAOUTPUTPIPE_WRITE]);
		m_childPipe[MEDIAOUTPUTPIPE_WRITE] = 0;

		execl(oggPlayer.c_str(), oggPlayer.c_str(), fullAudioPath.c_str(), NULL);
	    exit(EXIT_FAILURE);
	}
	else							// Parent process
	{
		m_childPID = ogg123Pid;

		// Close write side of pipe from ogg
		close(m_childPipe[MEDIAOUTPUTPIPE_WRITE]);
		m_childPipe[MEDIAOUTPUTPIPE_WRITE] = 0;
	}

	LogDebug(VB_MEDIAOUT, "%s PID: %d\n", oggPlayer.c_str(), m_childPID);

	// Clear active file descriptor sets
	FD_ZERO (&m_activeFDSet);
	// Set description for reading from ogg
	FD_SET (m_childPipe[MEDIAOUTPUTPIPE_READ], &m_activeFDSet);

	m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_PLAYING;

	return 1;
}

/*
 *
 */
int ogg123Output::Process(void)
{
	if (m_childPID > 0)
	{
		PollMusicInfo();
	}

	return 1;
}

/*
 *
 */
int ogg123Output::Stop(void)
{
	LogDebug(VB_MEDIAOUT, "ogg123Output::Stop()\n");

	pthread_mutex_lock(&m_outputLock);

	if(m_childPID > 0)
	{
		pid_t childPID = m_childPID;

		m_childPID = 0;
		kill(childPID, SIGKILL);
	}

	pthread_mutex_unlock(&m_outputLock);

	m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;

	return 1;
}

void ogg123Output::ParseTimes()
{
	static int lastRemoteSync = 0;
	int result;
	int secs;
	int mins;
	int subSecs;
	char tmp[3];
	tmp[2]= '\0';

	// Mins
	tmp[0] = m_ogg123_strTime[1];
	tmp[1] = m_ogg123_strTime[2];
	sscanf(tmp,"%d",&mins);

	// Secs
	tmp[0] = m_ogg123_strTime[4];
	tmp[1] = m_ogg123_strTime[5];
	sscanf(tmp,"%d",&secs);
	m_mediaOutputStatus->secondsElapsed = 60*mins + secs;

	// Subsecs
	tmp[0] = m_ogg123_strTime[7];
	tmp[1] = m_ogg123_strTime[8];
	sscanf(tmp,"%d",&m_mediaOutputStatus->subSecondsElapsed);

	// Mins Remaining
	tmp[0] = m_ogg123_strTime[11];
	tmp[1] = m_ogg123_strTime[12];
	sscanf(tmp,"%d",&mins);

	// Secs Remaining
	tmp[0] = m_ogg123_strTime[14];
	tmp[1] = m_ogg123_strTime[15];
	sscanf(tmp,"%d",&secs);
	m_mediaOutputStatus->secondsRemaining = 60*mins + secs;

	// Subsecs remaining
	tmp[0] = m_ogg123_strTime[17];
	tmp[1] = m_ogg123_strTime[18];
	sscanf(tmp,"%d",&m_mediaOutputStatus->subSecondsRemaining);

	// Total Mins
	tmp[0] = m_ogg123_strTime[24];
	tmp[1] = m_ogg123_strTime[25];
	sscanf(tmp,"%d",&m_mediaOutputStatus->minutesTotal);

	// Total Secs
	tmp[0] = m_ogg123_strTime[27];
	tmp[1] = m_ogg123_strTime[28];
	sscanf(tmp,"%d",&m_mediaOutputStatus->secondsTotal);

	m_mediaOutputStatus->mediaSeconds = (float)((float)m_mediaOutputStatus->secondsElapsed + ((float)m_mediaOutputStatus->subSecondsElapsed/(float)100));

	if (getFPPmode() == MASTER_MODE)
	{
		if ((m_mediaOutputStatus->secondsElapsed > 0) &&
			(lastRemoteSync != m_mediaOutputStatus->secondsElapsed))
		{
			SendMediaSyncPacket(m_mediaFilename.c_str(), 0,
				m_mediaOutputStatus->mediaSeconds);
			lastRemoteSync = m_mediaOutputStatus->secondsElapsed;
		}
	}

	if ((sequence->IsSequenceRunning()) &&
		(m_mediaOutputStatus->secondsElapsed > 0))
	{
		LogExcess(VB_MEDIAOUT,
			"Elapsed: %.2d.%.2d  Remaining: %.2d Total %.2d:%.2d.\n",
			m_mediaOutputStatus->secondsElapsed,
			m_mediaOutputStatus->subSecondsElapsed,
			m_mediaOutputStatus->secondsRemaining,
			m_mediaOutputStatus->minutesTotal,
			m_mediaOutputStatus->secondsTotal);

		CalculateNewChannelOutputDelay(m_mediaOutputStatus->mediaSeconds);
	}
}

void ogg123Output::ProcessOGGData(int bytesRead)
{
	int  bufferPtr = 0;
	char state = 0;
	int  i = 0;
	bool commandNext=false;

	for(i=0;i<bytesRead;i++)
	{
		switch(m_oggBuffer[i])
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
					m_ogg123_strTime[bufferPtr++] = m_oggBuffer[i];
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
					m_ogg123_strTime[bufferPtr++] = m_oggBuffer[i];
					if(bufferPtr==32)
					{
						ParseTimes();
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

void ogg123Output::PollMusicInfo()
{
	int bytesRead;
	int result;
	struct timeval ogg123_timeout;

	m_readFDSet = m_activeFDSet;

	ogg123_timeout.tv_sec = 0;
	ogg123_timeout.tv_usec = 5;

	if(select(FD_SETSIZE, &m_readFDSet, NULL, NULL, &ogg123_timeout) < 0)
	{
	 	LogErr(VB_MEDIAOUT, "Error Select:%d\n",errno);

		Stop(); // Kill the child if we can't read from the pipe
	 	return; 
	}
	if(FD_ISSET(m_childPipe[MEDIAOUTPUTPIPE_READ], &m_readFDSet))
	{
		bytesRead = read(m_childPipe[MEDIAOUTPUTPIPE_READ], m_oggBuffer, MAX_BYTES_OGG);
		if (bytesRead > 0) 
		{
		    ProcessOGGData(bytesRead);
		} 
	}
}

