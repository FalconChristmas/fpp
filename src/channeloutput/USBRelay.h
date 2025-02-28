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

#include "ChannelOutput.h"
#include "SerialChannelOutput.h"

typedef enum {
    RELAY_DVC_UNKNOWN = 0,
    RELAY_DVC_BIT,
    RELAY_DVC_ICSTATION,
    RELAY_DVC_CH340
} RelayDeviceSubType;

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
    RelayDeviceSubType m_subType;
    int m_relayCount;
    int m_fd;
};