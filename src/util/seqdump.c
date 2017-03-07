/*
 *   Debug Utility for Falcon Pi Player (FPP)
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

#define FPPD_MAX_CHANNELS 131072

#define ESEQ_MODEL_COUNT_OFFSET     4 // hard-coded to 1 for now in Nutcracker
#define ESEQ_STEP_SIZE_OFFSET       8 // total step size of all models together
#define ESEQ_MODEL_START_OFFSET    12 // with current 1 model per file ability
#define ESEQ_MODEL_SIZE_OFFSET     16 // with current 1 model per file ability
#define ESEQ_CHANNEL_DATA_OFFSET   20 // with current 1 model per file ability


FILE         *seqFile = NULL;
char          seqFilename[1024] = {'\x00'};
unsigned long seqFileSize = 0;
unsigned long seqFilePosition = 0;
int           seqStartChannel = 0;
int           seqStarting = 0;
int           seqPaused = 0;
int           seqSingleStep = 0;
int           seqSingleStepBack = 0;
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

#define DATA_DUMP_SIZE  28


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <unistd.h>

#include "../common.h"

/*
 * Dump memory block in hex and human-readable formats
 */
void HexDump(char *title, void *data, int len) {
	int l = 0;
	int i = 0;
	int x = 0;
	int y = 0;
	unsigned char *ch = (unsigned char *)data;
	unsigned char str[17];
	char tmpStr[90];

	sprintf( tmpStr, "%s: (%d bytes)\n", title, len);
	printf("%s",tmpStr);

	while (l < len) {
		if ( x == 0 ) {
			sprintf( tmpStr, "       %06x: ", i );
		}

		if ( x < 16 ) {
			sprintf( tmpStr + strlen(tmpStr), "%02x ", *ch & 0xFF );
			str[x] = *ch;
			x++;
			i++;
		} else {
			sprintf( tmpStr + strlen(tmpStr), "   " );
			for( ; x > 0 ; x-- ) {
				if ( isgraph( str[16-x] ) || str[16-x] == ' ' ) {
					sprintf( tmpStr + strlen(tmpStr), "%c", str[16-x]);
				} else {
					sprintf( tmpStr + strlen(tmpStr), "." );
				}
			}

			sprintf( tmpStr + strlen(tmpStr), "\n" );
			printf("%s",tmpStr);
			x = 0;

			sprintf( tmpStr, "       %06x: ", i );
			sprintf( tmpStr + strlen(tmpStr), "%02x ", *ch & 0xFF );
			str[x] = *ch;
			x++;
			i++;
		}

		l++;
		ch++;
	}
	for( y = x; y < 16 ; y++ ) {
		sprintf( tmpStr + strlen(tmpStr), "   " );
	}
	sprintf( tmpStr + strlen(tmpStr), "   " );
	for( y = 0; y < x ; y++ ) {
		if ( isgraph( str[y] ) || str[y] == ' ' ) {
			sprintf( tmpStr + strlen(tmpStr), "%c", str[y]);
		} else {
			sprintf( tmpStr + strlen(tmpStr), "." );
		}
	}

	sprintf( tmpStr + strlen(tmpStr), "\n" );
	printf("%s",tmpStr);
}

/*
 *
 */
int OpenEffectFile(const char *filename)
{
	printf("OpenEffectFile(%s)\n", filename);

	if (!filename || !filename[0])
	{
		printf("Empty Effect Filename! (%s)\n", filename);
		return 0;
	}

	size_t bytesRead = 0;

	seqFileSize = 0;
	seqStarting = 1;
	seqPaused   = 0;
	seqDuration = 0;
	seqSecondsElapsed = 0;
	seqSecondsRemaining = 0;

	strcpy(seqFilename, filename);

	unsigned char tmpData[2048];
	char seqFormatID[5];
	strcpy(seqFormatID, "ESEQ");


	// Should already be at this position, but seek any to future-proof
	fseek(seqFile, ESEQ_MODEL_COUNT_OFFSET, SEEK_SET);
	bytesRead = fread(tmpData,1,1,seqFile);
	if (bytesRead < 1)
	{
		printf("Unable to load effect: %s\n", filename);
		return 0;
	}

	int modelCount = tmpData[0];

	if (modelCount > 1)
	{
		printf("This version of FPP does not support effect files with more than one model.");
		return 0;
	}

	// This will need to change if/when we support multiple models per file
	fseek(seqFile, ESEQ_MODEL_START_OFFSET, SEEK_SET);
	bytesRead = fread(tmpData,1,4,seqFile);
	if (bytesRead < 4)
	{
		printf("Unable to load effect: %s\n", filename);
		return 0;
	}

	seqStartChannel = tmpData[0] + (tmpData[1]<<8) + (tmpData[2]<<16) + (tmpData[3]<<24);

	// This will need to change if/when we support multiple models per file
	fseek(seqFile, ESEQ_MODEL_SIZE_OFFSET, SEEK_SET);
	bytesRead = fread(tmpData,1,4,seqFile);
	if (bytesRead < 4)
	{
		printf("Unable to load effect: %s\n", filename);
		return 0;
	}

    seqStepSize = tmpData[0] + (tmpData[1]<<8) + (tmpData[2]<<16) + (tmpData[3]<<24);

	seqChanDataOffset = ESEQ_CHANNEL_DATA_OFFSET;

	if (fseek(seqFile, ESEQ_CHANNEL_DATA_OFFSET, SEEK_SET))
	{
		printf("Unable to load effect: %s\n", filename);
		return 0;
	}

	fseek(seqFile, 0L, SEEK_END);
	seqFileSize = ftell(seqFile);
	seqDuration = (int)((float)(seqFileSize - seqChanDataOffset)
		/ ((float)seqStepSize * (float)seqRefreshRate));
	seqSecondsRemaining = seqDuration;
	fseek(seqFile, seqChanDataOffset, SEEK_SET);
	seqFilePosition = seqChanDataOffset;

	printf("Effect File Information\n");
	printf("seqFilename           : %s\n", seqFilename);
	printf("seqFormatID           : %s\n", seqFormatID);
	printf("seqChanDataOffset     : %d\n", seqChanDataOffset);
	printf("seqStartChannel       : %d\n", seqStartChannel);
	printf("seqStepSize           : %d\n", seqStepSize);
	printf("seqFileSize           : %lu\n", seqFileSize);
	printf("seqDuration           : %d\n", seqDuration);

//  WriteFalconPiModelFile(fullpath, data->NumChannels(), SeqData.NumFrames(), data, stChan, data->NumChannels());
//
//  000000: 45 53 45 51 01 00 00 00 2e 00 00 00 04 00 00 00    ESEQ............
//  000010: 2d 00 00 00 00 00 19 00 00 19 00 00 19 00 00 19    -...............
//
//  4 byte header        - 45 53 45 51 (ESEQ)
//  4 byte model count   - 01 00 00 00 (1)
//  4 byte step size     - 2e 00 00 00 (46 due to bug in xLights model save rounding formula)
//  4 byte start address - 04 00 00 00 (4 - this should be 1531 user says)
//  4 byte model size    - 2d 00 00 00 (44 this is correct but FPP doesn't use it anyway)

	seqPaused = 0;
	seqSingleStep = 0;
	seqSingleStepBack = 0;

	int frameNumber = 0;

	seqStarting = 0;

	return seqFileSize;
}

/*
 *
 */

int OpenSequenceFile(const char *filename) {
	printf("OpenSequenceFile(%s)\n", filename);

	if (!filename || !filename[0])
	{
		printf("Empty Sequence Filename! (%)\n", filename);
		return 0;
	}

	size_t bytesRead = 0;

	seqFileSize = 0;
	seqStarting = 1;
	seqPaused   = 0;
	seqDuration = 0;
	seqSecondsElapsed = 0;
	seqSecondsRemaining = 0;

	strcpy(seqFilename, filename);

	char tmpFilename[2048];
	unsigned char tmpData[2048];
	strcpy(tmpFilename, filename);

	seqFile = fopen((const char *)tmpFilename, "r");
	if (seqFile == NULL) 
	{
		printf("Error opening sequence file: %s. fopen returned NULL\n",
			tmpFilename);
		seqStarting = 0;
		return 0;
	}

	///////////////////////////////////////////////////////////////////////
	// Check 4-byte File format identifier
	char seqFormatID[5];
	strcpy(seqFormatID, "    ");
	bytesRead = fread(seqFormatID, 1, 4, seqFile);
	seqFormatID[4] = 0;
	if ((bytesRead != 4) || (strcmp(seqFormatID, "PSEQ") && strcmp(seqFormatID, "FSEQ") && strcmp(seqFormatID, "ESEQ")))
	{
		printf("Error opening sequence file: %s. Incorrect File Format header: '%s', bytesRead: %ld\n",
			filename, seqFormatID, bytesRead);

		fseek(seqFile, 0L, SEEK_SET);
		bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, seqFile);
		HexDump("Sequence File head:", tmpData, bytesRead);

		fclose(seqFile);
		seqFile = NULL;
		seqStarting = 0;
		return 0;
	}

	if (!strcmp(seqFormatID, "ESEQ"))
		return OpenEffectFile(filename);

	///////////////////////////////////////////////////////////////////////
	// Get Channel Data Offset
	bytesRead = fread(tmpData, 1, 2, seqFile);
	if (bytesRead != 2)
	{
		printf("Sequence file %s too short, unable to read channel data offset value\n", filename);

		fseek(seqFile, 0L, SEEK_SET);
		bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, seqFile);
		HexDump("Sequence File head:", tmpData, bytesRead);

		fclose(seqFile);
		seqFile = NULL;
		seqStarting = 0;
		return 0;
	}
	seqChanDataOffset = tmpData[0] + (tmpData[1] << 8);

	///////////////////////////////////////////////////////////////////////
	// Now that we know the header size, read the whole header in one shot
	fseek(seqFile, 0L, SEEK_SET);
	bytesRead = fread(tmpData, 1, seqChanDataOffset, seqFile);
	if (bytesRead != seqChanDataOffset)
	{
		printf("Sequence file %s too short, unable to read fixed header size value\n", filename);

		fseek(seqFile, 0L, SEEK_SET);
		bytesRead = fread(tmpData, 1, DATA_DUMP_SIZE, seqFile);
		HexDump("Sequence File head:", tmpData, bytesRead);

		fclose(seqFile);
		seqFile = NULL;
		seqStarting = 0;
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

	printf("Sequence File Information\n");
	printf("seqFilename           : %s\n", seqFilename);
	printf("seqVersion            : %d.%d\n",
		seqVersionMajor, seqVersionMinor);
	printf("seqFormatID           : %s\n", seqFormatID);
	printf("seqChanDataOffset     : %d\n", seqChanDataOffset);
	printf("seqFixedHeaderSize    : %d\n", seqFixedHeaderSize);
	printf("seqStepSize           : %d\n", seqStepSize);
	printf("seqNumPeriods         : %d\n", seqNumPeriods);
	printf("seqStepTime           : %dms\n", seqStepTime);
	printf("seqNumUniverses       : %d *\n", seqNumUniverses);
	printf("seqUniverseSize       : %d *\n", seqUniverseSize);
	printf("seqGamma              : %d *\n", seqGamma);
	printf("seqColorEncoding      : %d *\n", seqColorEncoding);
	printf("seqRefreshRate        : %d\n", seqRefreshRate);
	printf("seqFileSize           : %lu\n", seqFileSize);
	printf("seqDuration           : %d\n", seqDuration);
	printf("'*' denotes field is currently ignored by FPP\n");

	seqPaused = 0;
	seqSingleStep = 0;
	seqSingleStepBack = 0;

	int frameNumber = 0;

	seqStarting = 0;

	return seqFileSize;
}

int ReadSequenceData(void) {
	size_t  bytesRead = 0;

	bytesRead = 0;
	if(seqFilePosition < seqFileSize - seqStepSize)
	{
		bytesRead = fread(seqData, 1, seqStepSize, seqFile);
		seqFilePosition += bytesRead;
	}

	if (bytesRead != seqStepSize)
		return 0;

	seqSecondsElapsed = (int)((float)(seqFilePosition - seqChanDataOffset)/((float)seqStepSize*(float)seqRefreshRate));
	seqSecondsRemaining = seqDuration - seqSecondsElapsed;

	return bytesRead;
}

void CloseSequenceFile(void) {
	printf("CloseSequenceFile() %s\n", seqFilename);

	if (seqFile) {
		fclose(seqFile);
		seqFile = NULL;
	}

	seqFilename[0] = '\0';
	seqPaused = 0;
}

void DumpChannelData(int startChannel, int endChannel)
{
	char tmpStr[90];
	int bytesRead = 0;
	int frameNumber = 0;
	int firstNonBlank = 0;
	int totalSeconds = 0;
	int mins = 0;
	int secs = 0;
	int frameInSec = 0;
	int i = 0;

	if (startChannel < 1)
		return;

	bytesRead = fread(seqData, 1, seqStepSize, seqFile);
	while (bytesRead == seqStepSize)
	{
		firstNonBlank = 0;
		for (i = 0; i < seqStepSize && !firstNonBlank; i++)
		{
			if (seqData[i])
				firstNonBlank = i + 1;
		}

		frameInSec = frameNumber % seqRefreshRate;
		totalSeconds = frameNumber / seqRefreshRate;
		mins = totalSeconds / 60;
		secs = totalSeconds % 60;

		if (firstNonBlank)
			sprintf(tmpStr, "%02d:%02d.%02d Frame #%d, Ch: %d-%d, fnb: %d",
				mins, secs, frameInSec,
				frameNumber++, startChannel, endChannel, firstNonBlank);
		else
			sprintf(tmpStr, "%02d:%02d.%02d Frame #%d, Ch: %d-%d",
				mins, secs, frameInSec,
				frameNumber++, startChannel, endChannel);

		HexDump(tmpStr, &seqData[startChannel - 1], endChannel - startChannel + 1);
		bytesRead = fread(seqData, 1, seqStepSize, seqFile);
	}
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("USAGE: %s SequenceFilename.fseq [STARTCH [ENDCH]]\n", argv[0]);
		printf("       STARTCH = channel to start data dump at\n");
		printf("       ENDCH   = channel to end data dump at\n");
		printf("\n");
		printf("NOTE: This prints 2 or more lines per frame, so piping through less is good.\n");
		return -1;
	}

	OpenSequenceFile(argv[1]);

	int startChannel = 1;

	if (argc > 2)
		startChannel = atoi(argv[2]);

	int endChannel = startChannel + 15;
	if (argc > 3)
		endChannel = atoi(argv[3]);

	DumpChannelData(startChannel, endChannel);

	CloseSequenceFile();

	return 0;
}
