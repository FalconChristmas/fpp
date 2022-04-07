#pragma once
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

#include "ChannelOutput.h"

/*
 * This DebugOutput Channel Output driver is provided for debugging
 * purposes and as a template which can be used when creating new
 * Channel Output drivers for FPP.  It reimpliments the minimal
 * methods necessary from the ChannelOutput class which is is
 * derived from.
 */

class DebugOutput : public ChannelOutput {
public:
    DebugOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~DebugOutput();

    virtual int Init(Json::Value config) override;

    // Close the derived class.  This method must also call the
    // base class Close() method.
    virtual int Close(void) override;

    // Main routine to send channel data out
    virtual int SendData(unsigned char* channelData) override;

    // Dump the config variables for debugging.  This method must
    // also call the base class DumpConfig() method.
    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override {
        addRange(m_startChannel, m_startChannel + m_channelCount - 1);
    }
};
