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


extern "C" {
    RPIWS281xOutput *createOutputRPIWS281X(unsigned int startChannel,
                                           unsigned int channelCount) {
        return new RPIWS281xOutput(startChannel, channelCount);
    }
}

/////////////////////////////////////////////////////////////////////////////

// Declare ledstring here since it's needed by the CTRL-C handler.
// This could be done other/better ways, but this is simplest for now.

int ledstringCount = 0;
ws2811_t ledstring[3];

/*
 * CTRL-C handler to stop DMA output
 */
static void rpi_ws281x_ctrl_c_handler(int signum)
{
	for (int i = 0; i < ledstringCount; i++)
		ws2811_fini(&ledstring[i]);
}


/*
 *
 */
RPIWS281xOutput::RPIWS281xOutput(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_ledstringNumber(0),
	m_string1GPIO(18),
	m_string2GPIO(19),
	m_pixels(0)
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::RPIWS281xOutput(%u, %u)\n",
		startChannel, channelCount);
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

		if (s.isMember("gpio"))
		{
			if (i == 0)
				m_string1GPIO = s["gpio"].asInt();
			else if (i == 1)
				m_string2GPIO = s["gpio"].asInt();
		}

		if (!newString->Init(s))
			return 0;

		m_pixels += newString->m_outputChannels / 3;

		m_strings.push_back(newString);
	}

	m_ledstringNumber = ledstringCount++;

	LogDebug(VB_CHANNELOUT, "   Found %d strings of pixels\n", m_strings.size());
	ledstring[m_ledstringNumber].freq   = 800000; // Hard code this for now
	ledstring[m_ledstringNumber].dmanum = 10;

	ledstring[m_ledstringNumber].channel[0].gpionum = m_string1GPIO;
	ledstring[m_ledstringNumber].channel[0].count   = m_strings[0]->m_outputChannels / 3;
	ledstring[m_ledstringNumber].channel[0].strip_type = WS2811_STRIP_RGB;
	ledstring[m_ledstringNumber].channel[0].invert  = 0;
	ledstring[m_ledstringNumber].channel[0].brightness  = 255;

	ledstring[m_ledstringNumber].channel[1].gpionum = m_string2GPIO;
        if (m_strings.size() > 1) {
	    ledstring[m_ledstringNumber].channel[1].count   = m_strings[1]->m_outputChannels / 3;
	} else {
	    ledstring[m_ledstringNumber].channel[1].count   = 0;
	}
	ledstring[m_ledstringNumber].channel[1].strip_type = WS2811_STRIP_RGB;
	ledstring[m_ledstringNumber].channel[1].invert  = 0;
	ledstring[m_ledstringNumber].channel[1].brightness  = 255;

	if (m_ledstringNumber)
		SetupCtrlCHandler();

	int res = ws2811_init(&ledstring[m_ledstringNumber]);
	if (res)
	{
		LogErr(VB_CHANNELOUT, "ws2811_init() failed with error: %d\n", res);
		return 0;
	}

	return ThreadedChannelOutputBase::Init(config);
}

/*
 *
 */
int RPIWS281xOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "RPIWS281xOutput::Close()\n");

	ws2811_fini(&ledstring[m_ledstringNumber]);

	return ThreadedChannelOutputBase::Close();
}

void RPIWS281xOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    PixelString *ps = NULL;
    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
        int inCh = 0;
        int min = FPPD_MAX_CHANNELS;
        int max = 0;
        for (int p = 0; p < ps->m_outputChannels; p++) {
            int ch = ps->m_outputMap[inCh++];
            if (ch < FPPD_MAX_CHANNELS) {
                min = std::min(min, ch);
                max = std::max(max, ch);
            }
        }
        if (min < max) {
            addRange(min, max);
        }
    }
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

		for (int p = 0, pix = 0; p < ps->m_outputChannels; pix++)
		{
			r = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
			g = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];
			b = ps->m_brightnessMaps[p++][channelData[ps->m_outputMap[inCh++]]];

			ledstring[m_ledstringNumber].channel[s].leds[pix] =
				(r << 16) | (g <<  8) | (b);
		}
	}
}

/*
 *
 */
int RPIWS281xOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "RPIWS281xOutput::RawSendData(%p)\n", channelData);

	if (ws2811_render(&ledstring[m_ledstringNumber]))
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
			ledstring[m_ledstringNumber].channel[i].gpionum);
		m_strings[i]->DumpConfig();
	}

	ThreadedChannelOutputBase::DumpConfig();
}

void RPIWS281xOutput::SetupCtrlCHandler(void)
{
	struct sigaction sa;

	sa.sa_handler = rpi_ws281x_ctrl_c_handler;

	sigaction(SIGKILL, &sa, NULL);
}

