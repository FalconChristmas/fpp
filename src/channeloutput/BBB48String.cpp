/*
 *   BeagleBone Black PRU 48-string handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <pruss_intc_mapping.h>
extern "C" {
    extern int prussdrv_pru_clear_event(unsigned int eventnum);
    extern int prussdrv_pru_wait_event(unsigned int pru_evtout_num);
    extern int prussdrv_pru_disable(unsigned int prunum);
    extern int prussdrv_exit();
}

// LEDscape includes
#include "pru.h"

// FPP includes
#include "common.h"
#include "log.h"
#include "BBB48String.h"
#include "settings.h"

/////////////////////////////////////////////////////////////////////////////
// Map the 48 ports to positions in LEDscape code
// - Array index is output port number configured in UI
// - Value is position to use in LEDscape code

static const int PinMapLEDscape[] = {
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47
};

static const int PinMapRGBCape48C[] = {
	14, 15,  1,  2,  5,  6,  9, 10, 32, 33,
	34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
	44, 45, 46, 47, 11,  8,  7,  4,  3,  0,
	16, 20, 19, 17, 22, 23, 12, 25, 13, 30,
	31, 28, 29, 26, 27, 24, 21, 18
};

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
BBB48StringOutput::BBB48StringOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_config(NULL),
	m_leds(NULL),
	m_maxStringLen(0)
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::BBB48StringOutput(%u, %u)\n",
		startChannel, channelCount);
    m_useOutputThread = 0;
}

/*
 *
 */
BBB48StringOutput::~BBB48StringOutput()
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::~BBB48StringOutput()\n");
	m_strings.clear();
    prussdrv_exit();
}

/*
 *
 */
int BBB48StringOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::Init(JSON)\n");

	m_subType = config["subType"].asString();
	m_channelCount = config["channelCount"].asInt();
	m_config = &ledscape_matrix_default;

	for (int i = 0; i < config["outputs"].size(); i++)
	{
		Json::Value s = config["outputs"][i];
		PixelString *newString = new PixelString;

		if (!newString->Init(s["portNumber"].asInt(),
			m_startChannel,
			s["startChannel"].asInt() - 1,
			s["pixelCount"].asInt(),
			s["colorOrder"].asString(),
			s["nullNodes"].asInt(),
			s["hybridMode"].asInt(),
			s["reverse"].asInt(),
			s["grouping"].asInt(),
			s["zigZag"].asInt()))
			return 0;

		if ((newString->m_pixelCount + newString->m_nullNodes) > m_maxStringLen)
			m_maxStringLen = newString->m_pixelCount + newString->m_nullNodes;

		m_strings.push_back(newString);
	}

	if (!MapPins())
	{
		LogErr(VB_CHANNELOUT, "Unable to map pins\n");
		return 0;
	}

	m_config = reinterpret_cast<ledscape_config_t*>(calloc(1, sizeof(ledscape_config_t)));
	if (!m_config)
	{
		LogErr(VB_CHANNELOUT, "Unable to allocate LEDscape config\n");
		return 0;
	}

	ledscape_strip_config_t * const lsconfig = &m_config->strip_config;

	int pruNumber = 0;

	lsconfig->type         = LEDSCAPE_STRIP;
	lsconfig->leds_width   = m_maxStringLen;

	//lsconfig->leds_height  = m_strings.size();
	// LEDscape always drives 48 ports, so use this instead of # of strings
	lsconfig->leds_height = 48;

	int LEDs = lsconfig->leds_width * lsconfig->leds_height;

	std::string pru_program(getBinDirectory());

	if (tail(pru_program, 4) == "/src")
		pru_program += "/pru/";
	else
		pru_program += "/../lib/";

	if ((m_subType == "F4-B") ||
		(m_subType == "F16-B"))
	{
		pru_program += "FalconWS281x.bin";
	}
	else if ((m_subType == "F4-B-WS") ||
			 (m_subType == "F16-B-WS"))
	{
		pru_program += "FalconWS281x_WS.bin";
	}
	else
	{
		pru_program += m_subType + ".bin";
	}

	m_leds = ledscape_strip_init(m_config, 0, pruNumber, pru_program.c_str());

	if (!m_leds)
	{
		LogErr(VB_CHANNELOUT, "Unable to initialize LEDscape\n");

		return 0;
	}

	return ChannelOutputBase::Init(config);
}


/*
 *
 */
int BBB48StringOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::Close()\n");

    // Send the stop command
    m_leds->ws281x->command = 0xFF;
    
    prussdrv_pru_wait_event(0); //PRU_EVTOUT_0);
    prussdrv_pru_clear_event(PRU0_ARM_INTERRUPT);
    prussdrv_pru_disable(m_leds->pru->pru_num);
    
    //ledscape_close only checks PRU0 events and then unmaps the memory that
    //may be used by the other pru
    //ledscape_close(m_leds);

	free(m_config);
	m_config = NULL;

	return ChannelOutputBase::Close();
}

/*
 *
 */
int BBB48StringOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "BBB48StringOutput::RawSendData(%p)\n",
		channelData);

	ledscape_strip_config_t *config = reinterpret_cast<ledscape_strip_config_t*>(m_config);

	// Bypass LEDscape draw routine and format data for PRU ourselves
	static unsigned frame = 0;
	uint8_t * const out = (uint8_t *)m_leds->pru->ddr + m_leds->frame_size * frame;

	PixelString *ps = NULL;
	uint8_t *c = NULL;
	int inCh;

	for (int s = 0; s < m_strings.size(); s++)
	{
		ps = m_strings[s];
		c = out + (ps->m_nullNodes * 48 * 3) + ps->m_portNumber;

		if ((ps->m_hybridMode) &&
			((channelData[ps->m_outputMap[0]]) ||
			 (channelData[ps->m_outputMap[1]]) ||
			 (channelData[ps->m_outputMap[2]])))
		{
			for (int p = 0; p < ps->m_pixelCount; p++)
			{
				*c = channelData[ps->m_outputMap[0]];
				c += 48;

				*c = channelData[ps->m_outputMap[1]];
				c += 48;

				*c = channelData[ps->m_outputMap[2]];
				c += 48;
			}
		}
		else
		{
			if (ps->m_hybridMode)
				inCh = 3;
			else
				inCh = 0;

			for (int p = 0; p < ps->m_pixelCount; p++)
			{
				*c = channelData[ps->m_outputMap[inCh++]];
				c += 48;

				*c = channelData[ps->m_outputMap[inCh++]];
				c += 48;

				*c = channelData[ps->m_outputMap[inCh++]];
				c += 48;
			}
		}
		
	}

	// Wait for the previous draw to finish
	while (m_leds->ws281x->command);

	// Map
	m_leds->ws281x->pixels_dma = m_leds->pru->ddr_addr + m_leds->frame_size * frame;
	// alternate frames every other draw
	// frame = (frame + 1) & 1;

	// Send the start command
	m_leds->ws281x->command = 1;

	return m_channelCount;
}

/*
 *
 */
void BBB48StringOutput::ApplyPinMap(const int *map)
{
	int origPortNumber = 0;

	for (int i = 0; i < m_strings.size(); i++)
	{
		origPortNumber = m_strings[i]->m_portNumber;
		m_strings[i]->m_portNumber = map[origPortNumber];
	}
}

/*
 *
 */
int BBB48StringOutput::MapPins(void)
{
	if (m_subType == "RGBCape48C")
	{
		ApplyPinMap(PinMapRGBCape48C);
	}

	return 1;
}

/*
 *
 */
void BBB48StringOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "BBB48StringOutput::DumpConfig()\n");

	ledscape_strip_config_t *config = reinterpret_cast<ledscape_strip_config_t*>(m_config);
	
	LogDebug(VB_CHANNELOUT, "    strings       : %d\n", m_strings.size());
	LogDebug(VB_CHANNELOUT, "    longest string: %d pixels\n", m_maxStringLen);

	for (int i = 0; i < m_strings.size(); i++)
	{
		LogDebug(VB_CHANNELOUT, "    string #%02d\n", i);
		m_strings[i]->DumpConfig();
	}

	ChannelOutputBase::DumpConfig();
}

