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

class PCF8574Output : public ChannelOutput {
public:
    PCF8574Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~PCF8574Output();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override {
        addRange(m_startChannel, m_startChannel + m_channelCount - 1);
    }

private:
    I2CUtils* i2c;

    class PCF8574Port {
    public:
        unsigned char m_invert = 0;
        unsigned char m_pinmode = 0;
        unsigned char m_threshold = 128;
        unsigned char m_hysteresisUpper = 192;
        unsigned char m_hysteresisLower = 64;
        unsigned char m_lastState = 0;
    };

    int m_deviceID;
    PCF8574Port m_ports[8];
    unsigned char m_pinOrderingInvert;
    unsigned char m_lastVal = 0xFF;
};
