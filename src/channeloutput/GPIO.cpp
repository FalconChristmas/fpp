/*
 *   GPIO Pin Channel Output driver for Falcon Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

#include <stdlib.h>

#include "common.h"
#include "GPIO.h"
#include "log.h"

#ifdef USEWIRINGPI
#   include "wiringPi.h"
#   include <softPwm.h>
#else
#   define pinMode(a, b)
#   define digitalWrite(a, b)
#   define softPwmCreate(a, b, c)
#   define softPwmWrite(a, b)
#endif

/*
 *
 */
GPIOOutput::GPIOOutput(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_GPIOPin(-1),
	m_invertOutput(0),
	m_softPWM(0)
{
	LogDebug(VB_CHANNELOUT, "GPIOOutput::GPIOOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = 1;
}

/*
 *
 */
GPIOOutput::~GPIOOutput()
{
	LogDebug(VB_CHANNELOUT, "GPIOOutput::~GPIOOutput()\n");
}

/*
 *
 */
int GPIOOutput::Init(char *configStr)
{
	LogDebug(VB_CHANNELOUT, "GPIOOutput::Init('%s')\n", configStr);

	// Disable threading because it is fast to digitalWrite() to one pin
	m_useOutputThread = 0;

	std::vector<std::string> configElems = split(configStr, ';');

	for (int i = 0; i < configElems.size(); i++)
	{
		std::vector<std::string> elem = split(configElems[i], '=');
		if (elem.size() < 2)
			continue;

		if (elem[0] == "gpio")
			m_GPIOPin = atoi(elem[1].c_str());
		else if (elem[0] == "invert")
			m_invertOutput = atoi(elem[1].c_str());
		else if (elem[0] == "softPWM")
			m_softPWM = atoi(elem[1].c_str());
	}

	if (m_GPIOPin < 0)
	{
		LogErr(VB_CHANNELOUT, "GPIO Pin not configured\n");
		return 0;
	}

	if (m_softPWM)
	{
		softPwmCreate(m_GPIOPin, 0, 100);

		if (m_invertOutput)
			softPwmWrite(m_GPIOPin, 100);
		else
			softPwmWrite(m_GPIOPin, 0);
	}
	else
	{
		pinMode(m_GPIOPin, OUTPUT);

		if (m_invertOutput)
			digitalWrite(m_GPIOPin, HIGH);
		else
			digitalWrite(m_GPIOPin, LOW);
	}

	return ChannelOutputBase::Init(configStr);
}

/*
 *
 */
int GPIOOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "GPIOOutput::Close()\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
int GPIOOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "GPIOOutput::RawSendData(%p)\n", channelData);

	if (m_softPWM)
	{
		if (m_invertOutput)
			softPwmWrite(m_GPIOPin, (int)((255-channelData[0]) * 100.0 / 255));
		else
			softPwmWrite(m_GPIOPin, (int)(channelData[0] * 100.0 / 255));
	}
	else
	{
		if (m_invertOutput)
			digitalWrite(m_GPIOPin, !channelData[0]);
		else
			digitalWrite(m_GPIOPin, channelData[0]);
	}

	return m_channelCount;
}

/*
 *
 */
void GPIOOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "GPIOOutput::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    GPIO Pin       : %d\n", m_GPIOPin);
	LogDebug(VB_CHANNELOUT, "    Inverted Output: %s\n", m_invertOutput ? "Yes" : "No");
	LogDebug(VB_CHANNELOUT, "    Soft PWM       : %s\n", m_softPWM ? "Yes" : "No");

	ChannelOutputBase::DumpConfig();
}

