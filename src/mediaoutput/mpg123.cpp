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
#include "mpg123.h"
#include "Sequence.h"
#include "settings.h"

/*
 *
 */
mpg123Output::mpg123Output(std::string mediaFilename, MediaOutputStatus *status)
{
	LogDebug(VB_MEDIAOUT, "mpg123Output::mpg123Output(%s)\n",
		mediaFilename.c_str());

	m_mediaFilename = mediaFilename;
	m_mediaOutputStatus = status;
}

/*
 *
 */
mpg123Output::~mpg123Output()
{
}

/*
 *
 */
int mpg123Output::Start(void)
{
	std::string fullAudioPath;

	LogDebug(VB_MEDIAOUT, "mpg123Output::Start()\n");

	bzero(m_mediaOutputStatus, sizeof(MediaOutputStatus));

	fullAudioPath = getMusicDirectory();
	fullAudioPath += "/";
	fullAudioPath += m_mediaFilename;

	if (!FileExists(fullAudioPath.c_str()))
	{
		LogErr(VB_MEDIAOUT, "%s does not exist!\n", fullAudioPath.c_str());
		return 0;
	}

	std::string mp3Player(getSetting("mp3Player"));

	if (!mp3Player.length())
	{
		mp3Player = MPG123_BINARY;
	}
	else if (!FileExists(mp3Player.c_str()))
	{
		LogDebug(VB_MEDIAOUT, "Configured mp3Player %s does not exist, "
			"falling back to %s\n", mp3Player.c_str(), MPG123_BINARY);
		mp3Player = MPG123_BINARY;
	}

	LogDebug(VB_MEDIAOUT, "Spawning %s for MP3 playback\n", mp3Player.c_str());

	// Create Pipes to/from mpg123
	pipe(m_childPipe);
	
	pid_t mpg123Pid = fork();
	if (mpg123Pid == 0)			// mpg123 process
	{
		//mpg123 uses stderr for output
		dup2(m_childPipe[MEDIAOUTPUTPIPE_WRITE], STDERR_FILENO);

		close(m_childPipe[MEDIAOUTPUTPIPE_WRITE]);
		m_childPipe[MEDIAOUTPUTPIPE_WRITE] = 0;

		if (mp3Player == MPG123_BINARY)
			execl(mp3Player.c_str(), mp3Player.c_str(), "-v",
				fullAudioPath.c_str(), NULL);
		else
			execl(mp3Player.c_str(), mp3Player.c_str(),
				fullAudioPath.c_str(), NULL);

	    exit(EXIT_FAILURE);
	}
	else							// Parent process
	{
		m_childPID = mpg123Pid;

		// Close write side of pipe from mpg123
		close(m_childPipe[MEDIAOUTPUTPIPE_WRITE]);
		m_childPipe[MEDIAOUTPUTPIPE_WRITE] = 0;
	}

	// Clear active file descriptor sets
	FD_ZERO (&m_activeFDSet);
	// Set description for reading from mpg123
	FD_SET (m_childPipe[MEDIAOUTPUTPIPE_READ], &m_activeFDSet);

	m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_PLAYING;

	return 1;
}

/*
 *
 */
int mpg123Output::Process(void)
{
	if(m_childPID > 0)
	{
		PollMusicInfo();
	}

	return 1;
}

/*
 *
 */
int mpg123Output::Stop(void)
{
	LogDebug(VB_MEDIAOUT, "mpg123Output::Stop()\n");

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

/*
 *
 */
void mpg123Output::ParseTimes(void)
{
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
	tmp[0] = m_mpg123_strTime[1];
	tmp[1] = m_mpg123_strTime[2];
	sscanf(tmp,"%d",&mins);
	totalMins += mins;

	// Secs
	tmp[0] = m_mpg123_strTime[4];
	tmp[1] = m_mpg123_strTime[5];
	sscanf(tmp,"%d",&secs);
	m_mediaOutputStatus->secondsElapsed = 60*mins + secs;
	totalMinsExtraSecs += secs;

	// Subsecs
	tmp[0] = m_mpg123_strTime[7];
	tmp[1] = m_mpg123_strTime[8];
	sscanf(tmp,"%d",&m_mediaOutputStatus->subSecondsElapsed);
		
	// Mins Remaining
	tmp[0] = m_mpg123_strTime[11];
	tmp[1] = m_mpg123_strTime[12];
	sscanf(tmp,"%d",&mins);
	totalMins += mins;
	
	// Secs Remaining
	tmp[0] = m_mpg123_strTime[14];
	tmp[1] = m_mpg123_strTime[15];
	sscanf(tmp,"%d",&secs);
	m_mediaOutputStatus->secondsRemaining = 60*mins + secs;
	totalMinsExtraSecs += secs;

	// Subsecs remaining
	tmp[0] = m_mpg123_strTime[17];
	tmp[1] = m_mpg123_strTime[18];
	sscanf(tmp,"%d",&m_mediaOutputStatus->subSecondsRemaining);

	m_mediaOutputStatus->minutesTotal = totalMins + totalMinsExtraSecs / 60;
	m_mediaOutputStatus->secondsTotal = totalMinsExtraSecs % 60;

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

void mpg123Output::ProcessMP3Data(int bytesRead)
{
	int  bufferPtr = 0;
	char state = 0;
	int  i = 0;
	bool commandNext=false;

//#define NEW_MPG123
#if defined(NEW_MPG123) && !defined(USE_MPG321)
	// New verbose output format in v1.??, we key off the > at the beginning
	// > 0008+3784  00:00.18+01:38.84 --- 100=100 160 kb/s  522 B acc    0 clip p+0.000
	// States:
	// 123333455556677777777899999999
	m_mpg123_strTime[bufferPtr++] = ' ';
	m_mpg123_strTime[bufferPtr] = 0;
	for(i=0;i<bytesRead;i++)
	{
		switch(m_mp3Buffer[i])
		{
			case '-':
				state = 0;
				break;
			case '>':
				state = 1;
				break;
			case ' ':
				if (state == 1)
				{
					state = 2;
				}
				else if (state == 5)
				{
					state = 6;
				}
				else if (state == 6)
				{
					// Do nothing
				}
				else if (state == 9)
				{
					// Parse Times here
					m_mpg123_strTime[bufferPtr++] = ']';
					m_mpg123_strTime[bufferPtr] = 0;
					ParseTimes();
					bufferPtr = 1;
					state = 0;
					m_mpg123_strTime[bufferPtr] = 0;
				}
				else
				{
					state = 0;
				}
				break;
			case '+':
				if (state == 3)
				{
					state = 4;
				}
				else if (state == 7)
				{
					state = 8;
					m_mpg123_strTime[bufferPtr++] = ' ';
					m_mpg123_strTime[bufferPtr++] = '[';
					m_mpg123_strTime[bufferPtr] = 0;
				}
				else
				{
					state = 0;
				}
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case ':':
			case '.':
				if (state == 2)
				{
					state = 3;
				}
				else if (state == 3)
				{
					// Do nothing
				}
				else if (state == 4)
				{
					state = 5;
				}
				else if (state == 5)
				{
					// Do nothing
				}
				else if ((state == 6) || (state == 7) || (state == 8) || (state == 9))
				{
					if (state == 6)
						state = 7;
					else if (state == 8)
						state = 9;

					m_mpg123_strTime[bufferPtr++] = m_mp3Buffer[i];
					m_mpg123_strTime[bufferPtr] = 0;
				}
				else
				{
					state = 0;
				}
				break;
		}
	}
#else
	// Older mpg123 output format, we key off the capital T in Time.
	// Frame#   149 [ 1842], Time: 00:03.89 [00:48.11], RVA:   off, Vol: 100(100)
	for(i=0;i<bytesRead;i++)
	{
		switch(m_mp3Buffer[i])
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
					m_mpg123_strTime[bufferPtr++] = m_mp3Buffer[i];
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
					m_mpg123_strTime[bufferPtr++] = m_mp3Buffer[i];
					if(bufferPtr == 19)
					{
						ParseTimes();
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
#endif
}

void mpg123Output::PollMusicInfo(void)
{
	int bytesRead;
	int result;
	struct timeval mpg123_timeout;

	m_readFDSet = m_activeFDSet;

	mpg123_timeout.tv_sec = 0;
	mpg123_timeout.tv_usec = 5;

	if(select(FD_SETSIZE, &m_readFDSet, NULL, NULL, &mpg123_timeout) < 0)
	{
	 	LogErr(VB_MEDIAOUT, "Error Select:%d\n",errno);

		Stop(); // Kill the child if we can't read from the pipe
	 	return; 
	}
	if(FD_ISSET(m_childPipe[MEDIAOUTPUTPIPE_READ], &m_readFDSet))
	{
		bytesRead = read(m_childPipe[MEDIAOUTPUTPIPE_READ], m_mp3Buffer, MAX_BYTES_MP3);
		if (bytesRead > 0) 
		{
		    ProcessMP3Data(bytesRead);
		} 
	}
}

