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

#ifdef HAS_CAPEUTILS
#include "SerialChannelOutput.h"
#endif

class USBRelayOutput : public ChannelOutput
#ifdef HAS_CAPEUTILS
    , public SerialChannelOutput
#endif
{
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

    std::string m_deviceName;
    int m_fd;
};