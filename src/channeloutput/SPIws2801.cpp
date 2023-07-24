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

#include "fpp-pch.h"

#include "../log.h"

#include "SPIws2801.h"
#include <fcntl.h>

#define SPIWS2801_MAX_CHANNELS 1530

#include "Plugin.h"
class SPIws2801Plugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    SPIws2801Plugin() :
        FPPPlugins::Plugin("SPIws2801") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new SPIws2801Output(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new SPIws2801Plugin();
}
}

/////////////////////////////////////////////////////////////////////////////

SPIws2801Output::SPIws2801Output(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount),
    m_port(-1),
    m_pi36(0),
    m_pi36Data(NULL),
    m_pi36DataSize(0),
    m_spi(nullptr) {
    LogDebug(VB_CHANNELOUT, "SPIws2801Output::SPIws2801Output(%u, %u)\n",
             startChannel, channelCount);
}

SPIws2801Output::~SPIws2801Output() {
    LogDebug(VB_CHANNELOUT, "SPIws2801Output::~SPIws2801Output()\n");
    if (m_spi) {
        delete m_spi;
    }
}

int SPIws2801Output::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "SPIws2801Output::Init()\n");
    if (config.isMember("device")) {
        std::string dev = config["device"].asString();
        if (dev == "spidev0.0") {
            m_port = 0;
        } else if (dev == "spidev0.1") {
            m_port = 1;
        }
    }
    if (config.isMember("pi36")) {
        if (config["pi36"].asInt() == 1) {
            m_pi36 = 1;
            m_pi36DataSize = m_channelCount >= 36 ? m_channelCount : 36;
            m_pi36Data = new unsigned char[m_pi36DataSize];
        }
    }
    if (m_port == -1) {
        LogErr(VB_CHANNELOUT, "Invalid Config: No port\n");
        return 0;
    }

    LogDebug(VB_CHANNELOUT, "Using SPI Port %d\n", m_port);
    m_spi = new SPIUtils(m_port, 1000000);
    if (!m_spi->isOk()) {
        LogErr(VB_CHANNELOUT, "Unable to open SPI device\n");
        delete m_spi;
        m_spi = nullptr;
        return 0;
    }
    return ThreadedChannelOutput::Init(config);
}

int SPIws2801Output::Close(void) {
    LogDebug(VB_CHANNELOUT, "SPIws2801Output::Close()\n");

    return ThreadedChannelOutput::Close();
}
void SPIws2801Output::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

int SPIws2801Output::RawSendData(unsigned char* channelData) {
    LogDebug(VB_CHANNELOUT, "SPIws2801Output::RawSendData(%p)\n", channelData);

    if (!m_spi) {
        return 0;
    }

    if (m_pi36) {
        // Hanson Electronics Pi36 controller has 2x WS2803 onboard and a
        // chained output for ws2801 pixels.  WS2803 pull data from the
        // end of the stream while WS2801 pull from the beginning, so we
        // need to swap some channels around before sending.
        int ws2801Channels = m_pi36DataSize - 36;

        memcpy(m_pi36Data + ws2801Channels + 18, channelData, 18);
        memcpy(m_pi36Data + ws2801Channels, channelData + 18, 18);

        if (ws2801Channels)
            memcpy(m_pi36Data, channelData + 36, ws2801Channels);

        m_spi->xfer(m_pi36Data, nullptr, m_pi36DataSize);
    } else {
        m_spi->xfer(channelData, nullptr, m_channelCount);
    }

    return m_channelCount;
}

void SPIws2801Output::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "SPIws2801Output::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    port    : %d\n", m_port);

    ThreadedChannelOutput::DumpConfig();
}
