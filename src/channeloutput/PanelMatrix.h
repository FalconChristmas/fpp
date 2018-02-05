/*
 *   Panel Matrix handler for Falcon Pi Player (FPP)
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

#ifndef _PANELMATRIX_H
#define _PANELMATRIX_H

#include <string>
#include <vector>

#define MAX_MATRIX_OUTPUTS    12
#define MAX_PANELS_PER_OUTPUT 16
#define MAX_MATRIX_PANELS    (MAX_MATRIX_OUTPUTS * MAX_PANELS_PER_OUTPUT)

typedef struct ledPanel {
	int    output;
	int    chain;
	int    width;
	int    height;
	char   orientation;
	int    xOffset;
	int    yOffset;

	std::vector<int> pixelMap;
} LEDPanel;

class PanelMatrix {
  public:
	PanelMatrix(int panelWidth, int panelHeight, int channelsPerPixel = 3,
				int invertedData = 0);
	~PanelMatrix();

	int  AddPanel(int output, int chain, char orientation,
		int xOffset, int yOffset);

	int  ConfigurePanels(std::string layout);

	int  Width(void)      { return m_width; }
	int  Height(void)     { return m_height; }
	int  PanelCount(void) { return m_panelCount; }

	// Map of output channels to full matrix channels
	std::vector<int> m_outputPixelMap[MAX_MATRIX_OUTPUTS];

	// Unsorted list of panels on a output
	std::vector<int> m_outputPanels[MAX_MATRIX_OUTPUTS];
	LEDPanel    m_panels[MAX_MATRIX_PANELS];

  private:
	int  AddPanel(std::string config);

	int CalculateMaps(void);

	int  m_channelsPerPixel;
	int  m_width;
	int  m_height;
	int  m_outputCount;
	int  m_pixelCount;
	int  m_panelCount;
	int  m_panelWidth;
	int  m_panelHeight;
	int  m_invertedData;

};

#endif /* #ifdef _PANELMATRIX_H */
