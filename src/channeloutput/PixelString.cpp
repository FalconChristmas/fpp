/*
 *   PixelString Class for Falcon Player (FPP)
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

#include <stdlib.h>
#include <cmath>

#include "common.h"
#include "log.h"
#include "PixelString.h"
#include "Sequence.h" // for FPPD_MAX_CHANNELS

/////////////////////////////////////////////////////////////////////////////

#define MAX_PIXEL_STRING_LENGTH  999

#define CHECKPS_SETTING(SETTING) if (SETTING) { \
LogErr(VB_CHANNELOUT, "Invalid PixelString Config %s\n", #SETTING); \
return 0; \
}\

/*
 *
 */
PixelString::PixelString()
  : m_portNumber(0),
	m_channelOffset(0),
    m_inputChannels(0),
	m_outputChannels(0)
{
}

/*
 *
 */
PixelString::~PixelString()
{
}

/*
 *
 */
int PixelString::Init(Json::Value config)
{
	m_portNumber = config["portNumber"].asInt();
    m_outputChannels = 0;
    
	for (int i = 0; i < config["virtualStrings"].size(); i++)
	{
		Json::Value vsc = config["virtualStrings"][i];

		VirtualString vs;

		vs.startChannel = vsc["startChannel"].asInt();
		vs.pixelCount = vsc["pixelCount"].asInt();
		vs.groupCount = vsc["groupCount"].asInt();
		vs.reverse = vsc["reverse"].asInt();
		vs.nullNodes = vsc["nullNodes"].asInt();
		vs.zigZag = vsc["zigZag"].asInt();
		vs.brightness = vsc["brightness"].asInt();
		vs.gamma = atof(vsc["gamma"].asString().c_str());

		if (vs.brightness > 100 || vs.brightness < 0) {
			vs.brightness = 100;
		}
		if (vs.gamma < 0.01) {
			vs.gamma = 0.01;
		}
		if (vs.gamma > 50.0f) {
			vs.gamma = 50.0f;
		}

		CHECKPS_SETTING(vs.startChannel < 0);
		CHECKPS_SETTING(vs.startChannel > FPPD_MAX_CHANNELS);
		CHECKPS_SETTING(vs.pixelCount < 0);
		CHECKPS_SETTING(vs.pixelCount > MAX_PIXEL_STRING_LENGTH);
		CHECKPS_SETTING(vs.nullNodes < 0);
		CHECKPS_SETTING(vs.nullNodes > MAX_PIXEL_STRING_LENGTH);
		CHECKPS_SETTING((vs.nullNodes + vs.pixelCount) > MAX_PIXEL_STRING_LENGTH);
		CHECKPS_SETTING(vs.reverse < 0);
		CHECKPS_SETTING(vs.reverse > 1);
		CHECKPS_SETTING(vs.groupCount < 0);
        if (vs.pixelCount) {
            CHECKPS_SETTING(vs.groupCount > vs.pixelCount);
            CHECKPS_SETTING(vs.zigZag > vs.pixelCount);
        }
		CHECKPS_SETTING(vs.zigZag < 0);
        
        std::string colorOrder = vsc["colorOrder"].asString();
        if (colorOrder.length() == 4) {
            if (colorOrder[0] == 'W') {
                vs.whiteOffset = 0;
                colorOrder = colorOrder.substr(1);
            } else {
                vs.whiteOffset = 3;
                colorOrder = colorOrder.substr(0, 3);
            }
        }
        vs.colorOrder = ColorOrderFromString(colorOrder);

		if (vs.groupCount == 1)
			vs.groupCount = 0;

		if (vs.groupCount)
			m_inputChannels += (vs.pixelCount / vs.groupCount * vs.channelsPerNode());
		else
			m_inputChannels += (vs.pixelCount * vs.channelsPerNode());

		if ((vs.zigZag == vs.pixelCount) || (vs.zigZag == 1))
			vs.zigZag = 0;

        m_outputChannels += vs.nullNodes * 3;
        m_outputChannels += vs.pixelCount * vs.channelsPerNode();
        
		float bf = vs.brightness;
		float maxB = bf * 2.55f;
		for (int x = 0; x < 256; x++) {
			float f = x;
			f = maxB * pow(f / 255.0f, vs.gamma);
			if (f > 255.0) {
				f = 255.0;
			}
			if (f < 0.0) {
				f = 0.0;
			}
			vs.brightnessMap[x] = round(f);
		}

		m_virtualStrings.push_back(vs);
	}

	m_outputMap.resize(m_outputChannels);

	// Initialize all maps to an unused location which should be zero.
	// We need this so that null nodes in the middle of a string are sent
	// all zeroes to keep them dark.
	for (int i = 0; i < m_outputChannels; i++)
		m_outputMap[i] = FPPD_MAX_CHANNELS - 2;

	int offset = 0;
	int mapIndex = 0;

	m_brightnessMaps = (uint8_t **)calloc(1, sizeof(uint8_t*) * m_outputChannels);

	for (int i = 0; i < m_virtualStrings.size(); i++)
	{
		offset += m_virtualStrings[i].nullNodes * 3;

		SetupMap(offset, m_virtualStrings[i]);
		offset += m_virtualStrings[i].pixelCount * m_virtualStrings[i].channelsPerNode();

		for (int j = 0; j < ((m_virtualStrings[i].nullNodes*3) + (m_virtualStrings[i].pixelCount*m_virtualStrings[i].channelsPerNode())); j++)
			m_brightnessMaps[mapIndex++] = m_virtualStrings[i].brightnessMap;
	}

	return 1;
}


void PixelString::SetupMap(int vsOffset, VirtualString vs)
{
	int offset        = vsOffset;
	int group         = 0;
	int maxGroups     = 0;
	int itemsInGroup  = 0;
	int ch            = 0;
	int pStart        = 0;
	int ch1           = 0;
	int ch2           = 0;
	int ch3           = 0;

	if (vs.groupCount)
		maxGroups = vs.pixelCount / vs.groupCount;

	for (int p = pStart; p < vs.pixelCount; p++)
	{
		if (vs.groupCount)
		{
			if (itemsInGroup >= vs.groupCount)
			{
				group++;
				itemsInGroup = 0;
			}

			ch = vs.startChannel + (group * vs.channelsPerNode());

			itemsInGroup++;
		}
		else
		{
			ch = vs.startChannel + (p * vs.channelsPerNode());
		}

		if (vs.colorOrder == kColorOrderRGB)
		{
			ch1 = ch;
			ch2 = ch + 1;
			ch3 = ch + 2;
		}
		else if (vs.colorOrder == kColorOrderRBG)
		{
			ch1 = ch;
			ch2 = ch + 2;
			ch3 = ch + 1;
		}
		else if (vs.colorOrder == kColorOrderGRB)
		{
			ch1 = ch + 1;
			ch2 = ch;
			ch3 = ch + 2;
		}
		else if (vs.colorOrder == kColorOrderGBR)
		{
			ch1 = ch + 1;
			ch2 = ch + 2;
			ch3 = ch;
		}
		else if (vs.colorOrder == kColorOrderBRG)
		{
			ch1 = ch + 2;
			ch2 = ch;
			ch3 = ch + 1;
		}
		else if (vs.colorOrder == kColorOrderBGR)
		{
			ch1 = ch + 2;
			ch2 = ch + 1;
			ch3 = ch;
		}
        
        if (vs.whiteOffset == 0) {
            m_outputMap[offset++] = ch;
            ch1++;
            ch2++;
            ch3++;
        }
        m_outputMap[offset++] = ch1;
        m_outputMap[offset++] = ch2;
        m_outputMap[offset++] = ch3;
        if (vs.whiteOffset == 3) {
            m_outputMap[offset++] = ch + 3;
        }
		ch += vs.channelsPerNode();
	}

	DumpMap("BEFORE ZIGZAG");

	if (vs.zigZag)
	{
		int segment = 0;
		int pixel = 0;
		int zigChannelCount = vs.zigZag * vs.channelsPerNode();

		for (int i = 0; i < m_outputChannels; i += zigChannelCount)
		{
			segment = i / zigChannelCount;
			if (segment % 2)
			{
				int offset1 = i;
				int offset2 = i + zigChannelCount - vs.channelsPerNode();

				if ((offset2 + 2) < m_outputChannels)
					FlipPixels(offset1, offset2, vs.channelsPerNode());
			}
		}

		DumpMap("AFTER ZIGZAG");
	}

	if (vs.reverse && (vs.pixelCount > 1))
	{
		FlipPixels(vsOffset, vsOffset + (vs.pixelCount * vs.channelsPerNode()) - vs.channelsPerNode(), vs.channelsPerNode());

		DumpMap("AFTER REVERSE");
	}
}


/*
 *
 */
void PixelString::DumpMap(char *msg)
{
	if ((logLevel == LOG_EXCESSIVE) &&
		(logMask & VB_CHANNELOUT))
	{
		LogExcess(VB_CHANNELOUT, "OutputMap: %s\n", msg);
		for (int i = 0; i < m_outputChannels; i++)
		{
			LogExcess(VB_CHANNELOUT, "map[%d] = %d\n", i, m_outputMap[i]);
		}
	}
}

/*
 *
 */
void PixelString::FlipPixels(int offset1, int offset2, int chanCount)
{
	int ch1 = 0;
	int ch2 = 0;
	int ch3 = 0;
    int ch4 = 0;
	int flipPixels = (offset2 - offset1 + chanCount) / chanCount / 2;

	for (int i = 0; i < flipPixels; i++)
	{
		ch1 = m_outputMap[offset1    ];
		ch2 = m_outputMap[offset1 + 1];
		ch3 = m_outputMap[offset1 + 2];
        if (chanCount == 4) {
            ch4 = m_outputMap[offset1 + 3];
        }

		m_outputMap[offset1    ] = m_outputMap[offset2    ];
		m_outputMap[offset1 + 1] = m_outputMap[offset2 + 1];
		m_outputMap[offset1 + 2] = m_outputMap[offset2 + 2];
        if (chanCount == 4) {
            m_outputMap[offset1 + 3] = m_outputMap[offset2 + 3];
        }
        
		m_outputMap[offset2    ] = ch1;
		m_outputMap[offset2 + 1] = ch2;
		m_outputMap[offset2 + 2] = ch3;
        if (chanCount == 4) {
            m_outputMap[offset2 + 3] = ch4;
        }

		offset1 += chanCount;
		offset2 -= chanCount;
	}
}

/*
 *
 */
void PixelString::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "        port number      : %d\n",
		m_portNumber);

    for (int i = 0; i < m_virtualStrings.size(); i++)
    {
        VirtualString vs = m_virtualStrings[i];

        LogDebug(VB_CHANNELOUT, "        --- Virtual String #%d ---\n", i + 1);
        LogDebug(VB_CHANNELOUT, "        start channel : %d\n",
            vs.startChannel + m_channelOffset);
        LogDebug(VB_CHANNELOUT, "        pixel count   : %d\n",
            vs.pixelCount);
        LogDebug(VB_CHANNELOUT, "        group count   : %d\n",
            vs.groupCount);
        LogDebug(VB_CHANNELOUT, "        reverse       : %d\n",
            vs.reverse);
        LogDebug(VB_CHANNELOUT, "        color order   : %s\n",
            ColorOrderToString(vs.colorOrder).c_str());
        LogDebug(VB_CHANNELOUT, "        null nodes    : %d\n",
            vs.nullNodes);
        LogDebug(VB_CHANNELOUT, "        zig zag       : %d\n",
            vs.zigZag);
        LogDebug(VB_CHANNELOUT, "        brightness    : %d\n",
            vs.brightness);
        LogDebug(VB_CHANNELOUT, "        gamma         : %.3f\n",
            vs.gamma);
    }
}
