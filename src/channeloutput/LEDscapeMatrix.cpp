/*
 *   LEDscape Matrix handler for Falcon Pi Player (FPP)
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
#include <strings.h>
#include <unistd.h>

#include "pru.h"

#include "common.h"
#include "log.h"
#include "LEDscapeMatrix.h"
#include "settings.h"

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
LEDscapeMatrixOutput::LEDscapeMatrixOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_config(NULL),
	m_leds(NULL)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::LEDscapeMatrixOutput(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
LEDscapeMatrixOutput::~LEDscapeMatrixOutput()
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::~LEDscapeMatrixOutput()\n");
}

/*
 *
 */
int LEDscapeMatrixOutput::Init(char *configStr)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::Init('%s')\n", configStr);
	string panelCfg;

	m_config = &ledscape_matrix_default;

	std::vector<std::string> configElems = split(configStr, ';');

	for (int i = 0; i < configElems.size(); i++)
	{
		std::vector<std::string> elem = split(configElems[i], '=');
		if (elem.size() < 2)
			continue;

		if (elem[0] == "panels")
		{
			panelCfg = elem[1];
		}
	}

	// Sample configs:
	// 4x1: "panels=0:5:N:32:0|0:4:N:0:0|0:6:N:0:16|0:7:N:32:16"
	// 2x2: "panels=0:5:U:0:0|0:4:U:32:0|0:6:N:0:16|0:7:N:32:16"
	// 2x1: "panels=0:6:N:0:0|0:7:N:32:0"
	m_config = reinterpret_cast<ledscape_config_t*>(calloc(1, sizeof(ledscape_config_t)));
	if (!m_config)
	{
		LogErr(VB_CHANNELOUT, "Unable to allocate LEDscape config\n");
		return 0;
	}

	int maxWidth = 0;
	int maxHeight = 0;

	ledscape_matrix_config_t * const config = &m_config->matrix_config;

	config->type         = LEDSCAPE_MATRIX;
	config->panel_width  = 32;
	config->panel_height = 16;
	config->leds_width   = 256;
	config->leds_height  = 128;

	std::vector<std::string> panels = split(panelCfg, '|');
	for (int p = 0; p < panels.size(); p++)
	{
		int  output  = 0;
		int  chain   = 0;
		int  xOffset = 0;
		int  yOffset = 0;
		char orientation = 'N';
		int  pWidth  = 32;
		int  pHeight = 16;

		std::vector<std::string> elems = split(panels[p], ':');

		if (elems.size() == 5)
		{
			output  = atoi(elems[0].c_str());
			chain   = atoi(elems[1].c_str());
			xOffset = atoi(elems[3].c_str());
			yOffset = atoi(elems[4].c_str());

			if (elems[2].size())
				orientation = elems[2].c_str()[0];

			ledscape_matrix_panel_t * const pconfig =
				&config->panels[output][chain];

			pconfig->x   = xOffset;
			pconfig->y   = yOffset;
			pconfig->rot = 0; // Default, normal rotation

			switch (orientation)
			{
				case 'N':
					pconfig->rot = 0;
					break;
				case 'L':
					pconfig->rot = 1;
					pWidth = 16;
					pHeight = 32;
					break;
				case 'R':
					pconfig->rot = 2;
					pWidth = 16;
					pHeight = 32;
					break;
				case 'U':
					pconfig->rot = 3;
					break;
			}

			if ((xOffset + pWidth) > maxWidth)
				maxWidth = xOffset + pWidth;

			if ((yOffset + pHeight) > maxHeight)
				maxHeight = yOffset + pHeight;
		}

	}

	config->width = maxWidth;
	config->height = maxHeight;

	m_dataSize = config->width * config->height * 4;
	m_data = (uint8_t *)malloc(m_dataSize);

	if (!m_data)
	{
		LogErr(VB_CHANNELOUT, "Unable to initialize data buffer\n");

		return 0;
	}

	string pru_program(getBinDirectory());
	pru_program += "/../lib/LEDscapeMatrix.bin";
	m_leds = ledscape_matrix_init(m_config, 0, 0, pru_program.c_str());

	if (!m_leds)
	{
		LogErr(VB_CHANNELOUT, "Unable to initialize LEDscape\n");
		return 0;
	}

	return ChannelOutputBase::Init(configStr);
}

/*
 *
 */
int LEDscapeMatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::Close()\n");

	ledscape_close(m_leds);

	free(m_data);
	m_data = NULL;

	// FIXME, do we need to cleanup m_config also?

	return ChannelOutputBase::Close();
}

/*
 *
 */
int LEDscapeMatrixOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "LEDscapeMatrixOutput::RawSendData(%p)\n",
		channelData);

	bzero(m_data, m_dataSize);

	ledscape_matrix_config_t *config = reinterpret_cast<ledscape_matrix_config_t*>(m_config);

	unsigned char *c = m_data;
	unsigned char *r = channelData;
	unsigned char *g = channelData + 1;
	unsigned char *b = channelData + 2;

	for (int y = 0; y < config->height; y++)
	{
		for (int x = 0; x < config->width; x++)
		{
			*(c++) = *b;
			*(c++) = *g;
			*(c++) = *r;

			c++;
			r += 3;
			g += 3;
			b += 3;
		}
	}

long long drawTime = 0;
long long waitTime = 0;
long long waitStartTime = 0;
long long startTime = GetTime();

	ledscape_draw(m_leds, m_data);
drawTime = GetTime();
	usleep(5000);
//waitStartTime = GetTime();
//	const uint32_t response = ledscape_wait(m_leds);
//waitTime = GetTime();

//LogDebug(VB_CHANNELOUT, "V: %02x, Draw Time: %lldus, Wait Time: %lldus (+ 5ms)\n",
//	channelData[0], drawTime - startTime, waitTime - waitStartTime);

	return m_channelCount;
}

/*
 *
 */
void LEDscapeMatrixOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::DumpConfig()\n");

	ledscape_matrix_config_t *config = reinterpret_cast<ledscape_matrix_config_t*>(m_config);
	
	LogDebug(VB_CHANNELOUT, "    width   : %d\n", config->width);
	LogDebug(VB_CHANNELOUT, "    height  : %d\n", config->height);

//	LogDebug(VB_CHANNELOUT, "    panels : %d (%dx%d)\n",
//		m_panels, m_panelsWide, m_panelsHigh);
//	LogDebug(VB_CHANNELOUT, "    rows   : %d\n", m_rows);
}

