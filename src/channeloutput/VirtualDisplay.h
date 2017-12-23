/*
 *   VirtualDisplay Channel Output for Falcon Player (FPP)
 *
 *   Copyright (C) 2015 the Falcon Player Developers
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

#ifndef _VIRTUALDISPLAY_H
#define _VIRTUALDISPLAY_H

#include <linux/fb.h>
#include <string>
#include <vector>

#include "ChannelOutputBase.h"

typedef enum virtualPixelColor {
	kVPC_RGB,
	kVPC_RBG,
	kVPC_GRB,
	kVPC_GBR,
	kVPC_BRG,
	kVPC_BGR,
	kVPC_RGBW,
	kVPC_Red,
	kVPC_White,
	kVPC_Blue,
	kVPC_Green,
} VirtualPixelColor;

typedef struct virtualDisplayPixel {
	int x;
	int y;
	int ch;
	int r;
	int g;
	int b;
	int cpp;

	VirtualPixelColor vpc;
} VirtualDisplayPixel;

class VirtualDisplayOutput : public ChannelOutputBase {
  public:
	VirtualDisplayOutput(unsigned int startChannel, unsigned int channelCount);
	~VirtualDisplayOutput();

	virtual int  Init(Json::Value config);

	void DrawPixel(int rOffset, int gOffset, int bOffset,
		unsigned char r, unsigned char g, unsigned char b);
	void DrawPixels(unsigned char *channelData);

	void DumpConfig(void);

	std::string  m_backgroundFilename;
	int          m_width;
	int          m_height;
	int          m_bytesPerPixel;
	float        m_scale;
	int          m_previewWidth;
	int          m_previewHeight;
	std::string  m_colorOrder;
	char        *m_virtualDisplay;
	int          m_pixelSize;

	std::vector<VirtualDisplayPixel> m_pixels;
};

#endif /* _VIRTUALDISPLAY_H */
