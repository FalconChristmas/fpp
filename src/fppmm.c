#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "memorymapcontrol.h"
#include "sequence.h"

char *blockName     = NULL;
char *inputFilename = NULL;
int   enableMap     = 0;
int   disableMap    = 0;
char *testMode      = NULL;
char *channels      = NULL;
int   channelData   = -1;

char                             *dataMap    = NULL;
int                               dataFD     = -1;
FPPChannelMemoryMapControlHeader *ctrlHeader = NULL;
char                             *ctrlMap    = NULL;
int                               ctrlFD     = -1;

/*
 * Usage information for fppmm binary
 */
void usage(char *appname) {
	printf("Usage: %s [OPTIONS]\n", appname);
	printf("\n");
	printf("  Options:\n");
	printf("   -t on                  - Turn channel test mode On\n");
	printf("   -t off                 - Turn channel test mode Off\n");
	printf("   -t status              - Check current status of test mode\n");
	printf("   -c CHANNEL -s VALUE    - Set channel number CHANNEL to VALUE\n");
	printf("   -m MAP                 - List info about MAP block\n");
	printf("   -m MAP -e              - Enable MAP memory block\n");
	printf("   -m MAP -d              - Disable MAP memory block\n");
	printf("   -m MAP -f FILENAME     - Copy raw FILENAME data to MAP\n" );
	printf("   -m MAP -s VALUE        - Fill MAP with VALUE for all channels\n");
	printf("   -h                     - This help output\n");
}

/*
 * Parse command line arguments for fppmm binary
 */
int parseArguments(int argc, char **argv) {
	char *s = NULL;
	int   c;

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"mapname",        required_argument,    0, 'm'},
			{"enable",         no_argument,          0, 'e'},
			{"disable",        no_argument,          0, 'd'},
			{"filename",       required_argument,    0, 'f'},
			{"testmode",       required_argument,    0, 't'},
			{"channel",        required_argument,    0, 'c'},
			{"setvalue",       required_argument,    0, 's'},
			{"help",           no_argument,          0, 'h'},
			{0,                0,                    0, 0}
		};

		c = getopt_long(argc, argv, "m:edf:t:c:s:h", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'm':	blockName = strdup(optarg);
						break;
			case 'e':	enableMap = 1;
						break;
			case 'd':	disableMap = 1;
						break;
			case 'f':	inputFilename = strdup(optarg);
						break;
			case 't':	testMode = strdup(optarg);
						break;
			case 'c':	channels = strdup(optarg);
						break;
			case 's':	channelData = strtol(optarg, NULL, 10);
						break;
			case 'h':	usage(argv[0]);
						exit(EXIT_SUCCESS);
			default: 	usage(argv[0]);
						exit(EXIT_FAILURE);
		}
	}
}

/*
 * Open the channel data memory map file and set the global file descriptor
 * and data pointer to the map.
 */
int OpenChannelMemoryMap(void) {
	dataFD = open(FPPCHANNELMEMORYMAPDATAFILE, O_RDWR);

	if (dataFD < 0) {
		printf( "ERROR opening memory mapped file %s: %s",
			FPPCHANNELMEMORYMAPDATAFILE, strerror(errno));
		return dataFD;
	}

	dataMap = (char *)mmap(0, FPPD_MAX_CHANNELS, PROT_WRITE | PROT_READ,
		MAP_SHARED, dataFD, 0);

	if (!dataMap) {
		printf( "Unable to memory map file: %s\n", strerror(errno));
		close(dataFD);
		dataFD = -1;
		return dataFD;
	}

	return dataFD;
}

/*
 * Close the channel data memory map file and cleanup.
 */
void CloseChannelMemoryMap(void) {
	munmap(dataMap, FPPD_MAX_CHANNELS);
	close(dataFD);

	dataFD  = -1;
	dataMap = NULL;
}

/*
 * Open the channel data control memory map file and set the global file
 * descriptor and data pointer to the map.
 */
int OpenChannelControlMemoryMap(void) {
	ctrlFD = open(FPPCHANNELMEMORYMAPCTRLFILE, O_RDWR);

	if (ctrlFD < 0) {
		printf( "ERROR opening memory mapped file %s: %s",
			FPPCHANNELMEMORYMAPCTRLFILE, strerror(errno));
		return ctrlFD;
	}

	ctrlMap = (char *)mmap(0, FPPCHANNELMEMORYMAPSIZE, PROT_WRITE | PROT_READ,
		MAP_SHARED, ctrlFD, 0);

	if (!ctrlMap) {
		printf( "Unable to memory map control file: %s\n", strerror(errno));
		close(ctrlFD);
		ctrlFD = -1;
		return ctrlFD;
	}

	ctrlHeader = (FPPChannelMemoryMapControlHeader *)ctrlMap;

	return ctrlFD;
}

/*
 * Close the channel data control memory map file and cleanup
 */
void CloseChannelControlMemoryMap(void) {
	munmap(ctrlMap, FPPCHANNELMEMORYMAPSIZE);
	close(ctrlFD);

	ctrlFD  = -1;
	ctrlMap = NULL;
}

/*
 * Set the value of one (or more FIXME) channels in the channel data memory map
 */
void SetChannelMemoryMap(char *channels, int value) {
	// FIXME, enhance this to support ranges and comma-separated sets
	//        of channels "1-10" or "1,4,7,10"
	int channel = strtol(channels, NULL, 10);

	if ((channel <= 0) ||
		(channel > FPPD_MAX_CHANNELS)) {
		printf( "ERROR, channel %d is not in range of 1-%d\n",
			channel, FPPD_MAX_CHANNELS);
		return;
	}

	if (OpenChannelMemoryMap() < 0)
		return;

	dataMap[channel - 1] = (char)value;

	printf( "Set memory mapped channel %d to %d\n", channel, value);

	CloseChannelMemoryMap();
}

/*
 *  Set or query the channel data test mode setting
 *
 *  1 = Turn test mode On
 *  0 = Turn test mode Off
 * -1 = Query current test mode
 */
int ChannelTestMode(int mode) {
	int result = mode;

	if (OpenChannelControlMemoryMap() < 0)
		return;

	if (mode >= 0) {
		if (mode)
			printf( "Turning test mode On.\n" );
		else
			printf( "Turning test mode Off.\n" );

		ctrlHeader->testMode = (unsigned char)mode;
	} else {
		result = (int)ctrlHeader->testMode;

		if (result)
			printf( "Test mode is currently On.\n" );
		else
			printf( "Test mode is currently Off.\n" );
	}

	CloseChannelControlMemoryMap();

	return result;
}

/*
 * Find the named block of channels in the channel data memory map
 */
FPPChannelMemoryMapControlBlock *FindBlock(char *blockName) {
	if (!ctrlHeader)
		return NULL;

	FPPChannelMemoryMapControlBlock *cb =
		(FPPChannelMemoryMapControlBlock*)(ctrlMap + sizeof(FPPChannelMemoryMapControlHeader));

	int i;
	for (i = 0; i < ctrlHeader->totalBlocks; i++) {
		if (!strcmp(cb->blockName, blockName))
			return cb;
		cb++;
	}

	return NULL;
}

/*
 * Set a channel data map block as active/inactive.
 */
void SetMappedBlockActive(char *blockName, int active) {
	if (OpenChannelControlMemoryMap() < 0)
		return;

	FPPChannelMemoryMapControlBlock *cb = FindBlock(blockName);

	if (cb) {
		cb->isActive = active;
	} else {
		printf( "ERROR: Could not find MAP %s\n", blockName);
	}

	CloseChannelControlMemoryMap();
}

/*
 * Copy data in specified file to specified block in channel data memory map
 */
void CopyFileToMappedBlock(char *blockName, char *inputFilename) {
	if (!FileExists(inputFilename)) {
		printf( "ERROR: Input file %s does not exist!\n" );
		return;
	}

	int fd = open(inputFilename, O_RDONLY);

	if (fd < 0) {
		printf( "ERROR: Unable to open input file %s: %s",
			inputFilename, strerror(errno));
		return;
	}

	if (OpenChannelControlMemoryMap() < 0) {
		close(fd);
		return;
	}

	if (OpenChannelMemoryMap() < 0) {
		CloseChannelControlMemoryMap();
		close(fd);
		return;
	}

	FPPChannelMemoryMapControlBlock *cb = FindBlock(blockName);

	if (cb) {
		int i = read(fd, dataMap + cb->startChannel, cb->channelCount);
		if (i != cb->channelCount) {
			printf( "WARNING: Expected %d bytes of data but only read %d.\n",
				cb->channelCount, i);
		} else {
			printf( "Data imported\n" );
		}
	} else {
		printf( "ERROR: Could not find MAP %s\n", blockName);
	}

	CloseChannelMemoryMap();
	CloseChannelControlMemoryMap();
	close(fd);
}

/*
 * Fill a channel data block with a single specified value
 */
void FillMappedBlock(char *blockName, int channelData) {
	if (OpenChannelControlMemoryMap() < 0) {
		return;
	}

	if (OpenChannelMemoryMap() < 0) {
		CloseChannelControlMemoryMap();
		return;
	}

	FPPChannelMemoryMapControlBlock *cb = FindBlock(blockName);

	if (cb) {
		memset(dataMap + cb->startChannel, channelData, cb->channelCount);
	} else {
		printf( "ERROR: Could not find MAP %s\n", blockName);
	}

	CloseChannelMemoryMap();
	CloseChannelControlMemoryMap();
}

/*
 *
 */
int main (int argc, char *argv[])
{
	parseArguments(argc, argv);

	if (testMode) {
		if (!strcasecmp(testMode, "on"))
			ChannelTestMode(1);
		else if (!strcasecmp(testMode, "off"))
			ChannelTestMode(0);
		else if (!strcasecmp(testMode, "status"))
			ChannelTestMode(-1);
		else
		{
			printf( "ERROR: Invalid argument %s supplied to '-t' option\n",
				testMode);
			free(testMode);
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}

		free(testMode);
	} else if (blockName) {
		if (enableMap)
			SetMappedBlockActive(blockName, 1);
		else if (disableMap)
			SetMappedBlockActive(blockName, 0);
		else if (inputFilename)
			CopyFileToMappedBlock(blockName, inputFilename);
		else if (channelData >= 0)
			FillMappedBlock(blockName, channelData);
		else
		{
			printf( "ERROR: Insufficient arguments supplied\n", testMode);
			free(testMode);
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}

		free(blockName);
	} else if (channels) {
		if (channelData < 0)
			channelData = 0;
		else if (channelData > 255)
			channelData = 255;

        SetChannelMemoryMap(channels, channelData);

		free(channels);
	} else {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

