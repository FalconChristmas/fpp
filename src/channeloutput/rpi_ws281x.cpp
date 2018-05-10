/*
 *   Raspberry Pi rpi_ws281x handler for Falcon Player (FPP)
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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "common.h"
#include "log.h"
#include "rpi_ws281x.h"
#include "Sequence.h" // for FPPD_MAX_CHANNELS
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
	m_string1GPIO(18),
	m_string2GPIO(19),
	m_pixels(0)
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::RPIWS281xOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = FPPD_MAX_CHANNELS;
}

/*
 *
 */
RPIWS281xOutput::~RPIWS281xOutput()
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::~RPIWS281xOutput()\n");

	for (int s = 0; s < m_strings.size(); s++)
		delete m_strings[s];
}


/*
 *
 */
int RPIWS281xOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::Init(JSON)\n");

	for (int i = 0; i < config["outputs"].size(); i++)
	{
		Json::Value s = config["outputs"][i];
		PixelString *newString = new PixelString();

		if (!newString->Init(s))
			return 0;

		m_pixels += newString->m_pixels;

		m_strings.push_back(newString);
	}

	ledstring.freq   = 800000; // Hard code this for now
	ledstring.dmanum = 5;

	ledstring.channel[0].gpionum = m_string1GPIO;
	ledstring.channel[0].count   = m_strings[0]->m_pixels;
	ledstring.channel[0].strip_type = WS2811_STRIP_RGB;
	ledstring.channel[0].invert  = 0;
	ledstring.channel[0].brightness  = 255;

	ledstring.channel[1].gpionum = m_string2GPIO;
	ledstring.channel[1].count   = m_strings[1]->m_pixels;
	ledstring.channel[1].strip_type = WS2811_STRIP_RGB;
	ledstring.channel[1].invert  = 0;
	ledstring.channel[1].brightness  = 255;

	SetupCtrlCHandler();

	int res = ws2811_init(&ledstring);
	if (res)
	{
		LogErr(VB_CHANNELOUT, "ws2811_init() failed with error: %d\n", res);
		return 0;
	}

	return ChannelOutputBase::Init(config);
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

void RPIWS281xOutput::PrepData(unsigned char *channelData)
{
	unsigned char *c = channelData;
	unsigned int r = 0;
	unsigned int g = 0;
	unsigned int b = 0;

	PixelString *ps = NULL;
	int inCh = 0;

	for (int s = 0; s < m_strings.size(); s++)
	{
		ps = m_strings[s];
		inCh = 0;

		for (int p = 0; p < ps->m_outputChannels; )
		{
			r = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
			g = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
			b = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];

			ledstring.channel[s].leds[p] =
				(r << 16) | (g <<  8) | (b);
		}
	}
}

/*
 *
 */
int RPIWS281xOutput::RawSendData(unsigned char *channelData)
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::RawSendData(%p)\n", channelData);

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

	for (int i = 0; i < m_strings.size(); i++)
	{
		LogDebug(VB_CHANNELOUT, "    String #%d\n", i);
		LogDebug(VB_CHANNELOUT, "      GPIO       : %d\n",
			ledstring.channel[i].gpionum);
		m_strings[i]->DumpConfig();
	}

	ChannelOutputBase::DumpConfig();
}

void RPIWS281xOutput::SetupCtrlCHandler(void)
{
	struct sigaction sa;

	sa.sa_handler = rpi_ws281x_ctrl_c_handler;

	sigaction(SIGKILL, &sa, NULL);
}

