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

#include "util/GPIOUtils.h"
#include "util/SPIUtils.h"

class MAX7219MatrixOutput : public ChannelOutput {
public:
    MAX7219MatrixOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~MAX7219MatrixOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    void WriteCommand(uint8_t cmd, uint8_t value);

    int m_panels;
    int m_channelsPerPixel;
    int m_pinCS;

    const PinCapabilities* m_csPin;
    SPIUtils* m_spi;
};
