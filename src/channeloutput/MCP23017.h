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
#include "util/I2CUtils.h"

class MCP23017Output : public ChannelOutput {
public:
    MCP23017Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~MCP23017Output();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override {
        addRange(m_startChannel, m_startChannel + m_channelCount - 1);
    }

private:
    I2CUtils* i2c;
    int m_deviceID;
};
