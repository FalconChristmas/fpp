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

#include "ThreadedChannelOutput.h"

class ILI9488Output : public ThreadedChannelOutput {
public:
    ILI9488Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~ILI9488Output();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override {
        addRange(m_startChannel, m_startChannel + m_pixels * 3 - 1);
    }

private:
    int m_initialized;
    int m_rows;
    int m_cols;
    int m_pixels;
    int m_bpp;
    void* m_gpio_map;

    unsigned int m_dataValues[256];
    unsigned int m_clearWRXDataBits;
    unsigned int m_bitDCX;
    unsigned int m_bitWRX;

    volatile unsigned int* m_gpio;

    void ILI9488_Init(void);
    void ILI9488_SendByte(unsigned char byte);
    void ILI9488_Command(unsigned char cmd);
    void ILI9488_Data(unsigned char cmd);
    void ILI9488_Cleanup(void);

    void SetColumnRange(unsigned int x1, unsigned int x2);
    void SetRowRange(unsigned int y1, unsigned int y2);
    void SetRegion(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2);
};
