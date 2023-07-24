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

/*
 * This Channel Output is for SPI-attached MAX7219 chips connected to
 * 8x8 LED panels.  The chips/panels may be daisy-chained to provide
 * wider displays.
 *
 * Sample channeloutputs.json config
 *
 * {
 *       "channelOutputs": [
 *               {
 *                       "type": "MAX7219Matrix",
 *                       "enabled": 1,
 *                       "startChannel": 1,
 *                       "channelCount": 128,
 *                       "panels": 2,
 *                       "channelsPerPixel": 1
 *               }
 *       ]
 * }
 *
 */
#include "fpp-pch.h"

#include <unistd.h>

#include "../log.h"

#include "MAX7219Matrix.h"

#define MAX7219_DECODE_MODE 0x09
#define MAX7219_SHUTDOWN 0x0C
#define MAX7219_BRIGHTNESS 0x0A
#define MAX7219_SCAN_LIMIT 0x0B
#define MAX7219_TEST 0x0F

#include "Plugin.h"
class MAX7219MatrixPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    MAX7219MatrixPlugin() :
        FPPPlugins::Plugin("MAX7219Matrix") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new MAX7219MatrixOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new MAX7219MatrixPlugin();
}
}

/*
 *
 */
MAX7219MatrixOutput::MAX7219MatrixOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    m_panels(1),
    m_channelsPerPixel(1),
    m_pinCS(8),
    m_csPin(nullptr),
    m_spi(nullptr) {
    LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::MAX7219MatrixOutput(%u, %u)\n",
             startChannel, channelCount);

    m_csPin = PinCapabilities::getPinByGPIO(8).ptr();
}

/*
 *
 */
MAX7219MatrixOutput::~MAX7219MatrixOutput() {
    LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::~MAX7219MatrixOutput()\n");

    Close();
    if (m_spi) {
        delete m_spi;
    }
}

/*
 *
 */
int MAX7219MatrixOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::Init(JSON)\n");

    m_spi = new SPIUtils(0, 1000000);
    if (!m_spi->isOk()) {
        LogErr(VB_CHANNELOUT, "SPIUtils setup failed\n");
        return 0;
    }

    m_panels = config["panels"].asInt();
    m_channelsPerPixel = config["channelsPerPixel"].asInt();

    m_csPin->configPin("gpio", true);

    usleep(50000);

    WriteCommand(MAX7219_DECODE_MODE, 0x00); // Use LED Matrix, not BCD Mode
    WriteCommand(MAX7219_BRIGHTNESS, 0x00);  // Lowest brightness
    WriteCommand(MAX7219_SCAN_LIMIT, 0x07);  // Use all 8 columns
    WriteCommand(MAX7219_SHUTDOWN, 0x01);    // (1 == On, 0 == Off)
    WriteCommand(MAX7219_TEST, 0x00);        // Turn Off Test mode

    return ChannelOutput::Init(config);
}

void MAX7219MatrixOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_channelCount - 1);
}

/*
 *
 */
int MAX7219MatrixOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::Close()\n");

    return ChannelOutput::Close();
}

/*
 *
 */
void MAX7219MatrixOutput::WriteCommand(uint8_t cmd, uint8_t value) {
    uint8_t c;
    uint8_t v;
    uint8_t data[256];
    int bytes = 0;

    for (int p = 0; p < m_panels; p++) {
        data[bytes++] = cmd;
        data[bytes++] = value;
    }

    m_csPin->setValue(0);
    m_spi->xfer(data, nullptr, bytes);
    m_csPin->setValue(1);
}

/*
 *
 */
int MAX7219MatrixOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "MAX7219MatrixOutput::SendData(%p)\n",
              channelData);

    uint8_t data[256];

    // Line by line, send data starting with the last panel first and
    // left most pixel on a panel is MSB
    int c = 0;
    for (int i = 1; i < 9; i++) {
        int bytes = 0;
        int offset = m_panels * 8 * m_channelsPerPixel;

        for (int p = 0; p < m_panels; p++) {
            data[bytes++] = i;

            uint8_t ch = 0;
            for (int j = 0; j < 8; j++) {
                if (m_channelsPerPixel == 1) {
                    if (channelData[c + offset - 1])
                        ch |= 0x01 << j;
                } else { // 3 channels per pixel
                    if ((channelData[c + offset - 1]) ||
                        (channelData[c + offset - 2]) ||
                        (channelData[c + offset - 3]))
                        ch |= 0x01 << j;
                }
                offset -= m_channelsPerPixel;
            }
            data[bytes++] = ch;
        }

        m_csPin->setValue(0);
        m_spi->xfer(data, nullptr, bytes);
        m_csPin->setValue(1);

        c += m_panels * 8 * m_channelsPerPixel;
    }

    return m_channelCount;
}

/*
 *
 */
void MAX7219MatrixOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::DumpConfig()\n");

    ChannelOutput::DumpConfig();
}
