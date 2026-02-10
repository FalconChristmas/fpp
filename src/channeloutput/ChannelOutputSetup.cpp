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
#include "../FileMonitor.h"
#include "../OutputMonitor.h"
#include "../Plugin.h"
#include "../Plugins.h"
#include "../config.h"
#include "../MultiSync.h"

// old style that still need porting
#include "FPD.h"

#include "processors/OutputProcessor.h"

/////////////////////////////////////////////////////////////////////////////

class FPPChannelOutputInstance {
public:
    FPPChannelOutputInstance() {}
    ~FPPChannelOutputInstance() {}

    unsigned int startChannel = 0;
    unsigned int channelCount = 0;
    FPPChannelOutput* outputOld = nullptr;
    ChannelOutput* output = nullptr;
    void* privData = nullptr;
    std::string sourceFile;

    std::atomic<FPPChannelOutputInstance*> prev;
    std::atomic<FPPChannelOutputInstance*> next;
};

unsigned long channelOutputFrame = 0;
float mediaElapsedSeconds = 0.0;

std::atomic<FPPChannelOutputInstance*> channelOutputs;
std::atomic<FPPChannelOutputInstance*> lastChannelOutput;
inline void addChannelOutput(FPPChannelOutputInstance* inst) {
    inst->prev = lastChannelOutput.load();
    if (lastChannelOutput) {
        lastChannelOutput.load()->next = inst;
    } else {
        channelOutputs = inst;
    }
    lastChannelOutput = inst;
}

OutputProcessors outputProcessors;

bool HasChannelOutputs() {
    return channelOutputs.load() != nullptr;
}

static std::map<std::string, std::set<std::string>> outputLoadWarnings;
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
static void addRange(uint32_t min, uint32_t max, std::vector<std::pair<uint32_t, uint32_t>>& prec, std::vector<std::pair<uint32_t, uint32_t>>& normal) {
    for (auto& r : prec) {
        int rm = r.first + r.second - 1;
        if (min >= r.first && max <= rm) {
            // within the range, don't add it
            return;
        }
    }
    prec.emplace_back(min, max - min + 1);

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
    normal.emplace_back(min, max - min + 1);
}

static void ComputeOutputRanges() {
    std::vector<std::pair<uint32_t, uint32_t>> normal;
    std::vector<std::pair<uint32_t, uint32_t>> precise;
    FPPChannelOutputInstance* co = channelOutputs;
    while (co != nullptr) {
        if (co->output) {
            co->output->GetRequiredChannelRanges([&precise, &normal, &co](int m1, int m2) {
                LogInfo(VB_CHANNELOUT, "%s: Determined range needed %d - %d\n", co->output->GetOutputType().c_str(), m1, m2);
                addRange(m1, m2, precise, normal);
            });
        } else if (co->outputOld) {
            // old style outputs
            int m1 = co->startChannel;
            int m2 = m1 + co->channelCount - 1;
            LogInfo(VB_CHANNELOUT, "ChannelOutput:  Determined range needed %d - %d\n", m1, m2);
            addRange(m1, m2, precise, normal);
        }
        co = co->next;
    }
    outputProcessors.GetRequiredChannelRanges([&precise, &normal](int m1, int m2) {
        LogInfo(VB_CHANNELOUT, "OutputProcessor:  Determined range needed %d - %d\n", m1, m2);
        addRange(m1, m2, precise, normal);
    });
    sortRanges(normal, false);
    sortRanges(precise, true);
    for (auto& r : precise) {
        LogInfo(VB_CHANNELOUT, "Determined range needed %d - %d\n", r.first, r.first + r.second - 1);
    }
    preciseOutputRanges.swap(precise);
    outputRanges.swap(normal);
    MultiSync::INSTANCE.Ping();
    MultiSync::INSTANCE.WriteRuntimeInfoFile();
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

extern int ChannelOutputThreadIsRunning(void);
bool skipOutputRangeCompute = false;

static bool ReloadChannelOutputsForFile(const std::string& cfgFile) {
    Json::Value root;
    if (FileExists(cfgFile)) {
        if (!LoadJsonFromFile(cfgFile, root)) {
            if (!skipOutputRangeCompute) {
                // this is a reload, it could still be writing the file so we'll wait a bit
                // and try again
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (!LoadJsonFromFile(cfgFile, root)) {
                std::string warning = "Could not parse " + cfgFile + ". Some outputs may not work.";    
                WarningHolder::AddWarning(warning);
                outputLoadWarnings[cfgFile].insert(warning);
                LogErr(VB_CHANNELOUT, "Error parsing %s\n", cfgFile.c_str());
                return false;
            }
        }
    }

    bool changed = false;
    std::list<FPPChannelOutputInstance*> toDelete;
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        // remove everything in the list that was loaded from the cfgFile
        if (inst->sourceFile == cfgFile) {
            changed = true;
            toDelete.push_back(inst);
            if (inst->next) {
                inst->next.load()->prev = inst->prev.load();
            }
            if (inst->prev) {
                inst->prev.load()->next = inst->next.load();
            }
            if (inst == channelOutputs.load()) {
                channelOutputs = inst->next.load();
            }
            if (inst == lastChannelOutput.load()) {
                lastChannelOutput = inst->prev.load();
            }
        }
    }
    if (!toDelete.empty() && ChannelOutputThreadIsRunning()) {
        // if the channel output thread is running, we need to wait at least one cycle
        // before we can delete the outputs, this isn't ideal, but we don't want to
        // delete the outputs while the thread is running as it may be using them
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (auto inst : toDelete) {
            if ((inst->outputOld) &&
                (inst->outputOld->stopThread)) {
                inst->outputOld->stopThread(inst->privData);
            } else if (inst->output) {
                inst->output->StoppingOutput();
            }
        }
    }
    for (auto inst : toDelete) {
        if (inst->outputOld) {
            inst->outputOld->close(inst->privData);
            free(inst->privData);
        } else if (inst->output) {
            inst->output->Close();
            delete inst->output;
        }
        delete inst;
    }
    for (auto& warning : outputLoadWarnings[cfgFile]) {
        WarningHolder::RemoveWarning(warning);
    }
    outputLoadWarnings.erase(cfgFile);
    if (FileExists(cfgFile)) {
        const Json::Value outputs = root["channelOutputs"];
        std::string type;
        int start = 0;
        int count = 0;

        for (int c = 0; c < outputs.size(); c++) {
            type = outputs[c]["type"].asString();

            if (!outputs[c]["enabled"].asInt()) {
                LogDebug(VB_CHANNELOUT, "Skipping Disabled Channel Output: %s\n", type.c_str());
                continue;
            }

            start = outputs[c]["startChannel"].asInt();
            count = outputs[c]["channelCount"].asInt();

            // internally we start channel counts at zero
            start -= 1;
            if (start < 0 && count > 0) {
                // we have a negative channel number, but actually are supposed to be outputting data
                // Skip this output as that's not valid
                std::string warning = "Could not initialize output type " + type + ". Invalid start channel.";
                WarningHolder::AddWarning(warning);
                outputLoadWarnings[cfgFile].insert(warning);
                LogInfo(VB_CHANNELOUT, "%s\n", warning.c_str());
                continue;
            }

            FPPChannelOutputInstance* channelOutput = new FPPChannelOutputInstance();

            channelOutput->startChannel = start;
            channelOutput->channelCount = count;
            channelOutput->sourceFile = cfgFile;
            std::string libnamePfx = "";

            // First some Channel Outputs enabled everythwere
            if (type == "LEDPanelMatrix") {
                // for LED matrices, the driver is determined by the subType
                libnamePfx = "matrix-";
                type = outputs[c]["subType"].asString();
            } else if (OUTPUT_REMAPS.find(type) != OUTPUT_REMAPS.end()) {
                type = OUTPUT_REMAPS[type];
            }

            try {
                LogInfo(VB_CHANNELOUT, "Loading output plugin: type=%s, libname=%s\n", type.c_str(), ("fpp-co-" + libnamePfx + type).c_str());
                FPPPlugins::ChannelOutputPlugin* p = dynamic_cast<FPPPlugins::ChannelOutputPlugin*>(PluginManager::INSTANCE.findPlugin(libnamePfx + type, "fpp-co-" + libnamePfx + type));
                if (p) {
                    LogInfo(VB_CHANNELOUT, "Plugin found, creating output: start=%d, count=%d\n", start, count);
                    channelOutput->output = p->createChannelOutput(start, count);
                } else {
                    LogWarn(VB_CHANNELOUT, "Plugin NOT found for type: %s\n", type.c_str());
                }
                if (channelOutput->output) {
                    LogInfo(VB_CHANNELOUT, "Calling Init() on %s output\n", type.c_str());
                    if (channelOutput->output->Init(outputs[c])) {
                        LogInfo(VB_CHANNELOUT, "Init succeeded, adding to channel outputs\n");
                        addChannelOutput(channelOutput);
                        LogDebug(VB_CHANNELOUT, "Configured %s Channel Output\n", type.c_str());
                        channelOutput = nullptr;
                        changed = true;
                    } else {
                        std::string warning = "Could not initialize output type " + type + ". Check logs for details.";
                        WarningHolder::AddWarning(warning);
                        LogErr(VB_CHANNELOUT, warning.c_str());
                        outputLoadWarnings[cfgFile].insert(warning);
                        delete channelOutput->output;
                        channelOutput->output = nullptr;
                    }
                } else {
                    std::string warning = "Could not create output type " + type + ". Check logs for details.";
                    WarningHolder::AddWarning(warning);
                    outputLoadWarnings[cfgFile].insert(warning);
                    LogErr(VB_CHANNELOUT, warning.c_str());
                }
            } catch (const std::exception& ex) {
                std::string warning = "Could not initialize output type " + type + ". (" + ex.what() + ")";
                WarningHolder::AddWarning(warning);
                LogErr(VB_CHANNELOUT, warning.c_str());
                outputLoadWarnings[cfgFile].insert(warning);
                if (channelOutput && channelOutput->output) {
                    delete channelOutput->output;
                    channelOutput->output = nullptr;
                }
            }
            if (channelOutput) {
                // if we didn't add the channel output, delete it
                delete channelOutput;
            }
        }
    }
    return changed;
}

int InitializeChannelOutputs(void) {
    Json::Value root;

    channelOutputFrame = 0;

    if (FPDOutput.isConfigured()) {
        FPPChannelOutputInstance* inst = new FPPChannelOutputInstance();
        inst->startChannel = getSettingInt("FPDStartChannelOffset");
        inst->outputOld = &FPDOutput;
        inst->sourceFile = "FPD";

        if (FPDOutput.open("", &inst->privData)) {
            inst->channelCount = inst->outputOld->maxChannels(inst->privData);
            addChannelOutput(inst);
            LogDebug(VB_CHANNELOUT, "Configured FPD Channel Output\n");
        } else {
            delete inst;
            LogErr(VB_CHANNELOUT, "ERROR Opening FPD Channel Output\n");
        }
    }

    // FIXME, build this list dynamically
    const char* configFiles[] = {
        "/co-universes.json",
        "/co-other.json",
        "/co-pwm.json",
#if defined(PLATFORM_BBB) || defined(PLATFORM_BB64)
        "/co-bbbStrings.json",
#else
        "/co-pixelStrings.json",
#endif
        "/channeloutputs.json",
        NULL
    };
    skipOutputRangeCompute = true;
    for (int f = 0; configFiles[f]; f++) {
        std::string configFile = FPP_DIR_CONFIG(configFiles[f]);
        FileMonitor::INSTANCE.AddFile("ChannelOutputSetup", configFile, [configFile]() {
                                 LogDebug(VB_CHANNELOUT, "Reloading Channel Outputs for %s\n", configFile.c_str());
                                 bool changed = ReloadChannelOutputsForFile(configFile);
#if defined(PLATFORM_BBB) || defined(PLATFORM_BB64)
                                 // On BBB platforms, if co-bbbStrings.json changes and we have BBBMatrix outputs,
                                 // we need to reload channeloutputs.json too so compileMatrix.sh gets called
                                 if (changed && configFile.find("co-bbbStrings.json") != std::string::npos) {
                                     std::string channelOutputsFile = FPP_DIR_CONFIG("/channeloutputs.json");
                                     if (FileExists(channelOutputsFile)) {
                                         Json::Value root;
                                         if (LoadJsonFromFile(channelOutputsFile, root) && root.isMember("channelOutputs")) {
                                             for (int i = 0; i < root["channelOutputs"].size(); i++) {
                                                 if (root["channelOutputs"][i]["type"].asString() == "LEDPanelMatrix" &&
                                                     root["channelOutputs"][i]["subType"].asString() == "BBBMatrix" &&
                                                     root["channelOutputs"][i]["enabled"].asInt() == 1) {
                                                     LogDebug(VB_CHANNELOUT, "BBBMatrix output detected, reloading %s to ensure PRU code is compiled\n", channelOutputsFile.c_str());
                                                     ReloadChannelOutputsForFile(channelOutputsFile);
                                                     break;
                                                 }
                                             }
                                         }
                                     }
                                 }
#endif
                                 if (changed && !skipOutputRangeCompute) {
                                     ComputeOutputRanges();
                                 }
                             })
            .TriggerFileChanged(configFile);
    }
    skipOutputRangeCompute = false;

    int count = 0;
    FPPChannelOutputInstance* co = channelOutputs;
    while (co != nullptr) {
        count++;
        co = co->next;
    }
    LogDebug(VB_CHANNELOUT, "%d Channel Outputs configured\n", count);
    std::string opfilename = FPP_DIR_CONFIG("/outputprocessors.json");
    FileMonitor::INSTANCE.AddFile("outputprocessors.json", opfilename, [opfilename]() {
                             Json::Value newRoot;
                             if (FileExists(opfilename)) {
                                 LoadJsonFromFile(opfilename, newRoot);
                             }
                             outputProcessors.loadFromJSON(newRoot);
                             ComputeOutputRanges();
                         })
        .TriggerFileChanged(opfilename);
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

void OverlayOutputTestData(std::set<std::string> types, unsigned char* channelData, int cycleCnt, float percentOfCycle, int testType, const Json::Value& extraConfig) {
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
        if (output && output->SupportsTesting() && (types.empty() || types.find(output->GetOutputType()) != types.end())) {
            output->OverlayTestData(channelData, cycleCnt, percentOfCycle, testType, extraConfig);
        }
    }
}
std::set<std::string> GetOutputTypes() {
    std::set<std::string> ret;
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
        if (output && output->SupportsTesting()) {
            ret.insert(output->GetOutputType());
        }
    }
    return ret;
}
int PrepareChannelData(char* channelData) {
    outputProcessors.ProcessData((unsigned char*)channelData);
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
        if (output) {
            output->PrepData((unsigned char*)channelData);
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

    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
        if (inst->outputOld) {
            inst->outputOld->send(
                inst->privData,
                channelData + inst->startChannel,
                inst->channelCount < (FPPD_MAX_CHANNELS - inst->startChannel) ? inst->channelCount : (FPPD_MAX_CHANNELS - inst->startChannel));
        } else if (output) {
            output->SendData((unsigned char*)(channelData + inst->startChannel));
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

    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
        if ((inst->outputOld) &&
            (inst->outputOld->startThread)) {
            // old style outputs
            inst->outputOld->startThread(inst->privData);
        } else if (output) {
            output->StartingOutput();
        }
    }
}

/*
 *
 */
void StoppingOutput(void) {
    FPPChannelOutputInstance* output;
    int i = 0;

    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
        if ((inst->outputOld) &&
            (inst->outputOld->stopThread)) {
            inst->outputOld->stopThread(inst->privData);
        } else if (output) {
            output->StoppingOutput();
        }
    }
    OutputMonitor::INSTANCE.AutoDisableOutputs();
}

/*
 *
 */
void CloseChannelOutputs(void) {
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
        if (inst->outputOld) {
            inst->outputOld->close(inst->privData);
        } else if (output) {
            output->Close();
        }
    }
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
        if (inst->privData) {
            free(inst->privData);
            inst->privData = nullptr;
        }
        if (output) {
            inst->output = nullptr;
            delete output;
        }
    }
    while (channelOutputs.load() != nullptr) {
        FPPChannelOutputInstance* inst = channelOutputs.load();
        channelOutputs = inst->next.load();
        delete inst;
    }
}
