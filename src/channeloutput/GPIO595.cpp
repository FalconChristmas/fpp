/*
 *   GPIO attached 74HC595 Shift Register handler for Falcon Player (FPP)
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
#include <chrono>
#include <thread>

#include "common.h"
#include "log.h"
#include "settings.h"

#include "GPIO595.h"


#define GPIO595_MAX_CHANNELS  128

/////////////////////////////////////////////////////////////////////////////


extern "C" {
    GPIO595Output *createGPIO_595Output(unsigned int startChannel,
                                    unsigned int channelCount) {
        return new GPIO595Output(startChannel, channelCount);
    }
}

/*
 *
 */
GPIO595Output::GPIO595Output(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_clockPin(nullptr),
	m_dataPin(nullptr),
	m_latchPin(nullptr)
{
	LogDebug(VB_CHANNELOUT, "GPIO595Output::GPIO595Output(%u, %u)\n",
		startChannel, channelCount);

}

/*
 *
 */
GPIO595Output::~GPIO595Output()
{
	LogDebug(VB_CHANNELOUT, "GPIO595Output::~GPIO595Output()\n");
}
int GPIO595Output::Init(Json::Value config) {

	LogDebug(VB_CHANNELOUT, "GPIO595Output::Init()\n");

    if (config.isMember("clockPin")) {
        int p = config["clockPin"].asInt();
        m_clockPin = PinCapabilities::getPinByGPIO(p).ptr();
    }
    if (config.isMember("dataPin")) {
        int p = config["dataPin"].asInt();
        m_dataPin = PinCapabilities::getPinByGPIO(p).ptr();
    }
    if (config.isMember("latchPin")) {
        int p = config["latchPin"].asInt();
        m_latchPin = PinCapabilities::getPinByGPIO(p).ptr();
    }

	if ((m_clockPin == nullptr) ||
		(m_dataPin == nullptr) ||
		(m_latchPin == nullptr))
	{
		LogErr(VB_CHANNELOUT, "GPIO595 Invalid Config\n");
		return 0;
	}

    m_clockPin->configPin("gpio", true);
    m_dataPin->configPin("gpio", true);
    m_latchPin->configPin("gpio", true);
    
    m_clockPin->setValue(0);
    m_dataPin->setValue(0);
    m_latchPin->setValue(1);

	return ThreadedChannelOutputBase::Init(config);
}

/*
 *
 */
int GPIO595Output::Close(void)
{
	LogDebug(VB_CHANNELOUT, "GPIO595Output::Close()\n");

	return ThreadedChannelOutputBase::Close();
}

/*
 *
 */
int GPIO595Output::RawSendData(unsigned char *channelData)
{
	LogDebug(VB_CHANNELOUT, "GPIO595Output::RawSendData(%p)\n", channelData);

	// Drop the latch low
    m_latchPin->setValue(0);
    std::this_thread::sleep_for(std::chrono::microseconds(1));

	// Output one bit per channel along with a clock tick
	int i = 0;
	for (i = m_channelCount - 1; i >= 0; i--) {
		// We only support basic On/Off.  non-zero channel value == On
        m_dataPin->setValue(channelData[i]);

		// Send a clock tick
        m_clockPin->setValue(1);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        m_clockPin->setValue(0);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
	}

	// Bring the latch high to push the bits out of the chip
    m_latchPin->setValue(1);
    std::this_thread::sleep_for(std::chrono::microseconds(1));

	return m_channelCount;
}

/*
 *
 */
void GPIO595Output::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "GPIO595Output::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    Clock Pin: %d\n", m_clockPin ? m_clockPin->kernelGpio : -1);
	LogDebug(VB_CHANNELOUT, "    Data Pin : %d\n", m_dataPin ? m_dataPin->kernelGpio : -1);
	LogDebug(VB_CHANNELOUT, "    Latch Pin: %d\n", m_latchPin ? m_latchPin->kernelGpio : -1);

	ThreadedChannelOutputBase::DumpConfig();
}

