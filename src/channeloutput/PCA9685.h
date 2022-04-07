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

#include "ChannelOutputBase.h"
#include "util/I2CUtils.h"

class PCA9685Output : public ChannelOutputBase {
public:
    PCA9685Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~PCA9685Output();

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
    std::string m_i2cDevice;
    int m_frequency;

    class PCA9685Port {
    public:
        int m_min = 1000;
        int m_max = 2000;
        int m_center = 1500;
        int m_dataType = 0;
        int m_zeroBehavior = 0;

        unsigned short m_lastValue;
    };

    PCA9685Port m_ports[16];
};
