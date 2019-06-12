/*
 *   spixels library Channel Output for Falcon Player (FPP)
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
#include "spixels.h"
#include "Sequence.h" // for FPPD_MAX_CHANNELS
#include "settings.h"



extern "C" {
    SpixelsOutput *createOutputspixels(unsigned int startChannel,
                                       unsigned int channelCount) {
        return new SpixelsOutput(startChannel, channelCount);
    }
}


/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
SpixelsOutput::SpixelsOutput(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_spi(NULL)
{
	LogDebug(VB_CHANNELOUT, "SpixelsOutput::SpixelsOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = FPPD_MAX_CHANNELS;
}

/*
 *
 */
SpixelsOutput::~SpixelsOutput()
{
	LogDebug(VB_CHANNELOUT, "SpixelsOutput::~SpixelsOutput()\n");

	for (int s = 0; s < m_strips.size(); s++)
		delete m_strips[s];

	delete m_spi;

	for (int s = 0; s < m_strings.size(); s++)
		delete m_strings[s];
}


/*
 *
 */
int SpixelsOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "SpixelsOutput::Init(JSON)\n");

	bool haveWS2801 = false;

	for (int i = 0; i < config["outputs"].size(); i++)
	{
		Json::Value s = config["outputs"][i];

		if (s["protocol"].asString() == "ws2801")
			haveWS2801 = true;
	}

#if 0
// Can't use ws2801 DMA for now until mailbox issue is resolved.
// spixels includes its own copy of the mailbox code, but
// ends up calling the copy from jgarff's rpi-ws281x library
// since the functions have the same names.  Do we fork and
// rename or patch and submit a pull request?
// WS2801 is disabled in the UI, but code is left here for
// debugging.

	if (haveWS2801)
		m_spi = CreateDMAMultiSPI(); // WS2801 needs DMA for accurate timing
	else
#endif
		m_spi = CreateDirectMultiSPI();

	for (int i = 0; i < config["outputs"].size(); i++)
	{
		Json::Value s = config["outputs"][i];
		PixelString *newString = new PixelString();
		LEDStrip *strip = NULL;

		if (!newString->Init(s))
			return 0;

		int connector = 0;
		int pixels = newString->m_outputChannels / 3; // FIXME, need to confirm this

		if (pixels == 0)
		{
			delete newString;
			continue;
		}

		switch (s["portNumber"].asInt())
		{
			case  0: connector = MultiSPI::SPI_P1;  break;
			case  1: connector = MultiSPI::SPI_P2;  break;
			case  2: connector = MultiSPI::SPI_P3;  break;
			case  3: connector = MultiSPI::SPI_P4;  break;
			case  4: connector = MultiSPI::SPI_P5;  break;
			case  5: connector = MultiSPI::SPI_P6;  break;
			case  6: connector = MultiSPI::SPI_P7;  break;
			case  7: connector = MultiSPI::SPI_P8;  break;
			case  8: connector = MultiSPI::SPI_P9;  break;
			case  9: connector = MultiSPI::SPI_P10; break;
			case 10: connector = MultiSPI::SPI_P11; break;
			case 11: connector = MultiSPI::SPI_P12; break;
			case 12: connector = MultiSPI::SPI_P13; break;
			case 13: connector = MultiSPI::SPI_P14; break;
			case 14: connector = MultiSPI::SPI_P15; break;
			case 15: connector = MultiSPI::SPI_P16; break;
		}

		std::string protocol = s["protocol"].asString();
		if (protocol == "ws2801")
		{
			strip = CreateWS2801Strip(m_spi, connector, pixels);
		}
		else if (protocol == "apa102")
		{
			strip = CreateAPA102Strip(m_spi, connector, pixels);
		}
		else if (protocol == "lpd6803")
		{
			strip = CreateLPD6803Strip(m_spi, connector, pixels);
		}
		else if (protocol == "lpd8806")
		{
			strip = CreateLPD8806Strip(m_spi, connector, pixels);
		}
		else
		{
			LogErr(VB_CHANNELOUT, "Unknown Pixel Protocol: %s\n", s["protocol"].asString());
			return 0;
		}

		m_strings.push_back(newString);
		m_strips.push_back(strip);
	}

	LogDebug(VB_CHANNELOUT, "   Found %d strings of pixels\n", m_strings.size());

	return ThreadedChannelOutputBase::Init(config);
}

/*
 *
 */
int SpixelsOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "SpixelsOutput::Close()\n");

	return ThreadedChannelOutputBase::Close();
}

void SpixelsOutput::GetRequiredChannelRange(int &min, int & max) {
    min = FPPD_MAX_CHANNELS;
    max = 0;
    
    PixelString *ps = NULL;
    for (int s = 0; s < m_strings.size(); s++) {
        ps = m_strings[s];
        int inCh = 0;
        for (int p = 0; p < ps->m_outputChannels; p++) {
            int ch = ps->m_outputMap[inCh++];
            if (ch < (FPPD_MAX_CHANNELS - 3)) {
                min = std::min(min, ch);
                max = std::max(max, ch);
            }
        }
    }
}
void SpixelsOutput::PrepData(unsigned char *channelData)
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

			m_strips[s]->SetPixel(pix, RGBc(r,g,b));
		}
	}
}

/*
 *
 */
int SpixelsOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "SpixelsOutput::RawSendData(%p)\n", channelData);

	if (m_spi)
		m_spi->SendBuffers();

	return m_channelCount;
}

/*
 *
 */
void SpixelsOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "SpixelsOutput::DumpConfig()\n");

	for (int i = 0; i < m_strings.size(); i++)
	{
		LogDebug(VB_CHANNELOUT, "    String #%d\n", i);
		m_strings[i]->DumpConfig();
	}

	ThreadedChannelOutputBase::DumpConfig();
}

