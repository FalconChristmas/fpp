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
#include "Warnings.h"

//old style that still need porting
#include "FPD.h"

#include "processors/OutputProcessor.h"

/////////////////////////////////////////////////////////////////////////////

int                      channelOutputCount  = 0;
unsigned long            channelOutputFrame  = 0;
float                    mediaElapsedSeconds = 0.0;
FPPChannelOutputInstance channelOutputs[FPPD_MAX_CHANNEL_OUTPUTS];

static int LoadOutputProcessors(void);

OutputProcessors         outputProcessors;

static std::vector<std::pair<uint32_t, uint32_t>> outputRanges;

const std::vector<std::pair<uint32_t, uint32_t>> &GetOutputRanges() {
    if (outputRanges.empty()) {
        outputRanges.push_back(std::pair<uint32_t, uint32_t>(0, FPPD_MAX_CHANNELS));
    }
    return outputRanges;
}
// we'll sort the ranges that the outputs have registered and combine any overlaps
// or close ranges to keep the range list smaller
static void sortRanges() {
    std::map<uint32_t, uint32_t> ranges;
    //sort
    for (auto &a : outputRanges) {
        uint32_t cur = ranges[a.first];
        if (cur) {
            ranges[a.first] = std::max(a.second, cur);
        } else {
            ranges[a.first] = a.second;
        }
    }
    outputRanges.clear();
    std::pair<uint32_t, uint32_t> cur(FPPD_MAX_CHANNELS, FPPD_MAX_CHANNELS);
    for (auto &a : ranges) {
        if (cur.first == FPPD_MAX_CHANNELS) {
            cur.first = a.first;
            cur.second = a.second;
        } else {
            if (a.first <= (cur.first + cur.second + 1025)) {
                // overlap or within 1025 channels of an overlap, need to combine
                // if the two ranges are "close" (wthin 1025 channels) we'll combine
                // as the overhead of doing ranges wouldn't benefit with a small gap
                uint32_t max = cur.first + cur.second - 1;
                uint32_t amax = a.first + a.second -1 ;
                max = std::max(max, amax);
                cur.second = max - cur.first + 1;
            } else {
                outputRanges.push_back(cur);
                cur.first = a.first;
                cur.second = a.second;
            }
        }
    }
    if (cur.first != FPPD_MAX_CHANNELS) {
        outputRanges.push_back(cur);
    }
    
    if (outputRanges.empty()) {
        cur.first = 0;
        cur.second = 8;
        outputRanges.push_back(cur);
    }
}
static void addRange(uint32_t min, uint32_t max) {
    // having the reads be aligned to intervals of 8 can help performance so
    // we'll expand the range a bit to align things better
    //round minimum down to interval of 8

    min &= 0xFFFFFFF8;
    max += 8;
    max &= 0xFFFFFFF8;
    max -= 1;
    
    for (auto &r : outputRanges) {
        int rm = r.first + r.second - 1;
        if (min >= r.first && max <= rm) {
            //within the range, don't add it
            return;
        }
    }
    outputRanges.push_back(std::pair<uint32_t, uint32_t>(min, max - min + 1));
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
	int i = 0;

	channelOutputFrame = 0;

	for (i = 0; i < FPPD_MAX_CHANNEL_OUTPUTS; i++) {
		bzero(&channelOutputs[i], sizeof(channelOutputs[i]));
	}

	// Reset index so we can start populating the outputs array
	i = 0;
	if (FPDOutput.isConfigured()) {
		channelOutputs[i].startChannel = getSettingInt("FPDStartChannelOffset");
		channelOutputs[i].outputOld = &FPDOutput;

		if (FPDOutput.open("", &channelOutputs[i].privData)) {
			channelOutputs[i].channelCount = channelOutputs[i].outputOld->maxChannels(channelOutputs[i].privData);

            int m1 = channelOutputs[i].startChannel;
            int m2 = m1 + channelOutputs[i].channelCount - 1;
            LogInfo(VB_CHANNELOUT, "FPD:  Determined range needed %d - %d\n",
                    m1, m2);
            
            addRange(m1, m2);
            
			i++;
			LogDebug(VB_CHANNELOUT, "Configured FPD Channel Output\n");
		} else {
			LogErr(VB_CHANNELOUT, "ERROR Opening FPD Channel Output\n");
		}
	}
    
	// FIXME, build this list dynamically
	const char *configFiles[] = {
        "/config/co-universes.json",
		"/config/co-other.json",
		"/config/co-pixelStrings.json",
        "/config/co-bbbStrings.json",
        "/config/channeloutputs.json",
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
			if (!LoadJsonFromFile(filename, root))
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
                        WarningHolder::AddWarning("Could not create output type " + type + ". Check logs for details.");
                        dlclose(handle);
                        continue;
                    }
                    channelOutputs[i].output = fptr(start, count);
                    channelOutputs[i].libHandle = handle;
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
                    addRange(m1, m2);
					i++;
                } else if (channelOutputs[i].output) {
                    
                    if (channelOutputs[i].output->Init(outputs[c])) {
                        channelOutputs[i].output->GetRequiredChannelRanges([type, i](int m1, int m2) {
                            LogInfo(VB_CHANNELOUT, "%s %d:  Determined range needed %d - %d\n",
                                    type.c_str(), i, m1, m2);
                            addRange(m1, m2);

                        });
                        i++;
                    } else {
                        WarningHolder::AddWarning("Could not initialize output type " + type + ". Check logs for details.");
                        delete channelOutputs[i].output;
                        channelOutputs[i].output = nullptr;
                        if (channelOutputs[i].libHandle) {
                            dlclose(channelOutputs[i].libHandle);
                        }
                    }
				} else {
					LogErr(VB_CHANNELOUT, "ERROR Opening %s Channel Output\n", type.c_str());
                    WarningHolder::AddWarning("Could not create output type " + type + ". Check logs for details.");
					continue;
				}

				LogDebug(VB_CHANNELOUT, "Configured %s Channel Output\n", type.c_str());
			}
		}
	}

	channelOutputCount = i;

	LogDebug(VB_CHANNELOUT, "%d Channel Outputs configured\n", channelOutputCount);

	LoadOutputProcessors();
    outputProcessors.GetRequiredChannelRanges([](int m1, int m2) {
        LogInfo(VB_CHANNELOUT, "OutputProcessor:  Determined range needed %d - %d\n", m1, m2);
        addRange(m1, m2);
    });
    if (getControlMajor() || getControlMinor()) {
        int min = std::min(getControlMajor(), getControlMinor());
        int max = std::max(getControlMajor(), getControlMinor());
        addRange(min, max);
    }
    sortRanges();
    for (auto &r : outputRanges) {
        LogInfo(VB_CHANNELOUT, "Determined range needed %d - %d\n", r.first, r.first + r.second - 1);
    }

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
void CloseChannelOutputs(void) {
	int i = 0;

	for (i = channelOutputCount-1; i >= 0; i--) {
		if (channelOutputs[i].outputOld)
			channelOutputs[i].outputOld->close(channelOutputs[i].privData);
		else if (channelOutputs[i].output)
			channelOutputs[i].output->Close();

		if (channelOutputs[i].privData)
			free(channelOutputs[i].privData);
	}
    
    for (i = channelOutputCount-1; i >= 0; i--) {
        if (channelOutputs[i].output) {
            delete channelOutputs[i].output;
            channelOutputs[i].output = NULL;
            if (channelOutputs[i].libHandle) {
                dlclose(channelOutputs[i].libHandle);
            }
        }
    }
}


int LoadOutputProcessors(void) {
	char filename[1024];
	Json::Value root;

	strcpy(filename, getMediaDirectory());
	strcat(filename, "/config/outputprocessors.json");
    
	if (!FileExists(filename))
		return 0;

	LogDebug(VB_CHANNELOUT, "Loading Output Processors.\n");

	if (!LoadJsonFromFile(filename, root))
	{
		LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename);
		return 0;
	}

    outputProcessors.loadFromJSON(root);

	return 1;
}

