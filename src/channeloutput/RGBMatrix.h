/*
 *   librgbmatrix handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

#ifndef _RGBMATRIX_H
#define _RGBMATRIX_H

#include <string>

#include "Matrix.h"
#include "PanelMatrix.h"

#include "led-matrix.h"

using rgb_matrix::GPIO;
using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;

#include "ChannelOutputBase.h"

class RGBMatrixOutput : public ChannelOutputBase {
  public:
	RGBMatrixOutput(unsigned int startChannel, unsigned int channelCount);
	~RGBMatrixOutput();

	int Init(Json::Value config);
	int Close(void);

	void PrepData(unsigned char *channelData);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

  private:
	GPIO        *m_gpio;
	Canvas      *m_canvas;
	std::string  m_layout;
	std::string  m_colorOrder;

	int          m_panelWidth;
	int          m_panelHeight;
	int          m_panels;
	int          m_width;
	int          m_height;
	int          m_rows;
	int          m_outputs;
	int          m_longestChain;
	int          m_invertedData;

	Matrix      *m_matrix;
	PanelMatrix *m_panelMatrix;
};

#endif /* _RGBMATRIX_H */
