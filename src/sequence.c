/*
 *   Sequence handler for Falcon Pi Player (FPP)
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
#include "channeloutputthread.h"
#include "controlsend.h"
#include "events.h"
#include "effects.h"
#include "fpp.h" // for FPPstatus && #define-d status values
#include "log.h"
#include "memorymap.h"
#include "sequence.h"
#include "settings.h"

FILE         *seqFile = NULL;
char          seqFilename[1024] = {'\x00'};
unsigned long seqFileSize = 0;
unsigned long seqFilePosition = 0;
int           seqVersionMajor = 0;
int           seqVersionMinor = 0;
int           seqVersion = 0;
int           seqChanDataOffset = 0;
int           seqFixedHeaderSize = 0;
int           seqStepSize = 8192;
int           seqStepTime = 50;
int           seqNumPeriods = 0;
int           seqRefreshRate = 20;
int           seqNumUniverses = 0;
int           seqUniverseSize = 0;
int           seqGamma = 0;
int           seqColorEncoding = 0;
int           seqDuration = 0;
int           seqSecondsElapsed = 0;
int           seqSecondsRemaining = 0;
char          seqData[FPPD_MAX_CHANNELS] __attribute__ ((aligned (__BIGGEST_ALIGNMENT__)));

char          seqLastControlMajor = 0;
char          seqLastControlMinor = 0;


/* prototypes for support functions below */
char NormalizeControlValue(char in);

/*
 *
 */

int OpenSequenceFile(const char *filename) {
	LogDebug(VB_SEQUENCE, "OpenSequenceFile(%s)\n", filename);

	if (!filename || !filename[0])
	{
		LogErr(VB_SEQUENCE, "Empty Sequence Filename!\n", filename);
		return 0;
	}

	size_t bytesRead = 0;

	seqFileSize = 0;

	if (IsSequenceRunning())
		CloseSequenceFile();

	seqDuration = 0;
	seqSecondsElapsed = 0;
	seqSecondsRemaining = 0;

	strcpy(seqFilename, filename);

	char tmpFilename[1024];
	unsigned char tmpData[2048];
	strcpy(tmpFilename,(const char *)getSequenceDirectory());
	strcat(tmpFilename,"/");
	strcat(tmpFilename, filename);

	seqFile = fopen((const char *)tmpFilename, "r");
	if (seqFile == NULL) 
	{
		LogErr(VB_SEQUENCE, "Error opening sequence file: %s. fopen returned NULL\n",
			tmpFilename);
		return 0;
	}

	if (getFPPmode() == MASTER_MODE)
	{
		SendSeqSyncStartPacket(filename);

		// Give the remotes a head start spining up so they are ready
		usleep(100000);
	}

	///////////////////////////////////////////////////////////////////////
	// Check 4-byte File format identifier
	char seqFormatID[5];
	strcpy(seqFormatID, "    ");
	bytesRead = fread(seqFormatID, 1, 4, seqFile);
	seqFormatID[4] = 0;
	if ((bytesRead != 4) || (strcmp(seqFormatID, "PSEQ")))
	{
		LogErr(VB_SEQUENCE, "Error opening sequence file: %s. Incorrect File Format header: '%s'.\n", filename, seqFormatID);
		return 0;
	}

	///////////////////////////////////////////////////////////////////////
	// Get Channel Data Offset
	bytesRead = fread(tmpData, 1, 2, seqFile);
	if (bytesRead != 2)
	{
		LogErr(VB_SEQUENCE, "Sequence file %s too short, unable to read channel data offset value\n", filename);
		return 0;
	}
	seqChanDataOffset = tmpData[0] + (tmpData[1] << 8);

	///////////////////////////////////////////////////////////////////////
	// Now that we know the header size, read the whole header in one shot
	fseek(seqFile, 0L, SEEK_SET);
	bytesRead = fread(tmpData, 1, seqChanDataOffset, seqFile);
	if (bytesRead != seqChanDataOffset)
	{
		LogErr(VB_SEQUENCE, "Sequence file %s too short, unable to read fixed header size value\n", filename);
		return 0;
	}

	seqVersionMinor = tmpData[6];
	seqVersionMajor = tmpData[7];
	seqVersion      = (seqVersionMajor * 256) + seqVersionMinor;

	seqFixedHeaderSize =
		(tmpData[8])        + (tmpData[9] << 8);

	seqStepSize =
		(tmpData[10])       + (tmpData[11] << 8) +
		(tmpData[12] << 16) + (tmpData[13] << 24);

	seqNumPeriods =
		(tmpData[14])       + (tmpData[15] << 8) +
		(tmpData[16] << 16) + (tmpData[17] << 24);

	seqStepTime =
		(tmpData[18])       + (tmpData[19] << 8);

	seqNumUniverses = 
		(tmpData[20])       + (tmpData[21] << 8);

	seqUniverseSize = 
		(tmpData[22])       + (tmpData[23] << 8);

	seqGamma         = tmpData[24];
	seqColorEncoding = tmpData[25];

	// End of v1.0 fields
	if (seqVersion > 0x0100)
	{
	}

	seqRefreshRate = 1000 / seqStepTime;

	fseek(seqFile, 0L, SEEK_END);
	seqFileSize = ftell(seqFile);
	seqDuration = (int)((float)(seqFileSize - seqChanDataOffset)
		/ ((float)seqStepSize * (float)seqRefreshRate));
	seqSecondsRemaining = seqDuration;
	fseek(seqFile, seqChanDataOffset, SEEK_SET);
	seqFilePosition = seqChanDataOffset;

	LogDebug(VB_SEQUENCE, "Sequence File Information\n");
	LogDebug(VB_SEQUENCE, "seqFilename           : %s\n", seqFilename);
	LogDebug(VB_SEQUENCE, "seqVersion            : %d.%d\n",
		seqVersionMajor, seqVersionMinor);
	LogDebug(VB_SEQUENCE, "seqFormatID           : %s\n", seqFormatID);
	LogDebug(VB_SEQUENCE, "seqChanDataOffset     : %d\n", seqChanDataOffset);
	LogDebug(VB_SEQUENCE, "seqFixedHeaderSize    : %d\n", seqFixedHeaderSize);
	LogDebug(VB_SEQUENCE, "seqStepSize           : %d\n", seqStepSize);
	LogDebug(VB_SEQUENCE, "seqNumPeriods         : %d\n", seqNumPeriods);
	LogDebug(VB_SEQUENCE, "seqStepTime           : %dms\n", seqStepTime);
	LogDebug(VB_SEQUENCE, "seqNumUniverses       : %d *\n", seqNumUniverses);
	LogDebug(VB_SEQUENCE, "seqUniverseSize       : %d *\n", seqUniverseSize);
	LogDebug(VB_SEQUENCE, "seqGamma              : %d *\n", seqGamma);
	LogDebug(VB_SEQUENCE, "seqColorEncoding      : %d *\n", seqColorEncoding);
	LogDebug(VB_SEQUENCE, "seqRefreshRate        : %d\n", seqRefreshRate);
	LogDebug(VB_SEQUENCE, "seqFileSize           : %lu\n", seqFileSize);
	LogDebug(VB_SEQUENCE, "seqDuration           : %d\n", seqDuration);
	LogDebug(VB_SEQUENCE, "'*' denotes field is currently ignored by FPP\n");

	ResetChannelOutputFrameNumber();

	ReadSequenceData();

	SetChannelOutputRefreshRate(seqRefreshRate);
	StartChannelOutputThread();

	return seqFileSize;
}

int SeekSequenceFile(int frameNumber) {
	LogDebug(VB_SEQUENCE, "SeekSequenceFile(%d)\n", frameNumber);

	if (!IsSequenceRunning())
	{
		LogErr(VB_SEQUENCE, "No sequence is running\n");
		return 0;
	}

	int newPos = seqChanDataOffset + (frameNumber * seqStepSize);
	LogDebug(VB_SEQUENCE, "Seeking to byte %d in %s\n", newPos, seqFilename);

	fseek(seqFile, newPos, SEEK_SET);

	ReadSequenceData();

	SetChannelOutputFrameNumber(frameNumber);
}


char *CurrentSequenceFilename(void) {
	return seqFilename;
}

inline int IsSequenceRunning(void) {
	if (seqFile)
		return 1;

	return 0;
}

void BlankSequenceData(void) {
	bzero(seqData, sizeof(seqData));
}

void ReadSequenceData(void) {
	size_t  bytesRead = 0;

	if (IsSequenceRunning())
	{
		bytesRead = 0;
		if(seqFilePosition < seqFileSize - seqStepSize)
		{
			bytesRead = fread(seqData, 1, seqStepSize, seqFile);
			seqFilePosition += bytesRead;
		}

		if (bytesRead != seqStepSize)
		{
			CloseSequenceFile();
		}

		seqSecondsElapsed = (int)((float)(seqFilePosition - seqChanDataOffset)/((float)seqStepSize*(float)seqRefreshRate));
		seqSecondsRemaining = seqDuration - seqSecondsElapsed;
	}
	else if ( getFPPmode() != BRIDGE_MODE )
	{
		BlankSequenceData();
	}

	if (IsEffectRunning())
		OverlayEffects(seqData);

	if (UsingMemoryMapInput())
		OverlayMemoryMap(seqData);

	if (getControlMajor() && getControlMinor())
	{
		char thisMajor = NormalizeControlValue(seqData[getControlMajor()-1]);
		char thisMinor = NormalizeControlValue(seqData[getControlMinor()-1]);

		if ((seqLastControlMajor != thisMajor) ||
			(seqLastControlMinor != thisMinor))
		{
			seqLastControlMajor = thisMajor;
			seqLastControlMinor = thisMinor;

			if (seqLastControlMajor && seqLastControlMinor)
				TriggerEvent(seqLastControlMajor, seqLastControlMinor);
		}
	}
}

void SendSequenceData(void) {
	SendChannelData(seqData);
}

void SendBlankingData(void) {
	LogDebug(VB_SEQUENCE, "Sending Blanking Data\n");
	usleep(100000);
	ReadSequenceData();
	SendSequenceData();
}

void CloseSequenceFile(void) {
	LogDebug(VB_SEQUENCE, "CloseSequenceFile() %s\n", seqFilename);

	if (getFPPmode() == MASTER_MODE)
		SendSeqSyncStopPacket(seqFilename);

	if (seqFile) {
		fclose(seqFile);
		seqFile = NULL;
	}

	seqFilename[0] = '\0';

	if (!IsEffectRunning() && (FPPstatus != FPP_STATUS_PLAYLIST_PLAYING))
		SendBlankingData();
}

/*
 * Normalize control channel values into buckets
 */
char NormalizeControlValue(char in) {
	char result = (char)(((unsigned char)in + 5) / 10);

	if (result == 26)
		return 25;

	return result;
}

