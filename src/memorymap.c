#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "common.h"
#include "log.h"
#include "memorymap.h"
#include "memorymapcontrol.h"
#include "sequence.h"
#include "settings.h"

char         *chanDataMap;
int           chanDataMapFD = -1;
char         *ctrlMap;
int           ctrlFD = -1;

FPPChannelMemoryMapControlHeader *ctrlHeader = NULL;


/* Prototypes for helpers below */
int LoadChannelMemoryMapData(void);

/*
 * Create and initialize the channel data and control memory map files
 */
int InitializeChannelDataMemoryMap(void) {
	int  i  = 0;
	char ch = '\0';

	LogDebug(VB_CHANNELOUT, "InitializeChannelDataMemoryMap()\n");

	chanDataMapFD =
		open(FPPCHANNELMEMORYMAPDATAFILE, O_CREAT | O_TRUNC | O_RDWR, 0666);

	if (chanDataMapFD < 0) {
		LogErr(VB_CHANNELOUT, "Error opening %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
		return -1;
	}

	chmod(FPPCHANNELMEMORYMAPDATAFILE, 0666);

	for (i = 0; i < FPPD_MAX_CHANNELS; i++) {
		if (write(chanDataMapFD, &ch, 1) != 1) {
			LogErr(VB_CHANNELOUT, "Error populating %s memory map file: %s\n",
				FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
			close(chanDataMapFD);
			return -1;
		}
	}

	chanDataMap = (char *)mmap(0, FPPD_MAX_CHANNELS, PROT_READ, MAP_SHARED, chanDataMapFD, 0);

	if (!chanDataMap) {
		LogErr(VB_CHANNELOUT, "Error mapping %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
		close(chanDataMapFD);
		return -1;
	}

	ctrlFD = open(FPPCHANNELMEMORYMAPCTRLFILE, O_CREAT | O_TRUNC | O_RDWR, 0666);

	if (ctrlFD < 0) {
		LogErr(VB_CHANNELOUT, "Error opening %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
		CloseChannelDataMemoryMap();
		return -1;
	}

	chmod(FPPCHANNELMEMORYMAPCTRLFILE, 0666);

	for (i = 0; i < FPPCHANNELMEMORYMAPSIZE; i++) {
		if (write(ctrlFD, &ch, 1) != 1) {
			LogErr(VB_CHANNELOUT, "Error populating %s memory map file: %s\n",
				FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
			CloseChannelDataMemoryMap();
			return -1;
		}
	}

	ctrlMap = (char *)mmap(0, FPPCHANNELMEMORYMAPSIZE, PROT_READ|PROT_WRITE, MAP_SHARED, ctrlFD, 0);

	if (!ctrlMap) {
		LogErr(VB_CHANNELOUT, "Error mapping %s memory map file: %s\n",
			FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
		CloseChannelDataMemoryMap();
		return -1;
	}

	ctrlHeader = (FPPChannelMemoryMapControlHeader *)ctrlMap;

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

	if (ctrlFD) {
		munmap(ctrlMap, FPPCHANNELMEMORYMAPSIZE);
		close(ctrlFD);
	}
}

/*
 * Check to see if we need to run through the overlay process
 */
inline int UsingMemoryMapInput(void) {
	if ((ctrlHeader) &&
		(ctrlHeader->totalBlocks || ctrlHeader->testMode))
		return 1;

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
			if (cb->isActive) {
				memcpy(chanData + cb->startChannel - 1,
					   chanDataMap + cb->startChannel - 1, cb->channelCount);
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
		LogInfo(VB_CHANNELOUT, "Block %3d Name    : %s\n", i, cb->blockName);
		LogInfo(VB_CHANNELOUT, "Block %3d Start Ch: %d\n", i, cb->startChannel);
		LogInfo(VB_CHANNELOUT, "Block %3d Ch Count: %d\n", i, cb->channelCount);
	}
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
		return;

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
		}

		// Start Channel
		s = strtok(NULL, ",");
		startChannel = strtol(s, NULL, 10);
		if ((startChannel <= 0) ||
			(startChannel > FPPD_MAX_CHANNELS))
			continue;

		// Channel Count
		s=strtok(NULL,",");
		channelCount = strtol(s, NULL, 10);
		if ((channelCount <= 0) ||
			((startChannel + channelCount) > FPPD_MAX_CHANNELS))
			continue;

		cb->startChannel = startChannel;
		cb->channelCount = channelCount;
		cb++;

		ctrlHeader->totalBlocks++;
	}
	fclose(fp);

	if ((logLevel >= LOG_INFO) &&
		(logMask & VB_CHANNELOUT))
		PrintChannelMapBlocks();

	return 1;
}

