/*
 *   Pixel Overlay handler for Falcon Pi Player (FPP)
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
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <memory>
#include <array>

#include "common.h"
#include "log.h"
#include "PixelOverlay.h"
#include "PixelOverlayControl.h"
#include "Sequence.h"
#include "settings.h"
#include "channeloutputthread.h"

char         *chanDataMap;
int           chanDataMapFD = -1;
char         *ctrlMap;
int           ctrlFD = -1;
long long    *pixelMap;
int           pixelFD = -1;

FPPChannelMemoryMapControlHeader *ctrlHeader = NULL;


/* Prototypes for helpers below */
int LoadChannelMemoryMapData(void);

/*
 * Create and initialize the channel data and control memory map files
 */
int InitializeChannelDataMemoryMap(void) {
	int   i  = 0;
	char  ch = '\0';
	long long pixelLocation = 0;

	LogDebug(VB_CHANNELOUT, "InitializeChannelDataMemoryMap()\n");

	// Block of of raw channel data used to overlay data
	chanDataMapFD =
		open(FPPCHANNELMEMORYMAPDATAFILE, O_CREAT | O_TRUNC | O_RDWR, 0666);

	if (chanDataMapFD < 0) {
		LogErr(VB_CHANNELOUT, "Error opening %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
		return -1;
	}

	chmod(FPPCHANNELMEMORYMAPDATAFILE, 0666);

    uint8_t * tmpData = (uint8_t*)calloc(1, FPPD_MAX_CHANNELS);
	if (write(chanDataMapFD, (void *)tmpData, FPPD_MAX_CHANNELS) != FPPD_MAX_CHANNELS) {
		LogErr(VB_CHANNELOUT, "Error populating %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
		CloseChannelDataMemoryMap();
        free(tmpData);
		return -1;
	}
    free(tmpData);

	chanDataMap = (char *)mmap(0, FPPD_MAX_CHANNELS, PROT_READ, MAP_SHARED, chanDataMapFD, 0);

	if (!chanDataMap) {
		LogErr(VB_CHANNELOUT, "Error mapping %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
		CloseChannelDataMemoryMap();
		return -1;
	}

	// Control file to turn on/off blocks and get block info
	ctrlFD = open(FPPCHANNELMEMORYMAPCTRLFILE, O_CREAT | O_TRUNC | O_RDWR, 0666);

	if (ctrlFD < 0) {
		LogErr(VB_CHANNELOUT, "Error opening %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
		CloseChannelDataMemoryMap();
		return -1;
	}

	chmod(FPPCHANNELMEMORYMAPCTRLFILE, 0666);

    
    tmpData = (uint8_t*)calloc(1, FPPCHANNELMEMORYMAPSIZE);
	if (write(ctrlFD, (void *)tmpData, FPPCHANNELMEMORYMAPSIZE) != FPPCHANNELMEMORYMAPSIZE) {
		LogErr(VB_CHANNELOUT, "Error populating %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
		CloseChannelDataMemoryMap();
        free(tmpData);
		return -1;
    }
    free(tmpData);

	ctrlMap = (char *)mmap(0, FPPCHANNELMEMORYMAPSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, ctrlFD, 0);

	if (!ctrlMap) {
		LogErr(VB_CHANNELOUT, "Error mapping %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
		CloseChannelDataMemoryMap();
		return -1;
	}

	ctrlHeader = (FPPChannelMemoryMapControlHeader *)ctrlMap;

	// Pixel Map to map channels to matrix positions
	pixelFD =
		open(FPPCHANNELMEMORYMAPPIXELFILE, O_CREAT | O_TRUNC | O_RDWR, 0666);

	if (pixelFD < 0) {
		LogErr(VB_CHANNELOUT, "Error opening %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPPIXELFILE, strerror(errno));
		CloseChannelDataMemoryMap();
		return -1;
	}

	chmod(FPPCHANNELMEMORYMAPPIXELFILE, 0666);

    tmpData = (uint8_t*)calloc(FPPD_MAX_CHANNELS, sizeof(long long));
	if (write(pixelFD, (void *)tmpData,
              FPPD_MAX_CHANNELS * sizeof(long long)) != (FPPD_MAX_CHANNELS * sizeof(long long))) {
		LogErr(VB_CHANNELOUT, "Error populating %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPPIXELFILE, strerror(errno));
		CloseChannelDataMemoryMap();
        free(tmpData);
		return -1;
	}
    free(tmpData);

	pixelMap = (long long *)mmap(0, FPPD_MAX_CHANNELS * sizeof(long long), PROT_READ|PROT_WRITE, MAP_SHARED, pixelFD, 0);

	if (!pixelMap) {
		LogErr(VB_CHANNELOUT, "Error mapping %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPPIXELFILE, strerror(errno));
		CloseChannelDataMemoryMap();
		return -1;
	}

	for (i = 0; i < FPPD_MAX_CHANNELS; i++) {
		pixelMap[i] = i;
	}

	// Load the config
	LoadChannelMemoryMapData();

	if (ctrlHeader->totalBlocks)
		StartChannelOutputThread();

	return 1;
}

/*
 * Close the channel data memory map and control map files
 */
void CloseChannelDataMemoryMap(void) {
	LogDebug(VB_CHANNELOUT, "CloseChannelDataMemoryMap()\n");

	if (chanDataMapFD) {
		munmap(chanDataMap, FPPD_MAX_CHANNELS);
		close(chanDataMapFD);
	}
	chanDataMapFD = -1;
	chanDataMap   = NULL;

	if (ctrlFD) {
		munmap(ctrlMap, FPPCHANNELMEMORYMAPSIZE);
		close(ctrlFD);
	}
	ctrlFD  = -1;
	ctrlMap = NULL;

	if (pixelFD) {
		munmap(pixelMap, FPPD_MAX_CHANNELS * sizeof(long));
		close(pixelFD);
	}
	pixelFD  = -1;
	pixelMap = NULL;
}

/*
 * Check to see if we need to run through the overlay process
 */
#ifndef __GNUG__
inline
#endif
int UsingMemoryMapInput(void) {
	if (!ctrlHeader)
		return 0;

	if (ctrlHeader->testMode)
		return 1;

	FPPChannelMemoryMapControlBlock *cb =
		(FPPChannelMemoryMapControlBlock*)(ctrlMap +
			sizeof(FPPChannelMemoryMapControlHeader));

	int i = 0;
	for (i = 0; i < ctrlHeader->totalBlocks; i++, cb++) {
		if (cb->isActive)
			return 1;
	}

	return 0;
}

/*
 * Overlay memory mapped channel data on top of current channel data
 */
void OverlayMemoryMap(char *chanData) {
	if ((!ctrlHeader) ||
		(!ctrlHeader->totalBlocks && !ctrlHeader->testMode))
		return;

	int i = 0;
	FPPChannelMemoryMapControlBlock *cb =
		(FPPChannelMemoryMapControlBlock*)(ctrlMap +
			sizeof(FPPChannelMemoryMapControlHeader));

	if (ctrlHeader->testMode) {
		memcpy(chanData, chanDataMap, FPPD_MAX_CHANNELS);
	} else {
		for (i = 0; i < ctrlHeader->totalBlocks; i++, cb++) {
			if (cb->isActive == 1) { // Active - Opaque
				memcpy(chanData + cb->startChannel - 1,
					   chanDataMap + cb->startChannel - 1, cb->channelCount);
			} else if (cb->isActive == 2) { // Active - Transparent
				char *src = chanDataMap + cb->startChannel - 1;
				char *dst = chanData + cb->startChannel - 1;

				int j = 0;
				for (j = 0; j < cb->channelCount; j++) {
					if (*src)
						*dst = *src;
					src++;
					dst++;
				}
			} else if (cb->isActive == 3) { // Active - Transparent RGB
				char *src = chanDataMap + cb->startChannel - 1;
				char *dst = chanData + cb->startChannel - 1;

				int j = 0;
				for (j = 0; j < cb->channelCount; j += 3) {
					if (src[0] || src[1] || src[2])
					{
						dst[0] = src[0];
						dst[1] = src[1];
						dst[2] = src[2];
					}
					src += 3;
					dst += 3;
				}
			}
		}
	}
}

/*
 * Display list of defined memory mapped channel blocks
 */
void PrintChannelMapBlocks(void) {
	if (!ctrlHeader)
		return;

	int i = 0;
	FPPChannelMemoryMapControlBlock *cb =
		(FPPChannelMemoryMapControlBlock*)(ctrlMap +
			sizeof(FPPChannelMemoryMapControlHeader));

	LogInfo(VB_CHANNELOUT, "Channel Memory Map Blocks\n");
	LogInfo(VB_CHANNELOUT, "Control File Version: %d.%d\n",
		(int)ctrlHeader->majorVersion, (int)ctrlHeader->minorVersion);
	LogInfo(VB_CHANNELOUT, "Total Blocks Defined: %d\n",
		ctrlHeader->totalBlocks);

	for (i = 0; i < ctrlHeader->totalBlocks; i++, cb++) {
		LogInfo(VB_CHANNELOUT, "Block %3d Name         : %s\n", i, cb->blockName);
		LogInfo(VB_CHANNELOUT, "Block %3d Start Channel: %d\n", i, cb->startChannel);
		LogInfo(VB_CHANNELOUT, "Block %3d Channel Count: %d\n", i, cb->channelCount);
		LogInfo(VB_CHANNELOUT, "Block %3d Orientation  : %c\n", i, cb->orientation);
		LogInfo(VB_CHANNELOUT, "Block %3d Start Corner : %s\n", i, cb->startCorner);
		LogInfo(VB_CHANNELOUT, "Block %3d String Count : %d\n", i, cb->stringCount);
		LogInfo(VB_CHANNELOUT, "Block %3d Strand Count : %d\n", i, cb->strandsPerString);
	}
}

/*
 * Setup the pixel map for this channel block
 */
void SetupPixelMapForBlock(FPPChannelMemoryMapControlBlock *cb) {
	LogInfo(VB_CHANNELOUT, "Initializing Channel Memory Map Pixel Map '%s'\n",
		cb->blockName);

	if ((!cb->channelCount) ||
		(!cb->strandsPerString) ||
		(!cb->stringCount)) {
		LogErr(VB_CHANNELOUT, "Invalid config for '%s' Memory Map Block\n",
			cb->blockName);
		return;
	}

	if ((cb->channelCount % 3) != 0) {
//		LogInfo(VB_CHANNELOUT, "Memory Map Block '%s' channel count is not divisible by 3\n", cb->blockName);
//		LogInfo(VB_CHANNELOUT, "unable to configure pixel map array.\n");
		return;
	}

	int TtoB = (cb->startCorner[0] == 'T') ? 1 : 0;
	int LtoR = (cb->startCorner[1] == 'L') ? 1 : 0;
	int stringSize = cb->channelCount / 3 / cb->stringCount;
	int width = 0;
	int height = 0;

	if (cb->orientation == 'H') {
		// Horizontal Orientation
		width = stringSize / cb->strandsPerString;
		height = cb->channelCount / 3 / width;

		int y = 0;
		for (y = 0; y < height; y++) {
			int segment = y % cb->strandsPerString;
			int x = 0;
			for (x = 0; x < width; x++) {
				// Pixel Position in a TL Horizontal wrapping layout
				// 0 1 2
				// 3 4 6
				// 7 8 9
				int ppos = y * width + x;
				// Relative Input Pixel 'R' channel
				int inCh = (cb->startChannel - 1) + (ppos * 3);

				// X position in output
				int outX = (LtoR != ((segment % 2) != TtoB)) ? width - x - 1 : x;
				// Y position in output
				int outY = (TtoB) ? y : height - y - 1;

				// Relative Mapped Output Pixel 'R' channel
				int mpos = outY * width + outX;
				int outCh = (cb->startChannel - 1) + (mpos * 3);

				// Map the pixel's triplet
				pixelMap[inCh    ] = outCh;
				pixelMap[inCh + 1] = outCh + 1;
				pixelMap[inCh + 2] = outCh + 2;
			}
		}
	} else {
		// Vertical Orientation
		height = stringSize / cb->strandsPerString;
		width = cb->channelCount / 3 / height;

		int x = 0;
		for (x = 0; x < width; x++) {
			int segment = x % cb->strandsPerString;
			int y = 0;
			for (y = 0; y < height; y++) {
				// Pixel Position in a TL Horizontal wrapping layout
				// 0 1 2
				// 3 4 6
				// 7 8 9
				int ppos = y * width + x;
				// Relative Input Pixel 'R' channel
				int inCh = (cb->startChannel - 1) + (ppos * 3);

				// X position in output
				int outX = (LtoR) ? x : width - x - 1;
				// Y position in output
				int outY = (TtoB != ((segment % 2) != LtoR)) ? height - y - 1 : y;

				// Relative Mapped Output Pixel 'R' channel
				int mpos = outX * height + outY;
				int outCh = (cb->startChannel - 1) + (mpos * 3);

				// Map the pixel's triplet
				pixelMap[inCh    ] = outCh;
				pixelMap[inCh + 1] = outCh + 1;
				pixelMap[inCh + 2] = outCh + 2;
			}
		}
	}

	LogInfo(VB_CHANNELOUT, "Initialization complete for block '%s'\n",
		cb->blockName);
}

/*
 * Load memory mapped channel blocks configuration file
 */
int LoadChannelMemoryMapData(void) {
	LogDebug(VB_CHANNELOUT, "LoadChannelMemoryMapData()\n");

	FPPChannelMemoryMapControlBlock *cb = NULL;

	FILE *fp;
	char filename[1024];
	char buf[64];
	char *s;
	int startChannel;
	int channelCount;

	if (!ctrlMap) {
		LogErr(VB_CHANNELOUT, "Error, trying to load memory map data when "
			"memory map is not configured.");
		return -1;
	}

	ctrlHeader->majorVersion = FPPCHANNELMEMORYMAPMAJORVER;
	ctrlHeader->minorVersion = FPPCHANNELMEMORYMAPMINORVER;
	ctrlHeader->totalBlocks  = 0;
	ctrlHeader->testMode     = 0;

	strcpy(filename, getMediaDirectory());
	strcat(filename, "/channelmemorymaps");

	if (!FileExists(filename))
		return 0;

	LogDebug(VB_CHANNELOUT, "Loading Channel Memory Map data.\n");
	fp = fopen(filename, "r");
	if (fp == NULL) 
	{
		LogErr(VB_CHANNELOUT, "Could not open Channel Memory Map config file %s\n", filename);
		return 0;
	}

	cb = (FPPChannelMemoryMapControlBlock*)(ctrlMap +
			sizeof(FPPChannelMemoryMapControlHeader));
	while(fgets(buf, 64, fp) != NULL)
	{
		if (buf[0] == '#') // Allow # comments for testing
			continue;

		// Name
		s = strtok(buf, ",");
		if (s) {
			strncpy(cb->blockName, s, 32);
		} else {
			continue;
		}

		// Start Channel
		s = strtok(NULL, ",");
		if (!s)
			continue;
		startChannel = strtol(s, NULL, 10);
		if ((startChannel <= 0) ||
			(startChannel > FPPD_MAX_CHANNELS))
			continue;
		cb->startChannel = startChannel;

		// Channel Count
		s=strtok(NULL,",");
		if (!s)
			continue;
		channelCount = strtol(s, NULL, 10);
		if ((channelCount <= 0) ||
			((startChannel + channelCount) > FPPD_MAX_CHANNELS))
			continue;
		cb->channelCount = channelCount;

		// Orientation
		s=strtok(NULL,",");
		if (!s)
			continue;
		if (!strcmp(s, "vertical"))
			cb->orientation = 'V';
		else
			cb->orientation = 'H';

		// Start Corner
		s=strtok(NULL,",");
		if (!s)
			continue;
		strncpy(cb->startCorner, s, 2);

		// String Count
		s=strtok(NULL,",");
		if (!s)
			continue;
		cb->stringCount = strtol(s, NULL, 10);

		// Strands Per String
		s=strtok(NULL,",");
		if (!s)
			continue;
		cb->strandsPerString = strtol(s, NULL, 10);

		// Sanity check our string count
		if (cb->stringCount > (cb->channelCount / 3))
			cb->stringCount = cb->channelCount / 3;

		SetupPixelMapForBlock(cb);

		cb++;

		ctrlHeader->totalBlocks++;
	}
	fclose(fp);

	if ((logLevel >= LOG_INFO) &&
		(logMask & VB_CHANNELOUT))
		PrintChannelMapBlocks();

	return 1;
}

