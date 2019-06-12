/*
 *   generic output channel handler for Falcon Player (FPP)
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

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

#include "common.h"
#include "log.h"
#include "Sequence.h"
#include "settings.h"
#include "channeloutput.h"
#include "ChannelOutputBase.h"

//old style that still need porting
#include "FPD.h"
#include "LOR.h"
#include "SPInRF24L01.h"
#include "USBRenard.h"
#include "Triks-C.h"

#include "processors/OutputProcessor.h"

/////////////////////////////////////////////////////////////////////////////

int                      channelOutputCount  = 0;
unsigned long            channelOutputFrame  = 0;
float                    mediaElapsedSeconds = 0.0;
FPPChannelOutputInstance channelOutputs[FPPD_MAX_CHANNEL_OUTPUTS];

static int LoadOutputProcessors(void);

OutputProcessors         outputProcessors;

static std::vector<std::pair<uint32_t, uint32_t>> outputRanges;
const std::vector<std::pair<uint32_t, uint32_t>> GetOutputRanges() {
    if (outputRanges.empty()) {
        outputRanges.push_back(std::pair<uint32_t, uint32_t>(0, FPPD_MAX_CHANNELS));
    }
    return outputRanges;
}


/////////////////////////////////////////////////////////////////////////////

void ChannelOutputJSON2CSV(Json::Value config, char *configStr)
{
	Json::Value::Members memberNames = config.getMemberNames();

	configStr[0] = '\0';

	if (!config.isMember("type"))
	{
		strcpy(configStr, "");
		return;
	}

	if (config.isMember("enabled"))
		strcat(configStr, config["enabled"].asString().c_str());
	else
		strcat(configStr, "0");

	strcat(configStr, ",");

	strcat(configStr, config["type"].asString().c_str());
	strcat(configStr, ",");

	if (config.isMember("startChannel"))
		strcat(configStr, config["startChannel"].asString().c_str());
	else
		strcat(configStr, "1");

	strcat(configStr, ",");

	if (config.isMember("channelCount"))
		strcat(configStr, config["channelCount"].asString().c_str());
	else
		strcat(configStr, "1");

	std::string key;
	int first = 1;
	for (int i = 0; i < memberNames.size(); i++)
	{
		key = memberNames[i];

		if (first)
		{
			strcat(configStr, ",");
			first = 0;
		}
		else
			strcat(configStr, ";");

		strcat(configStr, key.c_str());
		strcat(configStr, "=");
		strcat(configStr, config[key].asString().c_str());
	}
}

// in some of these cases, we could symlink the shlib and add additional createXXXOutput methods
static std::map<std::string, std::string> OUTPUT_REMAPS = {
    {"VirtualDisplay", "FBVirtualDisplay"},
    {"VirtualMatrix", "FBMatrix" },
    {"DMX-Pro", "USBDMX"},
    {"DMX-Open", "USBDMX"},
    {"Pixelnet-Lynx", "USBPixelnet"},
    {"Pixelnet-Open", "USBPixelnet"},
    {"universes", "UDPOutput"}
};


/*
 *
 */
int InitializeChannelOutputs(void) {
	Json::Value root;
	Json::Reader reader;
	int i = 0;

	channelOutputFrame = 0;

	for (i = 0; i < FPPD_MAX_CHANNEL_OUTPUTS; i++) {
		bzero(&channelOutputs[i], sizeof(channelOutputs[i]));
	}

	// Reset index so we can start populating the outputs array
	i = 0;
    int maximumNeededChannel = 0;
    int minimumNeededChannel = FPPD_MAX_CHANNELS;

	if (FPDOutput.isConfigured())
	{
		channelOutputs[i].startChannel = getSettingInt("FPDStartChannelOffset");
		channelOutputs[i].outputOld = &FPDOutput;

		if (FPDOutput.open("", &channelOutputs[i].privData)) {
			channelOutputs[i].channelCount = channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData);

            int m1 = channelOutputs[i].startChannel;
            int m2 = m1 + channelOutputs[i].channelCount - 1;
            LogInfo(VB_CHANNELOUT, "FPD:  Determined range needed %d - %d\n",
                    m1, m2);
            
            minimumNeededChannel = std::min(minimumNeededChannel, m1);
            maximumNeededChannel = std::max(maximumNeededChannel, m2);

			i++;
			LogDebug(VB_CHANNELOUT, "Configured FPD Channel Output\n");
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening FPD Channel Output\n");
		}
	}
    
	// FIXME, build this list dynamically
	const char *configFiles[] = {
        "/config/co-universes.json",
		"/config/channeloutputs.json",
		"/config/co-other.json",
		"/config/co-pixelStrings.json",
        "/config/co-bbbStrings.json",
		NULL
		};

	FILE *fp;
	char filename[1024];
	char csvConfig[2048];

    
	// Parse the JSON channel outputs config files
	for (int f = 0; configFiles[f]; f++)
	{
		strcpy(filename, getMediaDirectory());
		strcat(filename, configFiles[f]);

		LogDebug(VB_CHANNELOUT, "Loading %s\n", filename);

		if (FileExists(filename))
		{
			std::ifstream t(filename);
			std::stringstream buffer;

			buffer << t.rdbuf();

			std::string config = buffer.str();

			bool success = reader.parse(buffer.str(), root);
			if (!success)
			{
				LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename);
				return 0;
			}

			const Json::Value outputs = root["channelOutputs"];
			std::string type;
			int start = 0;
			int count = 0;

			for (int c = 0; c < outputs.size(); c++)
			{
				csvConfig[0] = '\0';

				type = outputs[c]["type"].asString();

				if (!outputs[c]["enabled"].asInt())
				{
					LogDebug(VB_CHANNELOUT, "Skipping Disabled Channel Output: %s\n", type.c_str());
					continue;
				}

				start = outputs[c]["startChannel"].asInt();
				count = outputs[c]["channelCount"].asInt();

				// internally we start channel counts at zero
				start -= 1;

				channelOutputs[i].startChannel = start;
				channelOutputs[i].channelCount = count;
                std::string libnamePfx = "";

				// First some Channel Outputs enabled everythwere
				if (type == "LEDPanelMatrix") {
                    //for LED matrices, the driver is determined by the subType
                    libnamePfx = "matrix-";
                    type = outputs[c]["subType"].asString();
                // NOW some platform or config specific Channel Outputs
#ifdef PLATFORM_PI
				} else if (type == "SPI-nRF24L01") {
					channelOutputs[i].outputOld = &SPInRF24L01Output;
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
#endif
				} else if (type == "LOR") {
					channelOutputs[i].outputOld = &LOROutput;
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "Renard") {
					channelOutputs[i].outputOld = &USBRenardOutput;
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
				} else if (type == "Triks-C") {
					channelOutputs[i].outputOld = &TriksCOutput;
					ChannelOutputJSON2CSV(outputs[c], csvConfig);
                } else if (OUTPUT_REMAPS.find(type) != OUTPUT_REMAPS.end()) {
                    type = OUTPUT_REMAPS[type];
                }
                
                if (channelOutputs[i].outputOld == nullptr && channelOutputs[i].output == nullptr) {
                    std::string libname = "libfpp-co-" + libnamePfx + type + ".so";
                    void *handle = dlopen(libname.c_str(), RTLD_NOW);
                    if (handle == NULL){
                        LogErr(VB_CHANNELOUT, "Unknown Channel Output type: %s\n", type.c_str());
                        continue;
                    }
                    ChannelOutputBase* (*fptr)(unsigned int, unsigned int);
                    std::string methodName = "createOutput" + type;
                    std::replace( methodName.begin(), methodName.end(), '-', '_');
                    *(void **)(&fptr) = dlsym(handle, methodName.c_str());
                    if (fptr == nullptr) {
                        //some use createOutputFoo and others may use createFooOutput
                        std::string methodName = "create" + type + "Output";
                        std::replace( methodName.begin(), methodName.end(), '-', '_');
                        *(void **)(&fptr) = dlsym(handle, methodName.c_str());
                    }
                    if (fptr == nullptr) {
                        LogErr(VB_CHANNELOUT, "Could not create Channel Output type: %s\n", type.c_str());
                        continue;
                    }
                    channelOutputs[i].output = fptr(start, count);
                }

				if ((channelOutputs[i].outputOld) &&
					(channelOutputs[i].outputOld->open(csvConfig, &channelOutputs[i].privData)))
				{
					if (channelOutputs[i].channelCount > channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData)) {
						LogWarn(VB_CHANNELOUT,
							"Channel Output config, count (%d) exceeds max (%d) channel for configured output\n",
							channelOutputs[i].channelCount, channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData));

						channelOutputs[i].channelCount = channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData);

						LogWarn(VB_CHANNELOUT,
							"Count suppressed to %d for config: %s\n", channelOutputs[i].channelCount, csvConfig);
					}
                    
                    int m1 = channelOutputs[i].startChannel;
                    int m2 = m1 + channelOutputs[i].channelCount - 1;
                    LogInfo(VB_CHANNELOUT, "%s %d:  Determined range needed %d - %d\n",
                            type.c_str(), i, m1, m2);
                    minimumNeededChannel = std::min(minimumNeededChannel, m1);
                    maximumNeededChannel = std::max(maximumNeededChannel, m2);
					i++;
				} else if (channelOutputs[i].output && channelOutputs[i].output->Init(outputs[c])) {
                    int m1, m2;
                    channelOutputs[i].output->GetRequiredChannelRange(m1, m2);
                    minimumNeededChannel = std::min(minimumNeededChannel, m1);
                    maximumNeededChannel = std::max(maximumNeededChannel, m2);
                    LogInfo(VB_CHANNELOUT, "%s %d:  Determined range needed %d - %d\n",
                            type.c_str(), i, minimumNeededChannel, maximumNeededChannel);
                    i++;
				} else {
					LogErr(VB_CHANNELOUT, "ERROR Opening %s Channel Output\n", type.c_str());
					continue;
				}

				LogDebug(VB_CHANNELOUT, "Configured %s Channel Output\n", type.c_str());
			}
		}
	}

	channelOutputCount = i;

	LogDebug(VB_CHANNELOUT, "%d Channel Outputs configured\n", channelOutputCount);

	LoadOutputProcessors();
    int m1, m2;
    outputProcessors.GetRequiredChannelRange(m1, m2);
    minimumNeededChannel = std::min(minimumNeededChannel, m1);
    maximumNeededChannel = std::max(maximumNeededChannel, m2);
    if (maximumNeededChannel < minimumNeededChannel) {
        maximumNeededChannel = minimumNeededChannel = 0;
    }
    // having the reads be aligned to intervals of 8 can help performance so
    // we'll expand the range a bit to align things better
    //round minimum down to interval of 8
    if (minimumNeededChannel > 0) {
        minimumNeededChannel--;
    }
    minimumNeededChannel &= 0xFFFFFFF8;
    maximumNeededChannel += 8;
    maximumNeededChannel &= 0xFFFFFFF8;
    maximumNeededChannel -= 1;
    
    outputRanges.push_back(std::pair<uint32_t, uint32_t>(minimumNeededChannel, maximumNeededChannel - minimumNeededChannel + 1));

    LogInfo(VB_CHANNELOUT, "Determined range needed %d - %d\n", minimumNeededChannel, maximumNeededChannel);

	return 1;
}

/*
 * Set the channel output frame counter to a specific value
 */
void SetChannelOutputFrameNumber(int frameNumber)
{
	channelOutputFrame = frameNumber;
}

/*
 * Reset the output frame count
 */
void ResetChannelOutputFrameNumber(void) {
	channelOutputFrame = 0;
	mediaElapsedSeconds = 0.0;
}


int PrepareChannelData(char *channelData) {
    outputProcessors.ProcessData((unsigned char *)channelData);
    FPPChannelOutputInstance *inst;
    for (int i = 0; i < channelOutputCount; i++) {
        inst = &channelOutputs[i];
        if (inst->output) {
            inst->output->PrepData((unsigned char *)channelData);
        }
    }
    return 0;
}

/*
 *
 */
int SendChannelData(const char *channelData) {
	int i = 0;
	FPPChannelOutputInstance *inst;

	if (logMask & VB_CHANNELDATA) {
        uint32_t minimumNeededChannel = GetOutputRanges()[0].first;
        char buf[128];
        sprintf(buf, "Channel Data starting at channel %d", minimumNeededChannel);
		HexDump(buf, &channelData[minimumNeededChannel], 16);
	}

    for (i = 0; i < channelOutputCount; i++) {
        inst = &channelOutputs[i];
        if (inst->outputOld) {
            inst->outputOld->send(
                    inst->privData,
                    channelData + inst->startChannel,
                    inst->channelCount < (FPPD_MAX_CHANNELS - inst->startChannel) ? inst->channelCount : (FPPD_MAX_CHANNELS - inst->startChannel));
        } else if (inst->output) {
            inst->output->SendData((unsigned char *)(channelData + inst->startChannel));
        }
    }


	channelOutputFrame++;

	// Reset channelOutputFrame every week @ 50ms timing
	if (channelOutputFrame > 12096000)
		channelOutputFrame = 0;
    return 0;
}

/*
 *
 */
void StartOutputThreads(void) {
	FPPChannelOutputInstance *output;
	int i = 0;

	for (i = 0; i < channelOutputCount; i++) {
		if ((channelOutputs[i].outputOld) &&
			(channelOutputs[i].outputOld->startThread))
			channelOutputs[i].outputOld->startThread(channelOutputs[i].privData);
	}
}

/*
 *
 */
void StopOutputThreads(void) {
	FPPChannelOutputInstance *output;
	int i = 0;

	for (i = 0; i < channelOutputCount; i++) {
		if ((channelOutputs[i].outputOld) &&
			(channelOutputs[i].outputOld->stopThread))
			channelOutputs[i].outputOld->stopThread(channelOutputs[i].privData);
	}
}

/*
 *
 */
int CloseChannelOutputs(void) {
	int i = 0;

	for (i = 0; i < channelOutputCount; i++) {
		if (channelOutputs[i].outputOld)
			channelOutputs[i].outputOld->close(channelOutputs[i].privData);
		else if (channelOutputs[i].output)
			channelOutputs[i].output->Close();

		if (channelOutputs[i].privData)
			free(channelOutputs[i].privData);
	}
    
    for (i = 0; i < channelOutputCount; i++) {
        if (channelOutputs[i].output) {
            delete channelOutputs[i].output;
            channelOutputs[i].output = NULL;
        }
    }
}


int LoadOutputProcessors(void) {
	char filename[1024];
	Json::Value root;
	Json::Reader reader;

	strcpy(filename, getMediaDirectory());
	strcat(filename, "/config/outputprocessors.json");
    
	if (!FileExists(filename))
		return 0;

	LogDebug(VB_CHANNELOUT, "Loading Output Processors.\n");

	std::ifstream t(filename);
	std::stringstream buffer;

	buffer << t.rdbuf();

	std::string config = buffer.str();

	bool success = reader.parse(buffer.str(), root);
	if (!success)
	{
		LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename);
		return 0;
	}

    outputProcessors.loadFromJSON(root);

	return 1;
}

