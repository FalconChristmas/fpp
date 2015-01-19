/*
 *   librgbmatrix handler for Falcon Pi Player (FPP)
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

#include "common.h"
#include "log.h"
#include "RGBMatrix.h"
#include "settings.h"

#define RGBMatrix_MAX_PIXELS    512 * 4
#define RGBMatrix_MAX_CHANNELS  RGBMatrix_MAX_PIXELS * 3

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
RGBMatrixOutput::RGBMatrixOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_gpio(NULL),
	m_canvas(NULL),
	m_panels(0),
	m_panelsWide(0),
	m_panelsHigh(0),
	m_width(0),
	m_height(0),
	m_rows(0)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::RGBMatrixOutput(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
RGBMatrixOutput::~RGBMatrixOutput()
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::~RGBMatrixOutput()\n");
}

/*
 *
 */
int RGBMatrixOutput::Init(char *configStr)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::Init('%s')\n", configStr);

	std::vector<std::string> configElems = split(configStr, ';');

	for (int i = 0; i < configElems.size(); i++)
	{
		std::vector<std::string> elem = split(configElems[i], '=');
		if (elem.size() < 2)
			continue;

		if (elem[0] == "layout")
		{
			m_layout = elem[1];

			std::vector<std::string> dimensions
				= split(m_layout, 'x');
			m_panelsWide = atoi(dimensions[0].c_str());
			m_panelsHigh = atoi(dimensions[1].c_str());
			m_panels = m_panelsWide * m_panelsHigh;
		}
	}

	m_gpio = new GPIO();
	if (!m_gpio)
	{
		LogErr(VB_CHANNELOUT, "Unable to create GPIO instance\n");

		return 0;
	}

	if (!m_gpio->Init())
	{
		LogErr(VB_CHANNELOUT, "GPIO->Init() failed\n");

		delete m_gpio;
		return 0;
	}

	m_rows = 16; // Rows per panel

	m_width = m_panelsWide * 32;
	m_height = m_panelsHigh * 16;
	m_channelCount = m_width * m_height * 3;

	m_canvas = new RGBMatrix(m_gpio, m_rows, m_panels);
	if (!m_canvas)
	{
		LogErr(VB_CHANNELOUT, "Unable to create Canvas instance\n");
		return 0;
	}

	return ChannelOutputBase::Init(configStr);
}

/*
 *
 */
int RGBMatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::Close()\n");

	m_canvas->Fill(0,0,0);

	delete m_canvas;
	m_canvas = NULL;

	delete m_gpio;
	m_gpio = NULL;

	return ChannelOutputBase::Close();
}

/*
 *
 */
int RGBMatrixOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "RGBMatrixOutput::RawSendData(%p)\n",
		channelData);

	unsigned char *c = (unsigned char *)channelData;
	for (int y = 0; y < m_height; y++)
	{
		for (int x = 0; x < m_width; x++)
		{
			m_canvas->SetPixel(x, y, *(c++), *(c++), *(c++));
		}
	}

	return m_channelCount;
}

/*
 *
 */
void RGBMatrixOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "RGBMatrixOutput::DumpConfig()\n");
	LogDebug(VB_CHANNELOUT, "    panels : %d (%dx%d)\n",
		m_panels, m_panelsWide, m_panelsHigh);
	LogDebug(VB_CHANNELOUT, "    width  : %d\n", m_width);
	LogDebug(VB_CHANNELOUT, "    height : %d\n", m_height);
	LogDebug(VB_CHANNELOUT, "    rows   : %d\n", m_rows);
}

