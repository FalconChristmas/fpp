/*
 *   Panel Matrix handler for Falcon Player (FPP)
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

#include "fpp-pch.h"
#include "PanelMatrix.h"

PanelMatrix::PanelMatrix(int panelWidth, int panelHeight, int invertedData)
  : m_width(0),
	m_height(0),
	m_outputCount(0),
	m_pixelCount(0),
	m_panelCount(0),
	m_panelWidth(panelWidth),
	m_panelHeight(panelHeight),
	m_invertedData(invertedData)
{
}

PanelMatrix::~PanelMatrix()
{
}

int PanelMatrix::ConfigurePanels(std::string layout)
{
	int result = 1;

	LogDebug(VB_CHANNELOUT, "Parsing Config: '%s'\n", layout.c_str());

	std::vector<std::string> panels = split(layout, '|');

	for (int i = 0; i < panels.size(); i++)
	{
		result &= AddPanel(panels[i]);
	}

	if (result)
		CalculateMaps();

	return result;
}

int PanelMatrix::AddPanel(std::string config)
{
	int  output  = 0;
	int  chain   = 0;
	int  xOffset = 0;
	int  yOffset = 0;
	char orientation = 'N';

	std::vector<std::string> elems = split(config, ':');

	if (elems.size() == 5)
	{
		output  = atoi(elems[0].c_str());
		chain   = atoi(elems[1].c_str());
		xOffset = atoi(elems[3].c_str());
		yOffset = atoi(elems[4].c_str());

		if (elems[2].size())
			orientation = elems[2].c_str()[0];
	}
	else
	{
		LogErr(VB_CHANNELOUT, "Invalid Panel Config: '%s'\n", config.c_str());
		return 0;
	}
	
	return AddPanel(output, chain, orientation, xOffset, yOffset);
}

int PanelMatrix::AddPanel(int output, int chain, char orientation,
	 int xOffset, int yOffset, FPPColorOrder colorOrder)
{
	int pWidth  = m_panelWidth;
	int pHeight = m_panelHeight;

	// Orientation, Left, Right, Normal, Upside down
	if ((orientation == 'L') || (orientation == 'R'))
	{
		pWidth = m_panelHeight;
		pHeight = m_panelWidth;
	}
	else if ((orientation != 'N') && (orientation != 'U'))
	{
		LogErr(VB_CHANNELOUT, "Invalid orientation '%c'\n",
			orientation);
		return 0;
	}

	if ((xOffset + pWidth) > m_width)
	{
		m_width = xOffset + pWidth;
		m_pixelCount = m_width * m_height;
	}

	if ((yOffset + pHeight) > m_height)
	{
		m_height = yOffset + pHeight;
		m_pixelCount = m_width * m_height;
	}

	if (output > m_outputCount)
		m_outputCount = output;

	if ((m_panelCount < MAX_MATRIX_PANELS) &&
		(m_outputPanels[output].size() < MAX_PANELS_PER_OUTPUT))
	{
		m_panels[m_panelCount].output      = output;
		m_panels[m_panelCount].chain       = chain;
		m_panels[m_panelCount].width       = pWidth;
		m_panels[m_panelCount].height      = pHeight;
		m_panels[m_panelCount].orientation = orientation;
		m_panels[m_panelCount].xOffset     = xOffset;
		m_panels[m_panelCount].yOffset     = yOffset;
		m_panels[m_panelCount].colorOrder  = colorOrder;

		m_panels[m_panelCount].pixelMap.resize(pWidth & pHeight);

		m_outputPanels[output].push_back(m_panelCount);

		LogDebug(VB_CHANNELOUT, "Added Panel %d of size %dx%d at %dx%d facing '%c'.  Color Order: %s (%d).  Matrix size now: %dx%d. Panel Count: %d, Output: %d, Chain: %d\n",
			m_panelCount, pWidth, pHeight, xOffset, yOffset, orientation, ColorOrderToString(colorOrder).c_str(), (int)colorOrder, m_width, m_height, m_panelCount, output, chain);
		m_panelCount++;
	}

	CalculateMaps();

	return 1;
}

int PanelMatrix::CalculateMaps(void)
{
	LogDebug(VB_CHANNELOUT, "PanelMatrix::CalculateMaps(), %d panels\n",
		m_panelCount);

	for (int panel = 0; panel < m_panelCount; panel++)
	{
		int mOffset = 0;
		int pOffset = 0;
		int xOffset = m_panels[panel].xOffset;
		int yOffset = m_panels[panel].yOffset;
		int pWidth  = m_panels[panel].width;
		int pHeight = m_panels[panel].height;

		FPPColorOrder colorOrder = m_panels[panel].colorOrder;

		//LogDebug(VB_CHANNELOUT, "P: %d, O: %c, Pos: %dx%d, w: %d, h: %d\n", panel, m_panels[panel].orientation, xOffset, yOffset, pWidth, pHeight);
		m_panels[panel].pixelMap.resize(pWidth * pHeight * 3);

		for (int y = 0; y < pHeight; y++)
		{
			for (int x = 0; x < pWidth; x++)
			{
				if (m_invertedData)
					mOffset = (((m_height - (yOffset + y) - 1) * m_width) + (xOffset + x));
				else
					mOffset = (((yOffset + y) * m_width) + (xOffset + x));

				switch (m_panels[panel].orientation)
				{
					case 'N':	pOffset = (y * pWidth) + x;
								break;
					case 'U':	pOffset = (pWidth * pHeight) - ((y * pWidth) + x) - 1;
								break;
					case 'L':	pOffset = (x * pHeight) + (pHeight - y - 1);
								break;
					case 'R':	pOffset = ((pWidth - x - 1) * pHeight) + y;
								break;
				}


				mOffset *= 3;
				pOffset *= 3;

				if (colorOrder == kColorOrderRGB)
				{
					m_panels[panel].pixelMap[pOffset    ] = mOffset;
					m_panels[panel].pixelMap[pOffset + 1] = mOffset + 1;
					m_panels[panel].pixelMap[pOffset + 2] = mOffset + 2;
				}
				else if (colorOrder == kColorOrderRBG)
				{
					m_panels[panel].pixelMap[pOffset    ] = mOffset;
					m_panels[panel].pixelMap[pOffset + 2] = mOffset + 1;
					m_panels[panel].pixelMap[pOffset + 1] = mOffset + 2;
				}
				else if (colorOrder == kColorOrderGRB)
				{
					m_panels[panel].pixelMap[pOffset + 1] = mOffset;
					m_panels[panel].pixelMap[pOffset    ] = mOffset + 1;
					m_panels[panel].pixelMap[pOffset + 2] = mOffset + 2;
				}
				else if (colorOrder == kColorOrderGBR)
				{
					m_panels[panel].pixelMap[pOffset + 2] = mOffset;
					m_panels[panel].pixelMap[pOffset    ] = mOffset + 1;
					m_panels[panel].pixelMap[pOffset + 1] = mOffset + 2;
				}
				else if (colorOrder == kColorOrderBRG)
				{
					m_panels[panel].pixelMap[pOffset + 1] = mOffset;
					m_panels[panel].pixelMap[pOffset + 2] = mOffset + 1;
					m_panels[panel].pixelMap[pOffset    ] = mOffset + 2;
				}
				else if (colorOrder == kColorOrderBGR)
				{
					m_panels[panel].pixelMap[pOffset + 2] = mOffset;
					m_panels[panel].pixelMap[pOffset + 1] = mOffset + 1;
					m_panels[panel].pixelMap[pOffset    ] = mOffset + 2;
				}
				else // Assume RGB if somehow invalid
				{
					m_panels[panel].pixelMap[pOffset    ] = mOffset;
					m_panels[panel].pixelMap[pOffset + 1] = mOffset + 1;
					m_panels[panel].pixelMap[pOffset + 2] = mOffset + 2;
				}
			}
		}
	}

	return 1;
}

