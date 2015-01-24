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

// for sleep() for testing
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "RGBMatrix.h"
#include "settings.h"

#define RGBMatrix_PANEL_WIDTH   32
#define RGBMatrix_PANEL_HEIGHT  16
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
	m_panelWidth(RGBMatrix_PANEL_WIDTH),
	m_panelHeight(RGBMatrix_PANEL_HEIGHT),
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

	string panelCfg;

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
		else if (elem[0] == "panels")
		{
			panelCfg = elem[1];
		}
	}

	m_panelMatrix =
		new PanelMatrix(RGBMatrix_PANEL_WIDTH, RGBMatrix_PANEL_HEIGHT);

	if (!m_panelMatrix)
	{
		LogErr(VB_CHANNELOUT, "Unable to create PanelMatrix\n");

		return 0;
	}

	// Sample configs:
	// 4x1: "0,1,N,32,0;0,0,N,0,0;0,2,N,0,16;0,3,N,32,16"
	// 2x2: "0,1,U,0,0;0,0,U,32,0;0,2,N,0,16;0,3,N,32,16"
	if (!m_panelMatrix->ConfigurePanels(panelCfg))
	{
		return 0;
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

	m_rows = RGBMatrix_PANEL_HEIGHT;

	m_width  = m_panelMatrix->Width();
	m_height = m_panelMatrix->Height();

	m_channelCount = m_width * m_height * 3;

	m_canvas = new RGBMatrix(m_gpio, m_rows, m_panels);
	if (!m_canvas)
	{
		LogErr(VB_CHANNELOUT, "Unable to create Canvas instance\n");

		delete m_gpio;
		m_gpio = NULL;

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

	unsigned char *r = NULL;
	unsigned char *g = NULL;
	unsigned char *b = NULL;

	int output = 0; // Only one output for Pi RGBMatrix support

//LogDebug(VB_CHANNELOUT, "Printing panels for output: %d\n", output);
	for (int i = 0; i < m_panelMatrix->m_outputPanels[output].size(); i++)
	{
		int panel = m_panelMatrix->m_outputPanels[output][i];

		int chain = m_panelMatrix->m_panels[panel].chain;
//LogDebug(VB_CHANNELOUT, "Panel: %d, Chain: %d\n", panel, chain);
		for (int y = 0; y < m_panelHeight; y++)
		{
			int px = chain * m_panelWidth;
			for (int x = 0; x < m_panelWidth; x++)
			{
				r = channelData + m_panelMatrix->m_panels[panel].pixelMap[(y * m_panelWidth + x) * 3];
				g = r + 1;
				b = r + 2;

//LogDebug(VB_CHANNELOUT, "p: %d, c: %d, x: %d, px: %d, y: %d, po: %d, ro: %d, r: %d, g: %d, b: %d\n", panel, chain, x, px, y, (y * m_panelWidth + x) * 3, m_panelMatrix->m_panels[panel].pixelMap[(y * m_panelWidth + x) * 3], *r, *g, *b);
				m_canvas->SetPixel(px, y, *r, *g, *b);

				px++;
			}
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
}

