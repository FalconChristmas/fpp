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

#include "../common.h"
#include "../log.h"

#include "DebugOutput.h"

#include "Plugin.h"
class DebugPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    DebugPlugin() :
        FPPPlugins::Plugin("Debug") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new DebugOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new DebugPlugin();
}
}

/*
 *
 */
DebugOutput::DebugOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "DebugOutput::DebugOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
DebugOutput::~DebugOutput() {
    LogDebug(VB_CHANNELOUT, "DebugOutput::~DebugOutput()\n");
}

/*
 *
 */
int DebugOutput::Init(Json::Value v) {
    LogDebug(VB_CHANNELOUT, "DebugOutput::Init()\n");

    // Call the base class' Init() method, do not remove this line.
    return ChannelOutput::Init(v);
}

/*
 *
 */
int DebugOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "DebugOutput::Close()\n");

    return ChannelOutput::Close();
}

/*
 *
 */
int DebugOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "DebugOutput::RawSendData(%p)\n", channelData);

    HexDump("DebugOutput::RawSendData", channelData, m_channelCount, VB_CHANNELOUT);

    return m_channelCount;
}

/*
 *
 */
void DebugOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "DebugOutput::DumpConfig()\n");

    // Call the base class' DumpConfig() method, do not remove this line.
    ChannelOutput::DumpConfig();
}
