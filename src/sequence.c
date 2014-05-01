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

#include "channeloutput/E131.h"
#include "./channeloutput/channeloutputthread.h"
#include "events.h"
#include "effects.h"
#include "fpp.h" // for FPPstatus && #define-d status values
#include "log.h"
#include "memorymap.h"
#include "sequence.h"
#include "settings.h"

#define FSEQ_STEP_SIZE_OFFSET      10
#define FSEQ_CHANNEL_DATA_OFFSET   28

FILE         *seqFile = NULL;
char          seqFilename[1024];
unsigned long seqFileSize = 0;
unsigned long seqFilePosition = 0;
int           seqStepSize = 8192;
int           seqDuration = 0;
int           seqSecondsElapsed = 0;
int           seqSecondsRemaining = 0;
char          seqData[FPPD_MAX_CHANNELS];

char          seqLastControlMajor = 0;
char          seqLastControlMinor = 0;


/* prototypes for support functions below */
char NormalizeControlValue(char in);

/*
 *
 */

int OpenSequenceFile(const char *filename) {
	LogDebug(VB_SEQUENCE, "OpenSequenceFile(%s)\n", filename);

	size_t bytesRead = 0;

	seqFileSize = 0;

	if (IsSequenceRunning())
		CloseSequenceFile();

	seqDuration = 0;
	seqSecondsElapsed = 0;
	seqSecondsRemaining = 0;

	strcpy(seqFilename,(const char *)getSequenceDirectory());
	strcat(seqFilename,"/");
	strcat(seqFilename, filename);

	seqFile = fopen((const char *)seqFilename, "r");
	if (seqFile == NULL) 
	{
		LogErr(VB_SEQUENCE, "Error opening sequence file: %s fopen returned NULL\n",
			seqFilename);
		return 0;
	}

	// Get Step Size
	fseek(seqFile, FSEQ_STEP_SIZE_OFFSET, SEEK_SET);
	bytesRead=fread(seqData, 1, 4, seqFile);
	seqStepSize = seqData[0] +
		(seqData[1] << 8) + (seqData[2] << 16) + (seqData[3] << 24);

	fseek(seqFile, 0L, SEEK_END);
	seqFileSize = ftell(seqFile);
	seqDuration = (int)((float)(seqFileSize - FSEQ_CHANNEL_DATA_OFFSET)
		/ ((float)seqStepSize * (float)20));
	seqSecondsRemaining = seqDuration;
	fseek(seqFile, FSEQ_CHANNEL_DATA_OFFSET, SEEK_SET);
	seqFilePosition = FSEQ_CHANNEL_DATA_OFFSET;

	LogDebug(VB_SEQUENCE, "seqStepSize: %d\n", seqStepSize);
	LogDebug(VB_SEQUENCE, "seqFileSize: %lu\n", seqFileSize);

	ResetChannelOutputFrameNumber();

	ReadSequenceData();
	StartChannelOutputThread();

	return seqFileSize;
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

		if (bytesRead != seqStepSize)
		{
			CloseSequenceFile();
		}

		seqSecondsElapsed = (int)((float)(seqFilePosition - FSEQ_CHANNEL_DATA_OFFSET)/((float)seqStepSize*(float)20.0));
		seqSecondsRemaining = seqDuration - seqSecondsElapsed;
	}
	else
	{
		BlankSequenceData();
	}

	if (IsEffectRunning())
		OverlayEffects(seqData);

	if (UsingMemoryMapInput())
		OverlayMemoryMap(seqData);
}

void SendSequenceData(void) {
	SendChannelData(seqData);
}

void SendBlankingData(void) {
	LogDebug(VB_SEQUENCE, "Sending Blanking Data\n");
	ReadSequenceData();
	SendSequenceData();
}

void CloseSequenceFile(void) {
	LogDebug(VB_SEQUENCE, "CloseSequenceFile() %s\n", seqFilename);

	if (seqFile) {
		fclose(seqFile);
		seqFile = NULL;
	}
	strcpy(seqFilename, "");

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

