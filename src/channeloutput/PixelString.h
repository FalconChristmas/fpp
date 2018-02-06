/*
 *   Pixel String Class for Falcon Player (FPP)
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

#ifndef _PIXELSTRING_H
#define _PIXELSTRING_H

#include <jsoncpp/json/json.h>
#include <cmath>
#include <string>
#include <vector>
#include <stdint.h>

typedef enum {
	vsColorOrderRGB,
	vsColorOrderRBG,
	vsColorOrderGBR,
	vsColorOrderGRB,
	vsColorOrderBGR,
	vsColorOrderBRG
} vsColorOrder;

typedef struct virtualString {
	int            startChannel;
	int            pixelCount;
	int            groupCount;
	int            reverse;
	vsColorOrder   colorOrder;
	int            nullNodes;
	int            zigZag;
	int            brightness;
	float          gamma;
} VirtualString;

class PixelString {
  public:
	PixelString();
	~PixelString();

	int  Init(Json::Value config);
	int  Init(std::string configStr);
	int  Init(int portNumber, int channelOffset, int startChannel,
		int pixelCount, std::string colorOrder, int nullNodes,
		int hybridMode, int reverse, int grouping, int zigZag,
        int brightness, float gamma);
	void DumpConfig(void);

	int               m_portNumber;
	int               m_channelOffset;
	int               m_startChannel;
	int               m_pixelCount;
	std::string       m_colorOrder;
	int               m_nullNodes;
	int               m_hybridMode;
	int               m_reverseDirection;
	int               m_grouping;
	int               m_zigZag;
    int               m_brightness;
    float             m_gamma;

	int               m_inputChannels;
	int               m_outputChannels;

	int               m_pixels;

	std::vector<VirtualString>  m_virtualStrings;

	std::vector<int>  m_outputMap;
    uint8_t           m_brightnessMap[256];
  private:
	void SetupMap(int vsOffset, VirtualString vs);
	void SetupMap(void);
	void FlipPixels(int offset1, int offset2);
	void DumpMap(char *msg);

};

#endif /* _PIXELSTRING_H */
