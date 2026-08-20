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

#include "fpp-json.h"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <sstream>
#include <string>

#include "ChannelOutput.h"
#include "ChannelOutputSetup.h"
#include "Sequence.h"
#include "channeloutputthread.h"
#include "Warnings.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "../AtomicSharedPtr.h"
#include "../FileMonitor.h"
#include "../OutputMonitor.h"
#include "../Plugin.h"
#include "../Plugins.h"
#include "../config.h"
#include "../MultiSync.h"

#include "processors/OutputProcessor.h"

/////////////////////////////////////////////////////////////////////////////

class FPPChannelOutputInstance {
public:
    FPPChannelOutputInstance() {}
    ~FPPChannelOutputInstance() {}

    unsigned int startChannel = 0;
    unsigned int channelCount = 0;
    ChannelOutput* output = nullptr;
    std::string sourceFile;

    std::atomic<FPPChannelOutputInstance*> prev;
    std::atomic<FPPChannelOutputInstance*> next;
};

unsigned long channelOutputFrame = 0;
float mediaElapsedSeconds = 0.0;

std::atomic<FPPChannelOutputInstance*> channelOutputs;
std::atomic<FPPChannelOutputInstance*> lastChannelOutput;
inline void addChannelOutput(FPPChannelOutputInstance* inst) {
    // the output thread walks this list under outputThreadLock; linking a node
    // in takes several stores, so publish them all before it can see any
    std::unique_lock<std::mutex> lock(outputThreadLock);
    // An output added while the output thread is already up -- a config reload,
    // which is what an xLights auto-upload does -- missed the StartingOutput()
    // that thread made when it came up, and would otherwise run in the stopped
    // state for as long as it lives: BBShiftPanel leaves its PRUs parked, so
    // the panels stay dark, and PCA9685 never leaves sleep.  The thread sets
    // ThreadIsRunning and makes its own StartingOutput() call under this same
    // lock, so checking here cannot land in between the two and either miss
    // the call or make it twice.
    if (inst->output && ChannelOutputThreadIsRunning()) {
        LogDebug(VB_CHANNELOUT, "Output thread already running, starting output on new %s\n",
                 inst->output->GetOutputType().c_str());
        inst->output->StartingOutput();
    }
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

// NOTE: this and GetOutputTypes() below walk the list from HTTP/MultiSync
// threads without outputThreadLock, so they can still read an instance the
// reload path is freeing.  Taking the lock here is NOT safe: the output thread
// holds it across Sequence::SendSequenceData(), which runs FSEQ-triggered
// commands (Sequence.cpp), and a command that reaches back into either of
// these would deadlock the output thread outright.  Closing this properly
// wants a read-side that cannot block the writer -- an RCU/epoch scheme or a
// shared_ptr snapshot of the list -- rather than this mutex.
bool HasUniverseOutputs() {
    std::string universeFile = FPP_DIR_CONFIG("/co-universes.json");
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        if (inst->sourceFile == universeFile) {
            return true;
        }
    }
    return false;
}

static std::map<std::string, std::set<std::string>> outputLoadWarnings;

using OutputRangeVec = std::vector<std::pair<uint32_t, uint32_t>>;
// Snapshots published by ComputeOutputRanges() and handed out by
// GetOutputRangesSnapshot(). Both are seeded here with the {0,8} default so a
// snapshot is never null and never empty -- readers never mutate these, so no
// lock is needed to see a consistent list while ComputeOutputRanges()
// publishes a new one from another thread.
static AtomicSharedPtr<const OutputRangeVec> outputRanges(std::make_shared<const OutputRangeVec>(OutputRangeVec{ { 0, 8 } }));
static AtomicSharedPtr<const OutputRangeVec> preciseOutputRanges(std::make_shared<const OutputRangeVec>(OutputRangeVec{ { 0, 8 } }));

// Returns a snapshot of the current ranges; the caller should hold the
// returned shared_ptr for the duration of any iteration over it rather than
// calling this repeatedly. Never null, never empty.
std::shared_ptr<const OutputRangeVec> GetOutputRangesSnapshot(bool precise) {
    return precise ? preciseOutputRanges.load() : outputRanges.load();
}

// Plugin-compatibility API (fpp-brightness and fpp-Capture call this; the
// return type is not part of the C++ name mangling, so changing it would let
// an already-compiled plugin misread the return value rather than fail to
// link).  Each calling thread gets its own copy of the snapshot, so the
// reference stays valid and immutable for that thread until IT calls again --
// a recompute on another thread can no longer mutate a list a plugin is
// iterating.  In-tree code should use GetOutputRangesSnapshot() instead.
const OutputRangeVec& GetOutputRanges(bool precise) {
    // One copy per flavor, so holding the precise and non-precise references
    // at the same time works, as it did when these were two distinct vectors.
    static thread_local OutputRangeVec copies[2];
    OutputRangeVec& copy = copies[precise ? 1 : 0];
    copy = *GetOutputRangesSnapshot(precise);
    return copy;
}

std::string GetOutputRangesAsString(bool precise, bool oneBased) {
    std::string ret;
    bool first = true;
    int offset = oneBased ? 1 : 0;
    auto ranges = GetOutputRangesSnapshot(precise);
    for (auto& a : *ranges) {
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
    for (auto& r : normal) {
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
    preciseOutputRanges.store(std::make_shared<const OutputRangeVec>(std::move(precise)));
    outputRanges.store(std::make_shared<const OutputRangeVec>(std::move(normal)));
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
    { "universes", "UDPOutput" },
    // The RPIWS281X driver has been removed.  DPIPixels drives the same pins and
    // maps the old RPIWS281X string configs itself, so old configs load unchanged.
    { "RPIWS281X", "DPIPixels" }
};

bool skipOutputRangeCompute = false;

static bool ReloadChannelOutputsForFile(const std::string& cfgFile) {
    Json::Value root;
    if (FileExists(cfgFile)) {
        if (!LoadJsonFromFile(cfgFile, root, JsonRoot::Object)) {
            if (!skipOutputRangeCompute) {
                // this is a reload, it could still be writing the file so we'll wait a bit
                // and try again
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (!LoadJsonFromFile(cfgFile, root, JsonRoot::Object)) {
                std::string warning = "Could not parse " + cfgFile + ". Some outputs may not work.";
                WarningHolder::AddWarning(26, warning);
                outputLoadWarnings[cfgFile].insert(warning);
                LogErr(VB_CHANNELOUT, "Error parsing %s\n", cfgFile.c_str());
                return false;
            }
        }
    }

    bool changed = false;
    {
        // Unlink and destroy under outputThreadLock.  PrepareChannelData() and
        // SendChannelData() walk this list from the channel output thread,
        // which holds that lock for the whole of its cycle; without it the
        // walk can be sitting inside inst->output->PrepData() when the
        // instance is freed here, and inst = inst->next then reads a freed
        // node as well.  This used to be a 100ms sleep, which is not a
        // barrier at all -- a slow output cycle (the reports that led here had
        // Send: 73ms against a 50ms loop) simply runs past it.
        //
        // Only the teardown is covered.  Init() of the replacement outputs
        // below can take a long time (compileMatrix.sh, PRU firmware loads)
        // and holding the lock across it would stall output for that whole
        // time; a half-populated list is harmless, an unlinked-but-live one is
        // not.
        std::list<FPPChannelOutputInstance*> toDelete;
        std::unique_lock<std::mutex> lock(outputThreadLock);
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
        if (ChannelOutputThreadIsRunning()) {
            // StoppingOutput() is the pair of the StartingOutput() the output
            // thread makes when it comes up, so it is only owed to the outputs
            // if that thread actually ran.
            for (auto inst : toDelete) {
                if (inst->output) {
                    inst->output->StoppingOutput();
                }
            }
        }
        for (auto inst : toDelete) {
            if (inst->output) {
                inst->output->Close();
                delete inst->output;
            }
            delete inst;
        }
    }
    for (auto& warning : outputLoadWarnings[cfgFile]) {
        WarningHolder::RemoveWarning(26, warning);
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
                WarningHolder::AddWarning(26, warning);
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
                    if (channelOutput->output) {
                        // Pins the plugin: the output outlives it and is owned
                        // elsewhere, so the plugin cannot be unloaded.
                        PluginManager::INSTANCE.noteChannelOutputCreated(p);
                    }
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
                        WarningHolder::AddWarning(26, warning);
                        LogErr(VB_CHANNELOUT, warning.c_str());
                        outputLoadWarnings[cfgFile].insert(warning);
                        delete channelOutput->output;
                        channelOutput->output = nullptr;
                    }
                } else {
                    std::string warning = "Could not create output type " + type + ". Check logs for details.";
                    WarningHolder::AddWarning(26, warning);
                    outputLoadWarnings[cfgFile].insert(warning);
                    LogErr(VB_CHANNELOUT, warning.c_str());
                }
            } catch (const std::exception& ex) {
                std::string warning = "Could not initialize output type " + type + ". (" + ex.what() + ")";
                WarningHolder::AddWarning(26, warning);
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
                                         if (LoadJsonFromFile(channelOutputsFile, root, JsonRoot::Object) && root.isMember("channelOutputs")) {
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
                                 LoadJsonFromFile(opfilename, newRoot, JsonRoot::Object);
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
// see the note on HasUniverseOutputs() for why this walk stays unlocked
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
        uint32_t minimumNeededChannel = GetOutputRangesSnapshot(false)->at(0).first;
        char buf[128];
        snprintf(buf, sizeof(buf), "Channel Data starting at channel %d", minimumNeededChannel);
        HexDump(buf, &channelData[minimumNeededChannel], 16, VB_CHANNELDATA);
    }

    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
        if (output) {
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

    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        if (inst->output) {
            inst->output->StartingOutput();
        }
    }
}

/*
 *
 */
void StoppingOutput(void) {
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        if (inst->output) {
            inst->output->StoppingOutput();
        }
    }
    OutputMonitor::INSTANCE.AutoDisableOutputs();
}

/*
 *
 */
void CloseChannelOutputs(void) {
    // Deliberately not taking outputThreadLock, unlike the reload path: this
    // runs at shutdown, after StopChannelOutputThread(), so there is nothing
    // left to race with.  If that thread did wedge and is still holding the
    // lock, blocking here would trade a crash-exit for a hung daemon.
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        if (inst->output) {
            inst->output->Close();
        }
    }
    for (auto inst = channelOutputs.load(); inst != nullptr; inst = inst->next) {
        auto output = inst->output;
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
