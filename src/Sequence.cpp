/*
 *   Sequence Class for Falcon Pi Player (FPP)
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
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "E131.h"
#include "common.h"
#include "controlsend.h"
#include "events.h"
#include "effects.h"
#include "fpp.h" // for FPPstatus && #define-d status values
#include "fppd.h"
#include "log.h"
#include "PixelOverlay.h"
#include "Player.h"
#include "Sequence.h"
#include "settings.h"

Sequence::Sequence(int priority, int startChannel, int blockSize)
  : m_sequenceID(0),
	m_seqFileSize(0),
	m_seqDuration(0),
	m_seqSecondsElapsed(0),
	m_seqSecondsRemaining(0),
	m_seqFile(NULL),
	m_seqFilePosition(0),
	m_priority(0),
	m_autoRepeat(0),
	m_startChannel(startChannel),
	m_blockSize(blockSize),
	m_seqStarting(0),
	m_seqPaused(0),
	m_seqSingleStep(0),
	m_seqSingleStepBack(0),
	m_seqVersionMajor(0),
	m_seqVersionMinor(0),
	m_seqVersion(0),
	m_seqChanDataOffset(0),
	m_seqFixedHeaderSize(0),
	m_seqStepSize(8192),
	m_seqStepTime(50),
	m_seqNumPeriods(0),
	m_seqRefreshRate(20),
	m_seqNumUniverses(0),
	m_seqUniverseSize(0),
	m_seqGamma(0),
	m_seqColorEncoding(0),
	m_seqLastControlMajor(0),
	m_seqLastControlMinor(0)
{
	m_seqFilename[0] = 0;

	pthread_mutex_init(&m_sequenceLock, NULL);
}

Sequence::~Sequence()
{
	pthread_mutex_destroy(&m_sequenceLock);
}

/*
 *
 */

int Sequence::OpenSequenceFile(std::string filename, int startSeconds)
{
	LogDebug(VB_SEQUENCE, "OpenSequenceFile(%s, %d)\n",
		filename.c_str(), startSeconds);

	if (filename == "")
	{
		LogErr(VB_SEQUENCE, "Empty Sequence Filename!\n");
		return 0;
	}

	size_t bytesRead = 0;

	m_seqFileSize = 0;

	if (m_seqFile)
		CloseSequenceFile();

	m_seqStarting = 1;
	m_seqPaused   = 0;
	m_seqDuration = 0;
	m_seqSecondsElapsed = 0;
	m_seqSecondsRemaining = 0;

	strcpy(m_seqFilename, filename.c_str());

	char tmpFilename[2048];
	unsigned char tmpData[2048];
	strcpy(tmpFilename,(const char *)getSequenceDirectory());
	strcat(tmpFilename,"/");
	strcat(tmpFilename, filename.c_str());

	if (getFPPmode() == REMOTE_MODE)
		CheckForHostSpecificFile(getSetting("HostName"), tmpFilename);

	if (!FileExists(tmpFilename))
	{
		LogErr(VB_SEQUENCE, "Sequence file %s does not exist\n", tmpFilename);
		m_seqStarting = 0;
		return 0;
	}

	m_seqFile = fopen((const char *)tmpFilename, "r");
	if (m_seqFile == NULL) 
	{
		LogErr(VB_SEQUENCE, "Error opening sequence file: %s. fopen returned NULL\n",
			tmpFilename);
		m_seqStarting = 0;
		return 0;
	}

	if (getFPPmode() == MASTER_MODE)
	{
		SendSeqSyncStartPacket(filename.c_str());

		// Give the remotes a head start spining up so they are ready
		usleep(100000);
	}

	///////////////////////////////////////////////////////////////////////
	// Check 4-byte File format identifier
	char seqFormatID[5];
	strcpy(seqFormatID, "    ");
	bytesRead = fread(seqFormatID, 1, 4, m_seqFile);
	seqFormatID[4] = 0;
	if ((bytesRead != 4) || (strcmp(seqFormatID, "PSEQ") && strcmp(seqFormatID, "FSEQ")))
	{
		LogErr(VB_SEQUENCE, "Error opening sequence file: %s. Incorrect File Format header: '%s', bytesRead: %d\n",
			filename.c_str(), seqFormatID, bytesRead);

		fseek(m_seqFile, 0L, SEEK_SET);
		bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, m_seqFile);
		HexDump("Sequence File head:", tmpData, bytesRead);

		fclose(m_seqFile);
		m_seqFile = NULL;
		m_seqStarting = 0;
		return 0;
	}

	///////////////////////////////////////////////////////////////////////
	// Get Channel Data Offset
	bytesRead = fread(tmpData, 1, 2, m_seqFile);
	if (bytesRead != 2)
	{
		LogErr(VB_SEQUENCE, "Sequence file %s too short, unable to read channel data offset value\n", filename.c_str());

		fseek(m_seqFile, 0L, SEEK_SET);
		bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, m_seqFile);
		HexDump("Sequence File head:", tmpData, bytesRead);

		fclose(m_seqFile);
		m_seqFile = NULL;
		m_seqStarting = 0;
		return 0;
	}
	m_seqChanDataOffset = tmpData[0] + (tmpData[1] << 8);

	///////////////////////////////////////////////////////////////////////
	// Now that we know the header size, read the whole header in one shot
	fseek(m_seqFile, 0L, SEEK_SET);
	bytesRead = fread(tmpData, 1, m_seqChanDataOffset, m_seqFile);
	if (bytesRead != m_seqChanDataOffset)
	{
		LogErr(VB_SEQUENCE, "Sequence file %s too short, unable to read fixed header size value\n", filename.c_str());

		fseek(m_seqFile, 0L, SEEK_SET);
		bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, m_seqFile);
		HexDump("Sequence File head:", tmpData, bytesRead);

		fclose(m_seqFile);
		m_seqFile = NULL;
		m_seqStarting = 0;
		return 0;
	}

	m_seqVersionMinor = tmpData[6];
	m_seqVersionMajor = tmpData[7];
	m_seqVersion      = (m_seqVersionMajor * 256) + m_seqVersionMinor;

	m_seqFixedHeaderSize =
		(tmpData[8])        + (tmpData[9] << 8);

	m_seqStepSize =
		(tmpData[10])       + (tmpData[11] << 8) +
		(tmpData[12] << 16) + (tmpData[13] << 24);

	m_seqNumPeriods =
		(tmpData[14])       + (tmpData[15] << 8) +
		(tmpData[16] << 16) + (tmpData[17] << 24);

	m_seqStepTime =
		(tmpData[18])       + (tmpData[19] << 8);

	m_seqNumUniverses = 
		(tmpData[20])       + (tmpData[21] << 8);

	m_seqUniverseSize = 
		(tmpData[22])       + (tmpData[23] << 8);

	m_seqGamma         = tmpData[24];
	m_seqColorEncoding = tmpData[25];

	// End of v1.0 fields
	if (m_seqVersion > 0x0100)
	{
	}

	m_seqRefreshRate = 1000 / m_seqStepTime;

	fseek(m_seqFile, 0L, SEEK_END);
	m_seqFileSize = ftell(m_seqFile);
	m_seqDuration = (int)((float)(m_seqFileSize - m_seqChanDataOffset)
		/ ((float)m_seqStepSize * (float)m_seqRefreshRate));
	m_seqSecondsRemaining = m_seqDuration;
	fseek(m_seqFile, m_seqChanDataOffset, SEEK_SET);
	m_seqFilePosition = m_seqChanDataOffset;

	LogDebug(VB_SEQUENCE, "Sequence File Information\n");
	LogDebug(VB_SEQUENCE, "seqFilename           : %s\n", m_seqFilename);
	LogDebug(VB_SEQUENCE, "seqVersion            : %d.%d\n",
		m_seqVersionMajor, m_seqVersionMinor);
	LogDebug(VB_SEQUENCE, "seqFormatID           : %s\n", seqFormatID);
	LogDebug(VB_SEQUENCE, "seqChanDataOffset     : %d\n", m_seqChanDataOffset);
	LogDebug(VB_SEQUENCE, "seqFixedHeaderSize    : %d\n", m_seqFixedHeaderSize);
	LogDebug(VB_SEQUENCE, "seqStepSize           : %d\n", m_seqStepSize);
	LogDebug(VB_SEQUENCE, "seqNumPeriods         : %d\n", m_seqNumPeriods);
	LogDebug(VB_SEQUENCE, "seqStepTime           : %dms\n", m_seqStepTime);
	LogDebug(VB_SEQUENCE, "seqNumUniverses       : %d *\n", m_seqNumUniverses);
	LogDebug(VB_SEQUENCE, "seqUniverseSize       : %d *\n", m_seqUniverseSize);
	LogDebug(VB_SEQUENCE, "seqGamma              : %d *\n", m_seqGamma);
	LogDebug(VB_SEQUENCE, "seqColorEncoding      : %d *\n", m_seqColorEncoding);
	LogDebug(VB_SEQUENCE, "seqRefreshRate        : %d\n", m_seqRefreshRate);
	LogDebug(VB_SEQUENCE, "seqFileSize           : %lu\n", m_seqFileSize);
	LogDebug(VB_SEQUENCE, "seqDuration           : %d\n", m_seqDuration);
	LogDebug(VB_SEQUENCE, "'*' denotes field is currently ignored by FPP\n");

	m_seqPaused = 0;
	m_seqSingleStep = 0;
	m_seqSingleStepBack = 0;

	if (m_startChannel >= m_seqStepSize)
		m_startChannel = 0;

	if (m_blockSize == 0)
		m_blockSize = -1;

	if ((m_startChannel) || (m_blockSize > -1))
	{
		if ((m_startChannel + m_blockSize) > m_seqStepSize)
			m_blockSize = m_seqStepSize - m_startChannel;
	}
	else
	{
		m_blockSize = m_seqStepSize;
	}

	int frameNumber = 0;

	if (startSeconds)
	{
		int frameNumber = startSeconds * m_seqRefreshRate;
		int newPos = m_seqChanDataOffset + (frameNumber * m_seqStepSize);
		LogDebug(VB_SEQUENCE, "Seeking to byte %d in %s\n", newPos, m_seqFilename);

		fseek(m_seqFile, newPos, SEEK_SET);
	}

	if (!ReadSequenceData())
		return 0;

	SetChannelOutputFrameNumber(frameNumber);

	m_seqStarting = 0;

	return m_seqFileSize;
}

int Sequence::SeekSequenceFile(int frameNumber) {
	LogDebug(VB_SEQUENCE, "SeekSequenceFile(%d)\n", frameNumber);

	if (!m_seqFile)
	{
		LogErr(VB_SEQUENCE, "No sequence is running\n");
		return 0;
	}

	int newPos = m_seqChanDataOffset + (frameNumber * m_seqStepSize);
	LogDebug(VB_SEQUENCE, "Seeking to byte %d in %s\n", newPos, m_seqFilename);

	fseek(m_seqFile, newPos, SEEK_SET);

	if (!ReadSequenceData())
		return 0;

	SetChannelOutputFrameNumber(frameNumber);

	return 1;
}

void Sequence::OverlayNextFrame(char *outputBuffer)
{
	memcpy(outputBuffer + m_startChannel, m_seqData + m_startChannel,
		m_blockSize);
}

char *Sequence::CurrentSequenceFilename(void)
{
	return m_seqFilename;
}

void Sequence::SetPauseState(int pause)
{
	m_seqPaused = pause;
}

int Sequence::SequenceIsPaused(void)
{
	return m_seqPaused;
}

void Sequence::ToggleSequencePause(void)
{
	if (m_seqPaused)
		m_seqPaused = 0;
	else
		m_seqPaused = 1;
}

void Sequence::SingleStepSequence(void)
{
	m_seqSingleStep = 1;
}

void Sequence::SingleStepSequenceBack(void)
{
	m_seqSingleStepBack = 1;
}

int Sequence::ReadSequenceData(void) {
	LogDebug(VB_SEQUENCE, "ReadSequenceData()\n");
	size_t  bytesRead = 0;

	if (m_seqStarting)
		return 1;

	if (m_seqPaused)
	{
		if (m_seqSingleStep)
		{
			m_seqSingleStep = 0;
		}
		else if (m_seqSingleStepBack)
		{
			m_seqSingleStepBack = 0;

			int offset = m_seqStepSize * 2;
			if (m_seqFilePosition > offset)
				fseek(m_seqFile, 0 - offset, SEEK_CUR);
		}
		else
		{
			return 1;
		}
	}

	if (m_seqFile)
	{
		bytesRead = 0;
		if(m_seqFilePosition < m_seqFileSize - m_seqStepSize)
		{
			bytesRead = fread(m_seqData, 1, m_seqStepSize, m_seqFile);
			m_seqFilePosition += bytesRead;
		}

		if (bytesRead != m_seqStepSize)
		{
			if ((m_autoRepeat) &&
				(m_seqFilePosition > (m_seqChanDataOffset + m_seqStepSize)))
			{
LogDebug(VB_SEQUENCE, "Sequence hit EOF and is set to repeat, looping\n");
LogDebug(VB_SEQUENCE, "m_seqFilePosition: %d\n", m_seqFilePosition);
				fseek(m_seqFile, m_seqChanDataOffset, SEEK_SET);
				m_seqFilePosition = m_seqChanDataOffset;

				bytesRead = fread(m_seqData, 1, m_seqStepSize, m_seqFile);
				m_seqFilePosition += bytesRead;
LogDebug(VB_SEQUENCE, "m_seqFilePosition: %d\n", m_seqFilePosition);
			}
			else
			{
				CloseSequenceFile();
				return 0;
			}
		}

		m_seqSecondsElapsed = (int)((float)(m_seqFilePosition - m_seqChanDataOffset)/((float)m_seqStepSize*(float)m_seqRefreshRate));
		m_seqSecondsRemaining = m_seqDuration - m_seqSecondsElapsed;
	}

	return 1;
}

void Sequence::CloseSequenceFile(void) {
	LogDebug(VB_SEQUENCE, "CloseSequenceFile() %s\n", m_seqFilename);

	if (getFPPmode() == MASTER_MODE)
		SendSeqSyncStopPacket(m_seqFilename);

	pthread_mutex_lock(&m_sequenceLock);

	if (m_seqFile) {
		fclose(m_seqFile);
		m_seqFile = NULL;
	}

	m_seqFilename[0] = '\0';
	m_seqPaused = 0;

	// FIXME PLAYLIST, figure out how to do this in the player itself
//	if ((!IsEffectRunning()) &&
//		((getFPPmode() != REMOTE_MODE) &&
//		 (FPPstatus != FPP_STATUS_PLAYLIST_PLAYING)) ||
//		(getSettingInt("blankBetweenSequences")))
//		SendBlankingData();

	pthread_mutex_unlock(&m_sequenceLock);
}

