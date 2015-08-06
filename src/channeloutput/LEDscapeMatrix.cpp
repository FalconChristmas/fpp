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
	m_leds(NULL),
	m_colorOrder("RGB"),
	m_dataSize(0),
	m_data(NULL),
	m_invertedData(0)
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
int LEDscapeMatrixOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::Init(JSON)\n");

	m_config = reinterpret_cast<ledscape_config_t*>(calloc(1, sizeof(ledscape_config_t)));
	if (!m_config)
	{
		LogErr(VB_CHANNELOUT, "Unable to allocate LEDscape config\n");
		return 0;
	}

	int maxWidth = 0;
	int maxHeight = 0;

	ledscape_matrix_config_t * const lmconfig = &m_config->matrix_config;

	lmconfig->type         = LEDSCAPE_MATRIX;
	lmconfig->panel_width  = config["panelWidth"].asInt();
	lmconfig->panel_height = config["panelHeight"].asInt();

	m_invertedData = config["invertedData"].asInt();
	m_colorOrder = config["colorOrder"].asString();

	if (!lmconfig->panel_width)
		lmconfig->panel_width = 32;

	if (!lmconfig->panel_height)
		lmconfig->panel_height = 16;

	lmconfig->leds_width   = lmconfig->panel_width * 8;
	lmconfig->leds_height  = lmconfig->panel_height * 8;

	for (int i = 0; i < config["panels"].size(); i++)
	{
		char orientation = 'N';
		int  pWidth  = lmconfig->panel_width;
		int  pHeight = lmconfig->panel_height;

		Json::Value p = config["panels"][i];

		int  output  = p["outputNumber"].asInt();
		int  chain   = 7 - p["panelNumber"].asInt(); // 7 is first in chain, 0 is last
		int  xOffset = p["xOffset"].asInt();
		int  yOffset = p["yOffset"].asInt();

		ledscape_matrix_panel_t * const pconfig =
			&lmconfig->panels[output][chain];

		pconfig->x   = xOffset;
		pconfig->y   = yOffset;
		pconfig->rot = 0; // Default, normal rotation

		const char *o = p["orientation"].asString().c_str();
		if (o && *o)
			orientation = o[0];

		switch (orientation)
		{
			case 'N':
				pconfig->rot = 0;
				break;
			case 'L':
				pconfig->rot = 1;
				pWidth = lmconfig->panel_height;
				pHeight = lmconfig->panel_width;
				break;
			case 'R':
				pconfig->rot = 2;
				pWidth = lmconfig->panel_height;
				pHeight = lmconfig->panel_width;
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

	lmconfig->width = maxWidth;
	lmconfig->height = maxHeight;

	m_dataSize = lmconfig->width * lmconfig->height * 4;
	m_data = (uint8_t *)malloc(m_dataSize);

	if (!m_data)
	{
		LogErr(VB_CHANNELOUT, "Unable to initialize data buffer\n");

		return 0;
	}

	std::string pru_program(getBinDirectory());

	if (tail(pru_program, 4) == "/src")
		pru_program += "/pru/";
	else
		pru_program += "/../lib/";

	if (lmconfig->panel_height == 32)
		pru_program += "FalconMatrix_32x32.bin";
	else
		pru_program += "FalconMatrix.bin";

	m_leds = ledscape_matrix_init(m_config, 0, 0, pru_program.c_str());

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
int LEDscapeMatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "LEDscapeMatrixOutput::Close()\n");

	ledscape_close(m_leds);

	free(m_data);
	m_data = NULL;

	free(m_config);
	m_config = NULL;

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
	unsigned char *g = r + 1;
	unsigned char *b = r + 2;

	for (int y = 0; y < config->height; y++)
	{
		if (m_invertedData)
		{
			r = channelData + ((config->height - 1 - y) * config->width * 3);
		}
		else
		{
			r = channelData + (y * config->width * 3);
		}

		g = r + 1;
		b = r + 2;

		for (int x = 0; x < config->width; x++)
		{
			if (m_colorOrder == "RGB")
			{
				*(c++) = *b;
				*(c++) = *g;
				*(c++) = *r;
			}
			else if (m_colorOrder == "RBG")
			{
				*(c++) = *g;
				*(c++) = *b;
				*(c++) = *r;
			}
			else if (m_colorOrder == "GRB")
			{
				*(c++) = *b;
				*(c++) = *r;
				*(c++) = *g;
			}
			else if (m_colorOrder == "GBR")
			{
				*(c++) = *r;
				*(c++) = *b;
				*(c++) = *g;
			}
			else if (m_colorOrder == "BRG")
			{
				*(c++) = *g;
				*(c++) = *r;
				*(c++) = *b;
			}
			else if (m_colorOrder == "BGR")
			{
				*(c++) = *r;
				*(c++) = *g;
				*(c++) = *b;
			}
			else
			{
				*(c++) = *b;
				*(c++) = *g;
				*(c++) = *r;
			}

			c++;
			r += 3;
			g += 3;
			b += 3;
		}
	}

	//long long drawTime = 0;
	//long long waitTime = 0;
	//long long waitStartTime = 0;
	//long long startTime = GetTime();

	ledscape_draw(m_leds, m_data);
	//drawTime = GetTime();
	usleep(5000);
	//waitStartTime = GetTime();
	//const uint32_t response = ledscape_wait(m_leds);
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

