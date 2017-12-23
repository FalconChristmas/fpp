/*
 *   Raspberry Pi rpi_ws281x handler for Falcon Player (FPP)
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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "common.h"
#include "log.h"
#include "rpi_ws281x.h"
#include "settings.h"


/////////////////////////////////////////////////////////////////////////////

// Declare ledstring here since it's needed by the CTRL-C handler.
// This could be done other/better ways, but this is simplest for now.

ws2811_t ledstring;

/*
 * CTRL-C handler to stop DMA output
 */
static void rpi_ws281x_ctrl_c_handler(int signum)
{
    ws2811_fini(&ledstring);
}


/*
 *
 */
RPIWS281xOutput::RPIWS281xOutput(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_string1GPIO(0),
	m_string1Pixels(0),
	m_string1ColorOrder("RGB"),
	m_string2GPIO(0),
	m_string2Pixels(0),
	m_string2ColorOrder("RGB")
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::RPIWS281xOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = RPIWS281X_MAX_CHANNELS;
}

/*
 *
 */
RPIWS281xOutput::~RPIWS281xOutput()
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::~RPIWS281xOutput()\n");
}

/*
 *
 */
int RPIWS281xOutput::Init(char *configStr)
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::Init('%s')\n", configStr);

	std::vector<std::string> configElems = split(configStr, ';');

	for (int i = 0; i < configElems.size(); i++)
	{
		std::vector<std::string> elem = split(configElems[i], '=');
		if (elem.size() < 2)
			continue;

		if (elem[0] == "string1GPIO")
		{
			m_string1GPIO = atoi(elem[1].c_str());
		}
		else if (elem[0] == "string1Pixels")
		{
			m_string1Pixels = atoi(elem[1].c_str());

			if (m_string1Pixels)
				m_string1GPIO = 18;
		}
		else if (elem[0] == "string1ColorOrder")
		{
			m_string1ColorOrder = elem[1].c_str();
		}
		else if (elem[0] == "string2GPIO")
		{
			m_string2GPIO = atoi(elem[1].c_str());
		}
		else if (elem[0] == "string2Pixels")
		{
			m_string2Pixels = atoi(elem[1].c_str());

			if (m_string2Pixels)
				m_string2GPIO = 19;
		}
		else if (elem[0] == "string2ColorOrder")
		{
			m_string2ColorOrder = elem[1].c_str();
		}
	}

	if (!m_string1GPIO && !m_string2GPIO)
	{
		LogErr(VB_CHANNELOUT, "Invalid Config Str: %s\n", configStr);
		return 0;
	}

	if (m_string1GPIO && !m_string1Pixels)
	{
		LogErr(VB_CHANNELOUT, "Invalid String 1 Pixel Count: %s\n",
			configStr);
		return 0;
	}

	if (m_string2GPIO && !m_string2Pixels)
	{
		LogErr(VB_CHANNELOUT, "Invalid String 2 Pixel Count: %s\n",
			 configStr);
		return 0;
	}

	ledstring.freq   = 800000; // Hard code this for now
	ledstring.dmanum = 5;

	ledstring.channel[0].gpionum = m_string1GPIO;
	ledstring.channel[0].count   = m_string1Pixels;

	if (m_string1ColorOrder == "RGB")
	{
		ledstring.channel[0].strip_type = WS2811_STRIP_RGB;
	}
	else if (m_string1ColorOrder == "RBG")
	{
		ledstring.channel[0].strip_type = WS2811_STRIP_RBG;
	}
	else if (m_string1ColorOrder == "GRB")
	{
		ledstring.channel[0].strip_type = WS2811_STRIP_GRB;
	}
	else if (m_string1ColorOrder == "GBR")
	{
		ledstring.channel[0].strip_type = WS2811_STRIP_GBR;
	}
	else if (m_string1ColorOrder == "BRG")
	{
		ledstring.channel[0].strip_type = WS2811_STRIP_BRG;
	}
	else if (m_string1ColorOrder == "BGR")
	{
		ledstring.channel[0].strip_type = WS2811_STRIP_BGR;
	}

	ledstring.channel[0].invert  = 0;
	ledstring.channel[0].brightness  = 255;

	ledstring.channel[1].gpionum = m_string2GPIO;
	ledstring.channel[1].count   = m_string2Pixels;

	if (m_string2ColorOrder == "RGB")
	{
		ledstring.channel[1].strip_type = WS2811_STRIP_RGB;
	}
	else if (m_string2ColorOrder == "RBG")
	{
		ledstring.channel[1].strip_type = WS2811_STRIP_RBG;
	}
	else if (m_string2ColorOrder == "GRB")
	{
		ledstring.channel[1].strip_type = WS2811_STRIP_GRB;
	}
	else if (m_string2ColorOrder == "GBR")
	{
		ledstring.channel[1].strip_type = WS2811_STRIP_GBR;
	}
	else if (m_string2ColorOrder == "BRG")
	{
		ledstring.channel[1].strip_type = WS2811_STRIP_BRG;
	}
	else if (m_string2ColorOrder == "BGR")
	{
		ledstring.channel[1].strip_type = WS2811_STRIP_BGR;
	}

	ledstring.channel[1].invert  = 0;
	ledstring.channel[1].brightness  = 255;

	SetupCtrlCHandler();

	int res = ws2811_init(&ledstring);
	if (res)
	{
		LogErr(VB_CHANNELOUT, "ws2811_init() failed with error: %d\n", res);
		return 0;
	}

	return ChannelOutputBase::Init(configStr);
}

/*
 *
 */
int RPIWS281xOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::Close()\n");

	ws2811_fini(&ledstring);

	return ChannelOutputBase::Close();
}

/*
 *
 */
int RPIWS281xOutput::RawSendData(unsigned char *channelData)
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::RawSendData(%p)\n", channelData);

	unsigned char *c = channelData;
	unsigned int r = 0;
	unsigned int g = 0;
	unsigned int b = 0;

	// Handle String #1
	if (m_string1GPIO)
	{
		for (int i = 0; i < m_string1Pixels; i++)
		{
			r = *(c++);
			g = *(c++);
			b = *(c++);
			ledstring.channel[0].leds[i] =
				(r << 16) | (g <<  8) | (b);
		}
	}

	// Handle String #2
	if (m_string2GPIO)
	{
		for (int i = 0; i < m_string2Pixels; i++)
		{
			r = *(c++);
			g = *(c++);
			b = *(c++);
			ledstring.channel[1].leds[i] =
				(r << 16) | (g <<  8) | (b);
		}
	}

	if (ws2811_render(&ledstring))
	{
		LogErr(VB_CHANNELOUT, "ws2811_render() failed\n");
	}

	return m_channelCount;
}

/*
 *
 */
void RPIWS281xOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::DumpConfig()\n");

	if (m_string1GPIO)
	{
		LogDebug(VB_CHANNELOUT, "    String #1\n");
		LogDebug(VB_CHANNELOUT, "      GPIO       : %d\n", m_string1GPIO);
		LogDebug(VB_CHANNELOUT, "      Pixels     : %d\n", m_string1Pixels);
		LogDebug(VB_CHANNELOUT, "      Color Order: %s\n", m_string1ColorOrder.c_str());
		LogDebug(VB_CHANNELOUT, "      Channels   : %d\n", m_string1Pixels * 3);
	}
	else
	{
		LogDebug(VB_CHANNELOUT, "    String #1 not configured.\n");
	}

	if (m_string2GPIO)
	{
		LogDebug(VB_CHANNELOUT, "    String #2\n");
		LogDebug(VB_CHANNELOUT, "      GPIO       : %d\n", m_string2GPIO);
		LogDebug(VB_CHANNELOUT, "      Pixels     : %d\n", m_string2Pixels);
		LogDebug(VB_CHANNELOUT, "      Color Order: %s\n", m_string2ColorOrder.c_str());
		LogDebug(VB_CHANNELOUT, "      Channels   : %d\n", m_string2Pixels * 3);
	}
	else
	{
		LogDebug(VB_CHANNELOUT, "    String #2 not configured.\n");
	}

	ChannelOutputBase::DumpConfig();
}

void RPIWS281xOutput::SetupCtrlCHandler(void)
{
	struct sigaction sa;

	sa.sa_handler = rpi_ws281x_ctrl_c_handler;

	sigaction(SIGKILL, &sa, NULL);
}

