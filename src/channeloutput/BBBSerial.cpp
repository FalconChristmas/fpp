/*
 *   BeagleBone Black PRU Serial DMX/Pixelnet handler for Falcon Pi Player (FPP)
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
#include "BBBSerial.h"
#include "settings.h"

#define BBBSERIAL_DDR_OFFSET 100000

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
BBBSerialOutput::BBBSerialOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_config(NULL),
	m_leds(NULL),
	m_pixelnet(0)
{
	LogDebug(VB_CHANNELOUT, "BBBSerialOutput::BBBSerialOutput(%u, %u)\n",
		startChannel, channelCount);
    m_useOutputThread = 0;
}

/*
 *
 */
BBBSerialOutput::~BBBSerialOutput()
{
	LogDebug(VB_CHANNELOUT, "BBBSerialOutput::~BBBSerialOutput()\n");
    prussdrv_exit();
}

/*
 *
 */
int BBBSerialOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "BBBSerialOutput::Init(JSON)\n");

	// Always send 8 outputs worth of data to PRU for now
	m_outputs = 8;

	m_channelCount = config["channelCount"].asInt();
	m_config = &ledscape_matrix_default;

	if (config["subType"].asString() == "Pixelnet")
		m_pixelnet = 1;
	else
		m_pixelnet = 0;

	m_startChannels.resize(config["outputs"].size());

	// Initialize the ouputs
	for (int i = 0; i < m_outputs; i++)
	{
		m_startChannels[i] = 0;
	}

	for (int i = 0; i < config["outputs"].size(); i++)
	{
		Json::Value s = config["outputs"][i];

		m_startChannels[s["outputNumber"].asInt()] = s["startChannel"].asInt() - 1;
	}

	m_config = reinterpret_cast<ledscape_config_t*>(calloc(1, sizeof(ledscape_config_t)));
	if (!m_config)
	{
		LogErr(VB_CHANNELOUT, "Unable to allocate LEDscape config\n");
		return 0;
	}

	ledscape_strip_config_t * const lsconfig = &m_config->strip_config;

	int pruNumber = 1;

	lsconfig->type         = LEDSCAPE_STRIP;
	lsconfig->leds_width   = (int)((m_pixelnet != 0 ? 4102 : 513) / 3) + 1;
	lsconfig->leds_height  = m_outputs;

	string pru_program(getBinDirectory());

	if (tail(pru_program, 4) == "/src")
		pru_program += "/pru/";
	else
		pru_program += "/../lib/";

	if (m_pixelnet)
		pru_program += "FalconPixelnet.bin";
	else
		pru_program += "FalconDMX.bin";

	if (!FileExists(pru_program.c_str()))
	{
		LogErr(VB_CHANNELOUT, "%s does not exist!\n", pru_program.c_str());
		free(m_config);
		m_config = NULL;

		return 0;
	}
    LogDebug(VB_CHANNELOUT, "Using program %s\n", pru_program.c_str());
	m_leds = ledscape_strip_init(m_config, 0, pruNumber, pru_program.c_str());

	if (!m_leds)
	{
		LogErr(VB_CHANNELOUT, "Unable to initialize LEDscape\n");
		free(m_config);
		m_config = NULL;

		return 0;
	}

	uint8_t *out = (uint8_t *)m_leds->pru->ddr + BBBSERIAL_DDR_OFFSET;
	for (int i = 0; i < m_outputs; i++)
	{
		if (m_pixelnet)
		{
			out[i + (0 * m_outputs)] = '\xAA';
			out[i + (1 * m_outputs)] = '\x55';
			out[i + (2 * m_outputs)] = '\x55';
			out[i + (3 * m_outputs)] = '\xAA';
			out[i + (4 * m_outputs)] = '\x15';
			out[i + (5 * m_outputs)] = '\x5D';
		}
		else
		{
			out[i] = '\x00';
		}
	}

	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int BBBSerialOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "BBBSerialOutput::Close()\n");

    // Send the stop command
    m_leds->ws281x->command = 0xFF;
    
    prussdrv_pru_wait_event(1); //PRU_EVTOUT_1);
    prussdrv_pru_clear_event(PRU1_ARM_INTERRUPT);
    prussdrv_pru_disable(m_leds->pru->pru_num);
    
    //ledscape_close only checks PRU0 events and then unmaps the memory that
    //may be used by the other pru
	//ledscape_close(m_leds);

	free(m_config);
	m_config = NULL;
    LogDebug(VB_CHANNELOUT, "BBBSerialOutput::Close() done\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
int BBBSerialOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "BBBSerialOutput::RawSendData(%p)\n",
		channelData);

	ledscape_strip_config_t *config = reinterpret_cast<ledscape_strip_config_t*>(m_config);

	// Bypass LEDscape draw routine and format data for PRU ourselves
	static unsigned frame = 0;
	uint8_t * const out = (uint8_t *)m_leds->pru->ddr + m_leds->frame_size * frame + BBBSERIAL_DDR_OFFSET;

	uint8_t *c = out;
	uint8_t *s = (uint8_t*)channelData;
	int chCount = m_pixelnet ? 4096 : 512;

	for (int i = 0; i < m_outputs; i++)
	{
		// Skip the headers (6 bytes per output for Pixelnet and 1 byte per output
		// for DMX) and index into the proper position in the m_outputs number of
		// bytes in each slice
		if (m_pixelnet)
			c = out + i + (m_outputs * 6);
		else
			c = out + i + (m_outputs);

		// Get the start channel for this output
		s = (uint8_t*)(channelData + m_startChannels[i]);

		// Now copy the individual channel data into each slice
		for (int ch = 0; ch < chCount; ch++)
		{
			*c = *(s++);

			c += m_outputs;
		}
	}

	// Wait for the previous draw to finish
	while (m_leds->ws281x->command);

	// Map
	m_leds->ws281x->pixels_dma = m_leds->pru->ddr_addr + m_leds->frame_size * frame + BBBSERIAL_DDR_OFFSET;
	// alternate frames every other draw
	// frame = (frame + 1) & 1;

	// Send the start command
	m_leds->ws281x->command = 1;

	return m_channelCount;
}

/*
 *
 */
void BBBSerialOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "BBBSerialOutput::DumpConfig()\n");

	ledscape_strip_config_t *config = reinterpret_cast<ledscape_strip_config_t*>(m_config);
	
	LogDebug(VB_CHANNELOUT, "    Pixelnet      : %d\n", m_pixelnet);
	LogDebug(VB_CHANNELOUT, "    Outputs       :\n" );

	for (int i = 0; i < m_outputs; i++)
	{
		LogDebug(VB_CHANNELOUT, "        #%d: %d-%d (%d Ch)\n",
			i + 1,
			m_startChannels[i] + 1,
			m_pixelnet ? m_startChannels[i] + 4096 : m_startChannels[i] + 512,
			m_pixelnet ? 4096 : 512);
	}

	ChannelOutputBase::DumpConfig();
}

