/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "ChannelOutput.h"
#include "ChannelOutputSetup.h"
#include "Sequence.h"
#include "Warnings.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "../OutputMonitor.h"
#include "../Plugin.h"
#include "../Plugins.h"
#include "../config.h"

// old style that still need porting
#include "FPD.h"

#include "processors/OutputProcessor.h"

/////////////////////////////////////////////////////////////////////////////

unsigned long channelOutputFrame = 0;
float mediaElapsedSeconds = 0.0;
std::vector<FPPChannelOutputInstance> channelOutputs;

static int LoadOutputProcessors(void);

OutputProcessors outputProcessors;

static std::vector<std::pair<uint32_t, uint32_t>> outputRanges;
static std::vector<std::pair<uint32_t, uint32_t>> preciseOutputRanges;

const std::vector<std::pair<uint32_t, uint32_t>>& GetOutputRanges(bool precise) {
    if (outputRanges.empty()) {
        outputRanges.push_back(std::pair<uint32_t, uint32_t>(0, 8));
        preciseOutputRanges.push_back(std::pair<uint32_t, uint32_t>(0, 8));
    }
    return precise ? preciseOutputRanges : outputRanges;
}
std::string GetOutputRangesAsString(bool precise, bool oneBased) {
    std::string ret;
    bool first = true;
    int offset = oneBased ? 1 : 0;
    for (auto& a : GetOutputRanges(precise)) {
        if (!first) {
            ret += ",";
        } else {
            first = false;
        }
        ret += std::to_string(a.first + offset) + "-" + std::to_string(a.first + a.second - 1 + offset);
    }
    if (ret == "") {
        ret = "1-8";
    }
    return ret;
}

// we'll sort the ranges that the outputs have registered and combine any overlaps
// or close ranges to keep the range list smaller
static void sortRanges(std::vector<std::pair<uint32_t, uint32_t>>& rngs, bool gaps) {
    std::map<uint32_t, uint32_t> ranges;
    // sort
    for (auto& a : rngs) {
        uint32_t cur = ranges[a.first];
        if (cur) {
            ranges[a.first] = std::max(a.second, cur);
        } else {
            ranges[a.first] = a.second;
        }
    }
    rngs.clear();
    std::pair<uint32_t, uint32_t> cur(FPPD_MAX_CHANNELS, FPPD_MAX_CHANNELS);
    uint32_t gapEliminate = gaps ? 1025 : 0;

    for (auto& a : ranges) {
        if (cur.first == FPPD_MAX_CHANNELS) {
            cur.first = a.first;
            cur.second = a.second;
        } else {
            if (a.first <= (cur.first + cur.second + gapEliminate)) {
                // overlap or within 1025 channels of an overlap, need to combine
                // if the two ranges are "close" (wthin 1025 channels) we'll combine
                // as the overhead of doing ranges wouldn't benefit with a small gap
                uint32_t max = cur.first + cur.second - 1;
                uint32_t amax = a.first + a.second - 1;
                max = std::max(max, amax);
                cur.second = max - cur.first + 1;
            } else {
                rngs.push_back(cur);
                cur.first = a.first;
                cur.second = a.second;
            }
        }
    }
    if (cur.first != FPPD_MAX_CHANNELS) {
        rngs.push_back(cur);
    }
    if (rngs.empty()) {
        cur.first = 0;
        cur.second = 8;
        rngs.push_back(cur);
    }
}
static void addRange(uint32_t min, uint32_t max) {
    for (auto& r : preciseOutputRanges) {
        int rm = r.first + r.second - 1;
        if (min >= r.first && max <= rm) {
            // within the range, don't add it
            return;
        }
    }
    preciseOutputRanges.push_back(std::pair<uint32_t, uint32_t>(min, max - min + 1));

    // having the reads be aligned to intervals of 8 can help performance so
    // we'll expand the range a bit to align things better
    // round minimum down to interval of 8
    min &= 0xFFFFFFF8;
    max += 8;
    max &= 0xFFFFFFF8;
    max -= 1;
    for (auto& r : outputRanges) {
        int rm = r.first + r.second - 1;
        if (min >= r.first && max <= rm) {
            // within the range, don't add it
            return;
        }
    }
    outputRanges.push_back(std::pair<uint32_t, uint32_t>(min, max - min + 1));
}

/////////////////////////////////////////////////////////////////////////////

void ChannelOutputJSON2CSV(Json::Value config, char* configStr) {
    Json::Value::Members memberNames = config.getMemberNames();

    configStr[0] = '\0';

    if (!config.isMember("type")) {
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
    for (int i = 0; i < memberNames.size(); i++) {
        key = memberNames[i];

        if (first) {
            strcat(configStr, ",");
            first = 0;
        } else
            strcat(configStr, ";");

        strcat(configStr, key.c_str());
        strcat(configStr, "=");
        strcat(configStr, config[key].asString().c_str());
    }
}

// in some of these cases, we could symlink the shlib
static std::map<std::string, std::string> OUTPUT_REMAPS = {
    { "VirtualMatrix", "FBMatrix" },
    { "DMX-Pro", "USBDMX" },
    { "DMX-Open", "USBDMX" },
    { "Pixelnet-Lynx", "USBPixelnet" },
    { "Pixelnet-Open", "USBPixelnet" },
    { "universes", "UDPOutput" }
};

/*
 *
 */
int InitializeChannelOutputs(void) {
    Json::Value root;

    channelOutputFrame = 0;
    channelOutputs.clear();

    // Reset index so we can start populating the outputs array
    if (FPDOutput.isConfigured()) {
        FPPChannelOutputInstance inst;
        inst.startChannel = getSettingInt("FPDStartChannelOffset");
        inst.outputOld = &FPDOutput;

        if (FPDOutput.open("", &inst.privData)) {
            inst.channelCount = inst.outputOld->maxChannels(inst.privData);

            int m1 = inst.startChannel;
            int m2 = m1 + inst.channelCount - 1;
            LogInfo(VB_CHANNELOUT, "FPD:  Determined range needed %d - %d\n",
                    m1, m2);

            addRange(m1, m2);

            channelOutputs.push_back(inst);
            LogDebug(VB_CHANNELOUT, "Configured FPD Channel Output\n");
        } else {
            LogErr(VB_CHANNELOUT, "ERROR Opening FPD Channel Output\n");
        }
    }

    // FIXME, build this list dynamically
    const char* configFiles[] = {
        "/co-universes.json",
        "/co-other.json",
        "/co-pixelStrings.json",
        "/co-bbbStrings.json",
        "/channeloutputs.json",
        NULL
    };

    FILE* fp;
    char filename[1024];
    char csvConfig[2048];

    // Parse the JSON channel outputs config files
    for (int f = 0; configFiles[f]; f++) {
        strcpy(filename, FPP_DIR_CONFIG(configFiles[f]).c_str());

        LogDebug(VB_CHANNELOUT, "Loading %s\n", filename);

        if (FileExists(filename)) {
            if (!LoadJsonFromFile(filename, root)) {
                WarningHolder::AddWarning("Could not parse " + std::string(filename) + ". Some outputs may not work.");
                LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename);
            }

            const Json::Value outputs = root["channelOutputs"];
            std::string type;
            int start = 0;
            int count = 0;

            for (int c = 0; c < outputs.size(); c++) {
                csvConfig[0] = '\0';

                type = outputs[c]["type"].asString();

                if (!outputs[c]["enabled"].asInt()) {
                    LogDebug(VB_CHANNELOUT, "Skipping Disabled Channel Output: %s\n", type.c_str());
                    continue;
                }

                start = outputs[c]["startChannel"].asInt();
                count = outputs[c]["channelCount"].asInt();

                // internally we start channel counts at zero
                start -= 1;

                FPPChannelOutputInstance channelOutput;

                channelOutput.startChannel = start;
                channelOutput.channelCount = count;
                std::string libnamePfx = "";

                // First some Channel Outputs enabled everythwere
                if (type == "LEDPanelMatrix") {
                    // for LED matrices, the driver is determined by the subType
                    libnamePfx = "matrix-";
                    type = outputs[c]["subType"].asString();
                } else if (OUTPUT_REMAPS.find(type) != OUTPUT_REMAPS.end()) {
                    type = OUTPUT_REMAPS[type];
                }

                FPPPlugins::ChannelOutputPlugin* p = dynamic_cast<FPPPlugins::ChannelOutputPlugin*>(PluginManager::INSTANCE.findPlugin(libnamePfx + type, "fpp-co-" + libnamePfx + type));
                if (p) {
                    ChannelOutput* co = p->createChannelOutput(start, count);
                    channelOutput.output = co;
                }
                if (channelOutput.output) {
                    if (channelOutput.output->Init(outputs[c])) {
                        channelOutput.output->GetRequiredChannelRanges([type](int m1, int m2) {
                            LogInfo(VB_CHANNELOUT, "%s:  Determined range needed %d - %d\n",
                                    type.c_str(), m1, m2);
                            addRange(m1, m2);
                        });
                        channelOutputs.push_back(channelOutput);
                    } else {
                        WarningHolder::AddWarning("Could not initialize output type " + type + ". Check logs for details.");
                        delete channelOutput.output;
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

    LogDebug(VB_CHANNELOUT, "%d Channel Outputs configured\n", channelOutputs.size());

    LoadOutputProcessors();
    outputProcessors.GetRequiredChannelRanges([](int m1, int m2) {
        LogInfo(VB_CHANNELOUT, "OutputProcessor:  Determined range needed %d - %d\n", m1, m2);
        addRange(m1, m2);
    });
    sortRanges(outputRanges, false);
    sortRanges(preciseOutputRanges, true);
    for (auto& r : preciseOutputRanges) {
        LogInfo(VB_CHANNELOUT, "Determined range needed %d - %d\n", r.first, r.first + r.second - 1);
    }

    return 1;
}

/*
 * Set the channel output frame counter to a specific value
 */
void SetChannelOutputFrameNumber(int frameNumber) {
    channelOutputFrame = frameNumber;
}

/*
 * Reset the output frame count
 */
void ResetChannelOutputFrameNumber(void) {
    channelOutputFrame = 0;
    mediaElapsedSeconds = 0.0;
}

void OverlayOutputTestData(std::set<std::string> types, unsigned char* channelData, int cycleCnt, float percentOfCycle, int testType) {
    for (auto& inst : channelOutputs) {
        if (inst.output && inst.output->SupportsTesting() && (types.empty() || types.find(inst.output->GetOutputType()) != types.end())) {
            inst.output->OverlayTestData(channelData, cycleCnt, percentOfCycle, testType);
        }
    }
}
std::set<std::string> GetOutputTypes() {
    std::set<std::string> ret;
    for (auto& inst : channelOutputs) {
        if (inst.output && inst.output->SupportsTesting()) {
            ret.insert(inst.output->GetOutputType());
        }
    }
    return ret;
}
int PrepareChannelData(char* channelData) {
    outputProcessors.ProcessData((unsigned char*)channelData);
    for (auto& inst : channelOutputs) {
        if (inst.output) {
            inst.output->PrepData((unsigned char*)channelData);
        }
    }
    return 0;
}

/*
 *
 */
int SendChannelData(const char* channelData) {
    int i = 0;
    FPPChannelOutputInstance* inst;

    if (WillLog(LOG_DEBUG, VB_CHANNELDATA)) {
        uint32_t minimumNeededChannel = GetOutputRanges(false)[0].first;
        char buf[128];
        snprintf(buf, sizeof(buf), "Channel Data starting at channel %d", minimumNeededChannel);
        HexDump(buf, &channelData[minimumNeededChannel], 16, VB_CHANNELDATA);
    }

    for (auto& inst : channelOutputs) {
        if (inst.outputOld) {
            inst.outputOld->send(
                inst.privData,
                channelData + inst.startChannel,
                inst.channelCount < (FPPD_MAX_CHANNELS - inst.startChannel) ? inst.channelCount : (FPPD_MAX_CHANNELS - inst.startChannel));
        } else if (inst.output) {
            inst.output->SendData((unsigned char*)(channelData + inst.startChannel));
        }
    }

    return 0;
}

/*
 *
 */
void StartingOutput(void) {
    OutputMonitor::INSTANCE.AutoEnableOutputs();

    FPPChannelOutputInstance* output;
    int i = 0;

    for (auto& inst : channelOutputs) {
        if ((inst.outputOld) &&
            (inst.outputOld->startThread)) {
            // old style outputs
            inst.outputOld->startThread(inst.privData);
        } else if (inst.output) {
            inst.output->StartingOutput();
        }
    }
}

/*
 *
 */
void StoppingOutput(void) {
    FPPChannelOutputInstance* output;
    int i = 0;

    for (auto& inst : channelOutputs) {
        if ((inst.outputOld) &&
            (inst.outputOld->stopThread)) {
            inst.outputOld->stopThread(inst.privData);
        } else if (inst.output) {
            inst.output->StoppingOutput();
        }
    }

    OutputMonitor::INSTANCE.AutoDisableOutputs();
}

/*
 *
 */
void CloseChannelOutputs(void) {
    int i = 0;

    for (i = channelOutputs.size() - 1; i >= 0; i--) {
        if (channelOutputs[i].outputOld)
            channelOutputs[i].outputOld->close(channelOutputs[i].privData);
        else if (channelOutputs[i].output)
            channelOutputs[i].output->Close();

        if (channelOutputs[i].privData)
            free(channelOutputs[i].privData);
    }

    for (i = channelOutputs.size() - 1; i >= 0; i--) {
        if (channelOutputs[i].output) {
            delete channelOutputs[i].output;
            channelOutputs[i].output = NULL;
        }
    }
}

int LoadOutputProcessors(void) {
    Json::Value root;
    std::string filename = FPP_DIR_CONFIG("/outputprocessors.json");

    if (!FileExists(filename))
        return 0;

    LogDebug(VB_CHANNELOUT, "Loading Output Processors.\n");

    if (!LoadJsonFromFile(filename, root)) {
        LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename.c_str());
        return 0;
    }

    outputProcessors.loadFromJSON(root);

    return 1;
}
