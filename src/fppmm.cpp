/*
 *   Memory Map handler for Falcon Player (FPP)
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

#include <stdio.h>
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

#include "fppversion.h"
#include "common.h"
#include "Sequence.h"

char *blockName     = NULL;
char *inputFilename = NULL;
int   isActive      = -1;
char *channels      = NULL;
int   channelData   = -1;
int   displayVers   = 0;


/*
 * Usage information for fppmm binary
 */
void usage(char *appname) {
	printf("Usage: %s [OPTIONS]\n", appname);
	printf("\n");
	printf("  Options:\n");
	printf("   -V                     - Print version information\n");
	printf("   -c CHANNEL -s VALUE    - Set channel number CHANNEL to VALUE\n");
    printf("                            VALUE of -1 will delete the range\n");
	printf("   -m MODEL               - List info about Pixel Overlay MODEL\n");
	printf("   -m MODEL -o MODE       - Set Pixel Overlay mode, Mode is one of:\n");
	printf("                            off, on, transparent, transparentrgb\n");
	printf("   -m MODEL -f FILENAME   - Copy raw FILENAME data to MODEL\n" );
	printf("   -m MODEL -s VALUE      - Fill MODEL with VALUE for all channels\n");
	printf("   -h                     - This help output\n");
}

/*
 * Parse command line arguments for fppmm binary
 */
static void parseArguments(int argc, char **argv) {
	char *s = NULL;
	int   c;

	while (1) {
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] = {
			{"mapname",        required_argument,    0, 'm'},
			{"overlaymode",    required_argument,    0, 'o'},
			{"filename",       required_argument,    0, 'f'},
			{"displayvers",    no_argument,          0, 'V'},
			{"channel",        required_argument,    0, 'c'},
			{"setvalue",       required_argument,    0, 's'},
			{"help",           no_argument,          0, 'h'},
			{0,                0,                    0, 0}
		};

		c = getopt_long(argc, argv, "m:o:f:t:c:s:hV", long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case 'V':   printVersionInfo();
						exit(0);
			case 'm':	blockName = strdup(optarg);
						break;
			case 'o':	if (!strcmp(optarg, "off"))
							isActive = 0;
						else if (!strcmp(optarg, "on"))
							isActive = 1;
						else if (!strcmp(optarg, "transparent"))
							isActive = 2;
						else if (!strcmp(optarg, "transparentrgb"))
							isActive = 3;

						break;
			case 'f':	inputFilename = strdup(optarg);
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
 * Set the value of one (or more FIXME) channels in the channel data memory map
 */
void SetChannelMemoryMap(const std::string &channels, int value) {
    
    if (value == -1) {
        std::string payload = "{\"delete\": true}";
        std::string url = "http://localhost:32322/overlays/range/" + channels;
        std::string response;
        urlPut(url, payload, response);
        printf("Response: %s\n", response.c_str());
        return;
    } else if (value < 0) {
        std::string payload = "{\"deleteAll\": true}";
        std::string url = "http://localhost:32322/overlays/range";
        std::string response;
        urlPut(url, payload, response);
        printf("Response: %s\n", response.c_str());
        return;
    }
    std::string payload = "{\"Value\": " + std::to_string(value) + "}";
    std::string url = "http://localhost:32322/overlays/range/" + channels;
    std::string response;
    urlPut(url, payload, response);
    printf("Response: %s\n", response.c_str());

}


/*
 * Set a channel data map block as active/inactive.
 */
void SetBlockActive(const std::string &blockName, int active) {
    
    std::string payload = "{\"State\": " + std::to_string(active) + "}";
    std::string url = "http://localhost:32322/overlays/model/" + blockName + "/state";
    std::string response;
    urlPut(url, payload, response);
    printf("Response: %s\n", response.c_str());
}

/*
 * Copy data in specified file to specified block in channel data memory map
 */
void CopyFileToMappedBlock(const std::string &blockName, char *inputFilename) {
    std::string url = "http://localhost:32322/overlays/model/" + blockName;
    std::string response;
    if (!urlGet(url, response)) {
        printf( "ERROR: Could not find model %s\n", blockName);
        return;
    }
        
    Json::Value v = LoadJsonFromString(response);

    
	if (!FileExists(inputFilename)) {
		printf( "ERROR: Input file %s does not exist!\n", inputFilename );
		return;
	}

	int fd = open(inputFilename, O_RDONLY);

	if (fd < 0) {
		printf( "ERROR: Unable to open input file %s: %s",
			inputFilename, strerror(errno));
		return;
	}

    int channelCount = v["ChannelCount"].asInt();
    int width = v["width"].asInt();
    int height = v["height"].asInt();

    char data[channelCount];
    int r = read(fd, data, channelCount);
    if (r != channelCount) {
        printf( "WARNING: Expected %lld bytes of data but only read %d.\n",
            channelCount, r);
    } else {
        std::string overlayBuferName = "/FPP-Model-Overlay-Buffer-" + blockName;
        mode_t mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
        int f = shm_open(overlayBuferName.c_str(), O_RDWR | O_CREAT, mode);
        int size = width * height * 3 + 12;
        uint8_t *overlayBufferData = (uint8_t*)mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, f, 0);
        close(f);
        memcpy(&overlayBufferData[12], data, channelCount);
        //data is copied, mark the overlay buffer as dirty so it gets copied into the data buffer
        uint32_t *flags = (uint32_t *)&overlayBufferData[8];
        *flags |= 1;
        munmap(overlayBufferData, size);
        printf( "Data imported\n" );
    }
}

/*
 * Fill a channel data block with a single specified value
 */
void FillMappedBlock(const std::string &blockName, int channelData) {
    std::string payload = "{ \"Value\": " + std::to_string(channelData) + " }";
    std::string url = "http://localhost:32322/overlays/model/" + blockName + "/fill";
    std::string response;
    urlPut(url, payload, response);
    printf("Response: %s\n", response.c_str());
}

/*
 * Display info on a named channel data block
 */
void DumpMappedBlockInfo(const std::string &blockName) {
    std::string url = "http://localhost:32322/overlays/model/" + blockName;
    std::string response;
    if (!urlGet(url, response)) {
        printf( "ERROR: Could not find model %s\n", blockName);
        return;
    }
        
    Json::Value v = LoadJsonFromString(response);
    
    printf( "Block Name: %s\n", v["Name"].asString().c_str());
    
    int sc = v["StartChannel"].asInt();
    int cc = v["ChannelCount"].asInt();
	printf( "Channels  : %d-%d (%d channels)\n", sc, sc+cc-1, cc);

    printf( "String Cnt: %d\n", v["StringCount"].asInt());
    printf( "Strand Cnt: %d\n", v["StrandsPerString"].asInt());
    printf( "Layout    : %dx%d\n", v["width"].asInt(), v["height"].asInt());

    printf( "Status    : ");
    switch (v["isActive"].asInt()) {
        case 0: printf("Idle\n");
                break;
        case 1: printf("Active\n");
                break;
        case 2: printf("Active (Transparent)\n");
                break;
        case 3: printf("Active (Transparent RGB)\n");
                break;
    }

    printf( "Effect running : ");
    switch (v["effectRunning"].asBool()) {
        case 0: printf("No\n");
                break;
        case 1: printf("Yes - %s\n", v["effectName"].asString().c_str());
                break;
    }
}

/*
 *
 */
int main (int argc, char *argv[])
{
	parseArguments(argc, argv);

	 if (blockName) {
		if (isActive >= 0)
			SetBlockActive(blockName, isActive);
		else if (inputFilename)
			CopyFileToMappedBlock(blockName, inputFilename);
		else if (channelData >= 0)
			FillMappedBlock(blockName, channelData);
		else
			DumpMappedBlockInfo(blockName);
		free(blockName);
	} else if (channels) {
		if (channelData > 255)
			channelData = 255;

        SetChannelMemoryMap(channels, channelData);
		free(channels);
	} else {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

