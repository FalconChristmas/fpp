/*
 *   VirtualDisplay Channel Output for Falcon Player (FPP)
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
	kVPC_Custom,
} VirtualPixelColor;

typedef struct virtualDisplayPixel {
	int x;
	int y;
	int z;
	int ch;
	int r;
	int g;
	int b;
	int cpp;
	unsigned char customR;
	unsigned char customG;
	unsigned char customB;

	VirtualPixelColor vpc;
} VirtualDisplayPixel;

class VirtualDisplayOutput : public ChannelOutputBase {
  public:
	VirtualDisplayOutput(unsigned int startChannel, unsigned int channelCount);
	virtual ~VirtualDisplayOutput();

	virtual int  Init(Json::Value config) override;

	int  InitializePixelMap(void);
	int  ScaleBackgroundImage(std::string &bgFile, std::string &rgbFile);
	void LoadBackgroundImage(void);

	void GetPixelRGB(VirtualDisplayPixel &pixel, unsigned char *channelData,
		unsigned char &r, unsigned char &g, unsigned char &b);
	void DrawPixel(int rOffset, int gOffset, int bOffset,
		unsigned char r, unsigned char g, unsigned char b);
	void DrawPixels(unsigned char *channelData);

	virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;

	std::string  m_backgroundFilename;
	float        m_backgroundBrightness;
	int          m_width;
	int          m_height;
	int          m_bytesPerPixel;
	int          m_bpp;
	float        m_scale;
	int          m_previewWidth;
	int          m_previewHeight;
	std::string  m_fitTo;
	std::string  m_colorOrder;
	unsigned char *m_virtualDisplay;
	int          m_pixelSize;

	uint16_t ***m_rgb565map;

	std::vector<VirtualDisplayPixel> m_pixels;
};

inline void VirtualDisplayOutput::GetPixelRGB(VirtualDisplayPixel &pixel,
	unsigned char *channelData, unsigned char &r, unsigned char &g, unsigned char &b)
{
	if (pixel.vpc == kVPC_Custom)
	{
		r = pixel.customR;
		g = pixel.customG;
		b = pixel.customB;
	}
	else if ((pixel.cpp == 3) ||
		     ((pixel.cpp == 4) && (channelData[pixel.ch + 3] == 0)))
	{
		if (pixel.vpc == kVPC_RGB)
		{
			r = channelData[pixel.ch    ];
			g = channelData[pixel.ch + 1];
			b = channelData[pixel.ch + 2];
		}
		else if (pixel.vpc == kVPC_RBG)
		{
			r = channelData[pixel.ch    ];
			g = channelData[pixel.ch + 2];
			b = channelData[pixel.ch + 1];
		}
		else if (pixel.vpc == kVPC_GRB)
		{
			r = channelData[pixel.ch + 1];
			g = channelData[pixel.ch    ];
			b = channelData[pixel.ch + 2];
		}
		else if (pixel.vpc == kVPC_GBR)
		{
			r = channelData[pixel.ch + 2];
			g = channelData[pixel.ch    ];
			b = channelData[pixel.ch + 1];
		}
		else if (pixel.vpc == kVPC_BRG)
		{
			r = channelData[pixel.ch + 1];
			g = channelData[pixel.ch + 2];
			b = channelData[pixel.ch    ];
		}
		else if (pixel.vpc == kVPC_BGR)
		{
			r = channelData[pixel.ch + 2];
			g = channelData[pixel.ch + 1];
			b = channelData[pixel.ch    ];
		}
	}
	else if (pixel.cpp == 4)
	{
		r = channelData[pixel.ch + 3];
		g = channelData[pixel.ch + 3];
		b = channelData[pixel.ch + 3];
	}
	else if (pixel.cpp == 1)
	{
		if (pixel.vpc == kVPC_Red)
		{
			r = channelData[pixel.ch];
			g = 0;
			b = 0;
		}
		else if (pixel.vpc == kVPC_Green)
		{
			r = 0;
			g = channelData[pixel.ch];
			b = 0;
		}
		else if (pixel.vpc == kVPC_Blue)
		{
			r = 0;
			g = 0;
			b = channelData[pixel.ch];
		}
		else if (pixel.vpc == kVPC_White)
		{
			r = channelData[pixel.ch];
			g = channelData[pixel.ch];
			b = channelData[pixel.ch];
		}
	}
}

#endif /* _VIRTUALDISPLAY_H */
