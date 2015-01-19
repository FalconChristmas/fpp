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

#ifndef _RGBMATRIX_H
#define _RGBMATRIX_H

#include <string>

using namespace::std;

#ifdef USERGBMATRIX
#	include "led-matrix.h"

	using rgb_matrix::GPIO;
	using rgb_matrix::RGBMatrix;
	using rgb_matrix::Canvas;
#else
#include <stdint.h>
class GPIO {
  public:
	GPIO() {}
	~GPIO() {}

	int Init(void) { return 1; }
};

class Canvas {
  public:
	Canvas(GPIO *a, int b, int c) {}
	~Canvas() {}

	void Clear(void) {}
	void Fill(uint8_t a, uint8_t b, uint8_t c) {}
	void SetPixel(int a, int b, uint8_t c, uint8_t d, uint8_t e) {}
};

class RGBMatrix : public Canvas {
  public:
	RGBMatrix(GPIO * a, int b, int c) : Canvas(a, b, c) {}
	~RGBMatrix() {}
};

#endif

#include "ChannelOutputBase.h"

class RGBMatrixOutput : public ChannelOutputBase {
  public:
	RGBMatrixOutput(unsigned int startChannel, unsigned int channelCount);
	~RGBMatrixOutput();

	int Init(char *configStr);
	int Close(void);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

  private:
	GPIO   *m_gpio;
	Canvas *m_canvas;
	string  m_layout;

	int	m_panels;
	int	m_panelsWide;
	int	m_panelsHigh;
	int     m_width;
	int     m_height;
	int     m_rows;
};

#endif /* _RGBMATRIX_H */
