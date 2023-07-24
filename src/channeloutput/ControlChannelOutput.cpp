/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2023 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include "../log.h"

#include "../commands/Commands.h"

#include "ControlChannelOutput.h"

#include "Plugin.h"
class ControlChannelOutputPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    ControlChannelOutputPlugin() :
        FPPPlugins::Plugin("ControlChannelOutput") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new ControlChannelOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new ControlChannelOutputPlugin();
}
}

ControlChannelOutput::ControlChannelOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount) {
}
ControlChannelOutput::~ControlChannelOutput() {
}

int ControlChannelOutput::Init(Json::Value config) {
    if (config.isMember("values")) {
        for (int x = 0; x < config["values"].size(); x++) {
            uint8_t v = config["values"][x]["value"].asUInt();
            std::string p = config["values"][x]["preset"].asString();
            if (p != "") {
                presets[v].emplace_back(p);
            }
        }
    }

    return ChannelOutput::Init(config);
}

int ControlChannelOutput::SendData(unsigned char* channelData) {
    uint8_t v = channelData[m_startChannel];
    if (v != lastValue) {
        lastValue = v;
        for (auto& p : presets[v]) {
            if (p != "") {
                CommandManager::INSTANCE.TriggerPreset(p);
            }
        }
    }
    return 0;
}

void ControlChannelOutput::DumpConfig(void) {
    ChannelOutput::DumpConfig();
    for (auto& p : presets) {
        std::string v = "    Value " + std::to_string(p.first) + ": ";
        for (auto& a : p.second) {
            v += " \"";
            v += a;
            v += "\"";
        }
        LogDebug(VB_CHANNELOUT, "%s\n", v.c_str());
    }
}