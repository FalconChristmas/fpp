/*
 *   WS2801 SPI handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <thread>

#include "common.h"
#include "log.h"
#include "settings.h"

#include "SPIws2801.h"


#define SPIWS2801_MAX_CHANNELS  1530

extern "C" {
    SPIws2801Output *createSPIws2801Output(unsigned int startChannel,
                                           unsigned int channelCount) {
        return new SPIws2801Output(startChannel, channelCount);
    }
}

/////////////////////////////////////////////////////////////////////////////

SPIws2801Output::SPIws2801Output(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_port(-1),
	m_pi36(0),
	m_pi36Data(NULL),
	m_pi36DataSize(0),
    m_spi(nullptr)
{
	LogDebug(VB_CHANNELOUT, "SPIws2801Output::SPIws2801Output(%u, %u)\n",
		startChannel, channelCount);
}

SPIws2801Output::~SPIws2801Output()
{
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
        LogErr(VB_CHANNELOUT, "Unable to open SPI device\n") ;
        delete m_spi;
        m_spi = nullptr;
        return 0;
	}
	return ThreadedChannelOutputBase::Init(config);
}

int SPIws2801Output::Close(void)
{
	LogDebug(VB_CHANNELOUT, "SPIws2801Output::Close()\n");

	return ThreadedChannelOutputBase::Close();
}
void SPIws2801Output::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

int SPIws2801Output::RawSendData(unsigned char *channelData)
{
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

		memcpy(m_pi36Data + ws2801Channels + 18, channelData     , 18);
		memcpy(m_pi36Data + ws2801Channels     , channelData + 18, 18);

		if (ws2801Channels)
			memcpy(m_pi36Data, channelData + 36, ws2801Channels);

        m_spi->xfer(m_pi36Data, nullptr, m_pi36DataSize);
    } else {
        m_spi->xfer(channelData, nullptr, m_channelCount);
	}

	return m_channelCount;
}

void SPIws2801Output::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "SPIws2801Output::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    port    : %d\n", m_port);

	ThreadedChannelOutputBase::DumpConfig();
}

