/*
 *   omxplayer driver for Falcon Player (FPP)
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
#include "log.h"
#include "MultiSync.h"
#include "omxplayer.h"
#include "mediaoutput.h"
#include "settings.h"
#include "Sequence.h"

#define MAX_BYTES_OMX 4096

/*
 *
 */
omxplayerOutput::omxplayerOutput(std::string mediaFilename, MediaOutputStatus *status)
{
	LogDebug(VB_MEDIAOUT, "omxplayerOutput::omxplayerOutput(%s)\n", mediaFilename.c_str());

	m_mediaFilename = mediaFilename;
	m_mediaOutputStatus = status;
    m_allowSpeedAdjust = true;

    m_omxBuffer = new char[MAX_BYTES_OMX + 1];
}

/*
 *
 */
omxplayerOutput::~omxplayerOutput()
{
    delete [] m_omxBuffer;
}

/*
 *
 */
int omxplayerOutput::Start(void)
{
	std::string fullVideoPath;
    m_beforeFirstTick = true;

	LogDebug(VB_MEDIAOUT, "omxplayerOutput::Start()\n");

	bzero(m_mediaOutputStatus, sizeof(MediaOutputStatus));

	fullVideoPath = getVideoDirectory();
	fullVideoPath += "/";
	fullVideoPath += m_mediaFilename;

	if (getFPPmode() == REMOTE_MODE)
	{
		char tmpPath[2048];
		strcpy(tmpPath, fullVideoPath.c_str());

		if (CheckForHostSpecificFile(getSetting("HostName"), tmpPath))
			fullVideoPath = tmpPath;
	}

	if (!FileExists(fullVideoPath.c_str()))
	{
		if (getFPPmode() != REMOTE_MODE)
			LogErr(VB_MEDIAOUT, "%s does not exist!\n", fullVideoPath.c_str());

		return 0;
	}

	// Create Pipes to/from omxplayer
	pid_t omxplayerPID = forkpty(&m_childPipe[0], 0, 0, 0);
	if (omxplayerPID == 0)			// omxplayer process
	{
		CloseOpenFiles();

		seteuid(1000); // 'pi' user

		execl("/opt/fpp/scripts/omxplayer", "/opt/fpp/scripts/omxplayer",
			fullVideoPath.c_str(), NULL);

		LogErr(VB_MEDIAOUT, "omxplayer_StartPlaying(), ERROR, we shouldn't "
			"be here, this means that execl() failed\n");

		exit(EXIT_FAILURE);
	}
	else							// Parent process
	{
		m_childPID = omxplayerPID;
	}

	// Clear active file descriptor sets
	FD_ZERO (&m_activeFDSet);
	// Set description for reading from omxplayer
	FD_SET (m_childPipe[0], &m_activeFDSet);

	m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_PLAYING;

	m_volumeShift = GetVolumeShift(getVolume());

	return 1;
}

/*
 *
 */
int omxplayerOutput::GetVolumeShift(int volume)
{
	volume = 50 + (volume / 2);

	return (volume - 87.5) / 2.5;
}

/*
 *
 */
int omxplayerOutput::AdjustSpeed(int delta)
{
    if (m_allowSpeedAdjust) {
        if (delta < 0) {
            LogDebug(VB_MEDIAOUT, "Slowing Down playback\n");
            write(m_childPipe[0], "8", 1);
        } else if (delta > 0) {
            LogDebug(VB_MEDIAOUT, "Speeding Up playback\n");
            write(m_childPipe[0], "0", 1);
        } else {
            LogDebug(VB_MEDIAOUT, "Speed Playback Normal\n");
            write(m_childPipe[0], "9", 1);
        }
	}
	return 1;
}

/*
 *
 */
void omxplayerOutput::SetVolume(int volume)
{
	int newShift = GetVolumeShift(volume);
	int diff = newShift - m_volumeShift;

	LogDebug(VB_MEDIAOUT, "omxplayer_SetVolume(%d): diff: %d\n", volume, diff);

	if (!diff)
		return;

	while (diff > 0)
	{
		write(m_childPipe[0], "=", 1);
		diff--;
	}

	while (diff < 0)
	{
		write(m_childPipe[0], "-", 1);
		diff++;
	}

	m_volumeShift = newShift;
}

/*
 *
 */
void omxplayerOutput::ProcessPlayerData(char *omxBuffer, int bytesRead)
{
	int        ticks = 0;
	int        mins = 0;
	int        secs = 0;
	int        subsecs = 0;
	int        totalCentiSecs = 0;
	char      *ptr = NULL;
    bool hasDuration = false;
    
	if ((m_mediaOutputStatus->secondsTotal == 0) &&
		(m_mediaOutputStatus->minutesTotal == 0) &&
		(ptr = strstr(omxBuffer, "Duration: ")))
	{
		// Sample line format:
		// (whitespace)Duration: 00:00:37.91, start: 0.000000, bitrate: 2569 kb/s
        ptr = ptr + 10;
        int hours = strtol(ptr, NULL, 10);
        ptr += 3;
        m_mediaOutputStatus->minutesTotal = strtol(ptr, NULL, 10) + (hours * 60);
        ptr += 3;
        m_mediaOutputStatus->secondsTotal = strtol(ptr, NULL, 10);
        hasDuration = true;
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
        m_beforeFirstTick = false;
    } else if ((!strncmp(omxBuffer, "have a nice day", 15)) ||
			   ((!strncmp(omxBuffer, "Did not receive", 15)) &&
				(strstr(omxBuffer, "have a nice day")))) {
        //hit the end
        m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;
        m_mediaOutputStatus->secondsRemaining = 0;
        Stop();
        return;
    } else if (!strncmp(omxBuffer, "V:", 2)) {
        //video settings, informational only, ignore
	} else {
        if (!hasDuration && bytesRead > 2 && !m_beforeFirstTick) {
            LogErr(VB_MEDIAOUT, "Error parsing omxplayer output.  %s\n", omxBuffer);
        }
		return;
	}

	totalCentiSecs = ticks / 10000;
	mins = totalCentiSecs / 6000;
	secs = totalCentiSecs % 6000 / 100;
	subsecs = totalCentiSecs % 100;

	m_mediaOutputStatus->secondsElapsed = 60 * mins + secs;

	m_mediaOutputStatus->subSecondsElapsed = subsecs;

	m_mediaOutputStatus->secondsRemaining = (m_mediaOutputStatus->minutesTotal * 60)
		+ m_mediaOutputStatus->secondsTotal - m_mediaOutputStatus->secondsElapsed;
	// FIXME, can we get this?
	// m_mediaOutputStatus->subSecondsRemaining = subsecs;

	m_mediaOutputStatus->mediaSeconds = (float)((float)m_mediaOutputStatus->secondsElapsed + ((float)m_mediaOutputStatus->subSecondsElapsed/(float)100));
}

/*
 *
 */
void omxplayerOutput::PollPlayerInfo(void)
{
	int bytesRead;
	int result;
	struct timeval omx_timeout;

	m_readFDSet = m_activeFDSet;

	omx_timeout.tv_sec = 0;
	omx_timeout.tv_usec = 5;

	if(select(FD_SETSIZE, &m_readFDSet, NULL, NULL, &omx_timeout) < 0) {
	 	LogErr(VB_MEDIAOUT, "Error Select:%d\n", errno);

		Stop(); // Kill the child if we can't read from the pipe
	 	return; 
	}
	if(FD_ISSET(m_childPipe[0], &m_readFDSet)) {
 		bytesRead = read(m_childPipe[0], m_omxBuffer, MAX_BYTES_OMX );
		if (bytesRead > 0) {
            int cur = 0;
            int max = 0;
            for (int m = 0; m < bytesRead; m++) {
                if (m_omxBuffer[m] == '\n' || m_omxBuffer[m] == '\r') {
                    m_omxBuffer[m] = 0;
                    ProcessPlayerData(&m_omxBuffer[cur], m - cur);
                    cur = m + 1;
                }
            }
            if (cur < bytesRead) {
                ProcessPlayerData(&m_omxBuffer[cur], bytesRead - cur);
            }
            
            if (getFPPmode() == MASTER_MODE) {
                multiSync->SendMediaSyncPacket(m_mediaFilename.c_str(), 0,
                                               m_mediaOutputStatus->mediaSeconds);
            }
            
            if ((sequence->IsSequenceRunning()) &&
                (m_mediaOutputStatus->secondsElapsed > 0)) {
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
	}
}

int omxplayerOutput::Process(void)
{
	if(m_childPID > 0) {
		PollPlayerInfo();
	}

	return m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_PLAYING;
}

int omxplayerOutput::Stop(void)
{
	m_mediaOutputStatus->status = MEDIAOUTPUTSTATUS_IDLE;

	if (!m_childPID)
		return 0;

    write(m_childPipe[0], "q", 1);

	LogDebug(VB_MEDIAOUT, "Stop(), killing pid %d\n", m_childPID);

	kill(m_childPID, SIGKILL);
	m_childPID = 0;

	// omxplayer is a shell script wrapper around omxplayer.bin and
	// killing the PID of the schell script doesn't kill the child
	// for some reason, so use this hack for now.
	system("killall -9 omxplayer.bin");

	return 1;
}

int omxplayerOutput::IsPlaying(void)
{
    return m_mediaOutputStatus->status == MEDIAOUTPUTSTATUS_PLAYING;
}
int omxplayerOutput::Close(void) {
    Stop();
    return 0;
}
