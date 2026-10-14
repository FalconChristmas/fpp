/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include "fpp-json.h"

#include <sys/stat.h>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>

#include "common.h"
#include "commands/Commands.h"

#include "fppversion.h"

char* blockName = NULL;
char* inputFilename = NULL;
int isActive = -1;
char* channels = NULL;
int channelData = -1;
int displayVers = 0;

/*
 * Usage information for fppmm binary
 */
void usage(char* appname) {
    printf("Usage: %s [OPTIONS]\n", appname);
    printf("\n");
    printf("  Options:\n");
    printf("   -V                     - Print version information\n");
    printf("   -c CHANNEL -s VALUE    - Set channel number CHANNEL to VALUE\n");
    printf("                            VALUE of -1 will delete the range\n");
    printf("   -m MODEL               - List info about Pixel Overlay MODEL\n");
    printf("   -m MODEL -o MODE       - Set Pixel Overlay mode, Mode is one of:\n");
    printf("                            off, on, transparent, transparentrgb\n");
    printf("   -m MODEL -f FILENAME   - Copy raw FILENAME data to MODEL\n");
    printf("   -m MODEL -s VALUE      - Fill MODEL with VALUE for all channels\n");
    printf("   -h                     - This help output\n");
}

/*
 * Parse command line arguments for fppmm binary
 */
static void parseArguments(int argc, char** argv) {
    char* s = NULL;
    int c;

    while (1) {
        int this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            { "mapname", required_argument, 0, 'm' },
            { "overlaymode", required_argument, 0, 'o' },
            { "filename", required_argument, 0, 'f' },
            { "displayvers", no_argument, 0, 'V' },
            { "channel", required_argument, 0, 'c' },
            { "setvalue", required_argument, 0, 's' },
            { "help", no_argument, 0, 'h' },
            { 0, 0, 0, 0 }
        };

        c = getopt_long(argc, argv, "m:o:f:t:c:s:hV", long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'V':
            printVersionInfo();
            exit(0);
        case 'm':
            blockName = strdup(optarg);
            break;
        case 'o':
            if (!strcmp(optarg, "off"))
                isActive = 0;
            else if (!strcmp(optarg, "on"))
                isActive = 1;
            else if (!strcmp(optarg, "transparent"))
                isActive = 2;
            else if (!strcmp(optarg, "transparentrgb"))
                isActive = 3;

            break;
        case 'f':
            inputFilename = strdup(optarg);
            break;
        case 'c':
            channels = strdup(optarg);
            break;
        case 's':
            channelData = strtol(optarg, NULL, 10);
            break;
        case 'h':
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

/*
 * Set the value of one (or more FIXME) channels in the channel data memory map
 */
void SetChannelMemoryMap(const std::string& channels, int value) {
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
void SetBlockActive(const std::string& blockName, int active) {
    std::string payload = "{\"State\": " + std::to_string(active) + "}";
    std::string url = "http://localhost:32322/overlays/model/" + blockName + "/state";
    std::string response;
    urlPut(url, payload, response);
    printf("Response: %s\n", response.c_str());
}

/*
 * Copy the raw data in a file into a model's overlay buffer.
 *
 * Through fppd's API, as the other options here are.  This used to map the
 * model's shared overlay buffer directly, sized for 3 bytes a pixel: an RGBW
 * model's data overran the mapping, and a model whose buffer fppd hadn't made
 * yet got an empty one created, and SIGBUS on the first write to it.
 */
void CopyFileToMappedBlock(const std::string& blockName, char* inputFilename) {
    std::string url = "http://localhost:32322/overlays/model/" + blockName;
    std::string response;
    if (!urlGet(url, response)) {
        printf("ERROR: Could not find model %s\n", blockName.c_str());
        return;
    }

    Json::Value v = LoadJsonFromString(response);
    if (!v.isObject() || !v.isMember("Name")) {
        printf("ERROR: Could not find model %s\n", blockName.c_str());
        return;
    }
    int width = v["width"].asInt();
    int height = v["height"].asInt();
    int bytesPerPixel = (width > 0 && height > 0) ? v["ChannelCount"].asInt() / (width * height) : 0;
    if (bytesPerPixel != 1 && bytesPerPixel != 3 && bytesPerPixel != 4) {
        printf("ERROR: Model %s is not a 1, 3 or 4 channel per pixel model\n", blockName.c_str());
        return;
    }

    if (!FileExists(inputFilename)) {
        printf("ERROR: Input file %s does not exist!\n", inputFilename);
        return;
    }

    std::string data = GetFileContents(inputFilename);
    size_t expected = (size_t)width * height * bytesPerPixel;
    if (data.size() < expected) {
        printf("WARNING: Expected %zu bytes of data but only read %zu.\n", expected, data.size());
        return;
    }
    data.resize(expected);

    const char* fmt = bytesPerPixel == 4 ? "rgbw" : (bytesPerPixel == 1 ? "mono" : "rgb");
    url += std::string("/data?target=overlay&stopEffect=false&fmt=") + fmt;
    if (!urlPut(url, data, response)) {
        printf("ERROR: Could not send the data to model %s\n", blockName.c_str());
        return;
    }
    printf("Response: %s\n", response.c_str());
}

/*
 * Fill a channel data block with a single specified value
 */
void FillMappedBlock(const std::string& blockName, int channelData) {
    std::string payload = "{ \"Value\": " + std::to_string(channelData) + " }";
    std::string url = "http://localhost:32322/overlays/model/" + blockName + "/fill";
    std::string response;
    urlPut(url, payload, response);
    printf("Response: %s\n", response.c_str());
}

/*
 * Display info on a named channel data block
 */
void DumpMappedBlockInfo(const std::string& blockName) {
    std::string url = "http://localhost:32322/overlays/model/" + blockName;
    std::string response;
    if (!urlGet(url, response)) {
        printf("ERROR: Could not find model %s\n", blockName.c_str());
        return;
    }

    Json::Value v = LoadJsonFromString(response);

    printf("Block Name: %s\n", v["Name"].asString().c_str());

    int sc = v["StartChannel"].asInt();
    int cc = v["ChannelCount"].asInt();
    printf("Channels  : %d-%d (%d channels)\n", sc, sc + cc - 1, cc);

    printf("String Cnt: %d\n", v["StringCount"].asInt());
    printf("Strand Cnt: %d\n", v["StrandsPerString"].asInt());
    printf("Layout    : %dx%d\n", v["width"].asInt(), v["height"].asInt());

    printf("Status    : ");
    switch (v["isActive"].asInt()) {
    case 0:
        printf("Idle\n");
        break;
    case 1:
        printf("Active\n");
        break;
    case 2:
        printf("Active (Transparent)\n");
        break;
    case 3:
        printf("Active (Transparent RGB)\n");
        break;
    }

    printf("Effect running : ");
    if (v["effectRunning"].asBool()) {
        printf("Yes - %s\n", v["effectName"].asString().c_str());
    } else {
        printf("No\n");
    }
}

/*
 *
 */
int main(int argc, char* argv[]) {
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
