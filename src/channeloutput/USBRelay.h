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

#include <string>
#include <vector>
#include "fpp-json-fwd.h"

#include "ChannelOutput.h"
#include "SerialChannelOutput.h"

class USBRelayOutput : public ChannelOutput, public SerialChannelOutput {
public:
    USBRelayOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~USBRelayOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    enum RelayType {
        RELAY_DVC_UNKNOWN,
        RELAY_DVC_BIT,
        RELAY_DVC_ICSTATION,
        RELAY_DVC_CH340
    };

    RelayType m_subType;
    int m_relayCount;

    // CH340 only.  -1 means "unknown", so the first frame transmits every relay
    // even if it is asking for the off state.
    std::vector<signed char> m_lastState;

    // One frame's worth of commands, sized for the worst case of every relay
    // changing (4 bytes each), which also covers the bitstream path's 1 byte
    // per 8 relays.
    std::vector<unsigned char> m_outputBuffer;
};
