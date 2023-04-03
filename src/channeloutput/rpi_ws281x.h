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

extern "C" {
#include "../../external/rpi_ws281x/clk.h"
#include "../../external/rpi_ws281x/dma.h"
#include "../../external/rpi_ws281x/gpio.h"
#include "../../external/rpi_ws281x/pwm.h"
#include "../../external/rpi_ws281x/ws2811.h"
}

#include <vector>

#include "PixelString.h"
#include "ThreadedChannelOutput.h"
#include "util/SPIUtils.h"

class RPIWS281xOutput : public ThreadedChannelOutput {
public:
    RPIWS281xOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~RPIWS281xOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType) override;
    virtual bool SupportsTesting() const { return  true; }
private:
    void SetupCtrlCHandler(void);

    std::vector<PixelString*> m_strings;

    int offsets[4];
    SPIUtils* m_spi0;
    SPIUtils* m_spi1;
    uint8_t* m_spi0Data;
    uint8_t* m_spi1Data;
    uint32_t m_spi0DataCount;
    uint32_t m_spi1DataCount;

    int m_testCycle = -1;
    int m_testType = 0;
    float m_testPercent = 0.0f;
};
