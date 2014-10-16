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

#define FSEQ_STEP_SIZE_OFFSET      10
#define FSEQ_STEP_TIME_OFFSET      18
#define FSEQ_CHANNEL_DATA_OFFSET   28

FILE         *seqFile = NULL;
char          seqFilename[1024] = {'\x00'};
unsigned long seqFileSize = 0;
unsigned long seqFilePosition = 0;
int           seqPaused = 0;
int           seqStepSize = 8192;
int           seqStepTime = 50;
int           seqRefreshRate = 20;
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

	// Get Step Size
	fseek(seqFile, FSEQ_STEP_SIZE_OFFSET, SEEK_SET);
	bytesRead=fread(seqData, 1, 4, seqFile);
	seqStepSize = seqData[0] +
		(seqData[1] << 8) + (seqData[2] << 16) + (seqData[3] << 24);

	// Get Step Time
	fseek(seqFile, FSEQ_STEP_TIME_OFFSET, SEEK_SET);
	bytesRead=fread(seqData, 1, 2, seqFile);
	seqStepTime = seqData[0] + (seqData[1] << 8);
	seqRefreshRate = 1000 / seqStepTime;

	fseek(seqFile, 0L, SEEK_END);
	seqFileSize = ftell(seqFile);
	seqDuration = (int)((float)(seqFileSize - FSEQ_CHANNEL_DATA_OFFSET)
		/ ((float)seqStepSize * (float)seqRefreshRate));
	seqSecondsRemaining = seqDuration;
	fseek(seqFile, FSEQ_CHANNEL_DATA_OFFSET, SEEK_SET);
	seqFilePosition = FSEQ_CHANNEL_DATA_OFFSET;

	LogDebug(VB_SEQUENCE, "seqStepSize: %d\n", seqStepSize);
	LogDebug(VB_SEQUENCE, "seqStepTime: %dms\n", seqStepTime);
	LogDebug(VB_SEQUENCE, "seqRefreshRate: %d\n", seqRefreshRate);
	LogDebug(VB_SEQUENCE, "seqFileSize: %lu\n", seqFileSize);
	LogDebug(VB_SEQUENCE, "seqDuration: %d\n", seqDuration);

	seqPaused = 0;

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

	int newPos = FSEQ_CHANNEL_DATA_OFFSET + (frameNumber * seqStepSize);
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

int SequenceIsPaused(void) {
	return seqPaused;
}

void ToggleSequencePause(void) {
	if (seqPaused)
		seqPaused = 0;
	else
		seqPaused = 1;
}

void ReadSequenceData(void) {
	size_t  bytesRead = 0;

	if (seqPaused)
		return;

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

		seqSecondsElapsed = (int)((float)(seqFilePosition - FSEQ_CHANNEL_DATA_OFFSET)/((float)seqStepSize*(float)seqRefreshRate));
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

