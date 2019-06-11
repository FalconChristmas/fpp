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

#include "common.h"
#include "log.h"
#include "settings.h"

#include "GPIO595.h"

#ifdef USEWIRINGPI
#   include "wiringPi.h"
#elif defined(PLATFORM_BBB)
#   include "util/BBBUtils.h"
#   define INPUT "in"
#   define OUTPUT "out"
#   define HIGH   1
#   define LOW   0
#   define pinMode(a, b)         configBBBPin(a, "gpio", b)
#   define digitalWrite(a,b)     setBBBPinValue(a, b)
#   define delayMicroseconds(a)     0
#else
#   define pinMode(a, b)
#   define digitalWrite(a, b)
#   define delayMicroseconds(a)     0
#endif

#define GPIO595_MAX_CHANNELS  128

/////////////////////////////////////////////////////////////////////////////


/*
 *
 */
GPIO595Output::GPIO595Output(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_clockPin(-1),
	m_dataPin(-1),
	m_latchPin(-1)
{
	LogDebug(VB_CHANNELOUT, "GPIO595Output::GPIO595Output(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = GPIO595_MAX_CHANNELS;
}

/*
 *
 */
GPIO595Output::~GPIO595Output()
{
	LogDebug(VB_CHANNELOUT, "GPIO595Output::~GPIO595Output()\n");
}

/*
 *
 */
int GPIO595Output::Init(char *configStr)
{
	LogDebug(VB_CHANNELOUT, "GPIO595Output::Init('%s')\n", configStr);

	std::vector<std::string> configElems = split(configStr, ';');

	for (int i = 0; i < configElems.size(); i++)
	{
		std::vector<std::string> elem = split(configElems[i], '=');
		if (elem.size() < 2)
			continue;

		if (elem[0] == "gpio")
		{
			LogDebug(VB_CHANNELOUT, "Using GPIO %s for output\n",
				elem[1].c_str());

			std::vector<std::string> gpios = split(elem[1], '-');
			if ((elem[1].length() == 8) &&
				(gpios.size() == 3))
			{
				m_clockPin = atoi(gpios[0].c_str());
				m_dataPin  = atoi(gpios[1].c_str());
				m_latchPin = atoi(gpios[2].c_str());
			}
		}
	}

	if ((m_clockPin == -1) ||
		(m_dataPin == -1) ||
		(m_latchPin == -1))
	{
		LogErr(VB_CHANNELOUT, "Invalid Config Str: %s\n", configStr);
		return 0;
	}

	digitalWrite(m_clockPin, LOW);
	digitalWrite(m_dataPin,  LOW);
	digitalWrite(m_latchPin, HIGH);

	pinMode(m_clockPin, OUTPUT);
	pinMode(m_dataPin,  OUTPUT);
	pinMode(m_latchPin, OUTPUT);

	return ThreadedChannelOutputBase::Init(configStr);
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
	digitalWrite(m_latchPin, LOW);
	delayMicroseconds(1);

	// Output one bit per channel along with a clock tick
	int i = 0;
	for (i = m_channelCount - 1; i >= 0; i--)
	{
		// We only support basic On/Off.  non-zero channel value == On
		digitalWrite(m_dataPin, channelData[i]);

		// Send a clock tick
		digitalWrite(m_clockPin, HIGH);
		delayMicroseconds(1);
		digitalWrite(m_clockPin, LOW);
		delayMicroseconds(1);
	}

	// Bring the latch high to push the bits out of the chip
	digitalWrite(m_latchPin, HIGH);
	delayMicroseconds(1);

	return m_channelCount;
}

/*
 *
 */
void GPIO595Output::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "GPIO595Output::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Clock Pin: %d\n", m_clockPin);
	LogDebug(VB_CHANNELOUT, "    Data Pin : %d\n", m_dataPin);
	LogDebug(VB_CHANNELOUT, "    Latch Pin: %d\n", m_latchPin);

	ThreadedChannelOutputBase::DumpConfig();
}

