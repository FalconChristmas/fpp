/*
 *   PixelString Class for Falcon Pi Player (FPP)
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
#include <cmath>

#include "common.h"
#include "log.h"
#include "PixelString.h"
#include "Sequence.h" // for FPPD_MAX_CHANNELS

/////////////////////////////////////////////////////////////////////////////

#define MAX_PIXEL_STRING_LENGTH  999


/*
 *
 */
PixelString::PixelString()
  : m_portNumber(0),
	m_channelOffset(0),
	m_startChannel(0),
	m_pixelCount(0),
	m_colorOrder("RGB"),
	m_nullNodes(0),
	m_hybridMode(0),
	m_reverseDirection(0),
	m_grouping(0),
	m_zigZag(0),
	m_inputChannels(0),
	m_outputChannels(0),
    m_brightness(100),
    m_gamma(1.0f)
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
int PixelString::Init(std::string configStr)
{
	std::vector<std::string> elems = split(configStr, ':');

	if (elems.size() != 9)
	{
		LogErr(VB_CHANNELOUT, "Invalid config string, wrong number of elements: '%s'\n",
			configStr.c_str());
		return 0;
	}

	return Init(atoi(elems[0].c_str()), 0, atoi(elems[1].c_str()),
		atoi(elems[2].c_str()), elems[3], atoi(elems[4].c_str()),
		atoi(elems[5].c_str()), atoi(elems[6].c_str()),
		atoi(elems[7].c_str()), atoi(elems[8].c_str()), 100, 1.0f);
}

#define CHECKPS_SETTING(SETTING) if (SETTING) { \
LogErr(VB_CHANNELOUT, "Invalid PixelString Config %s\n", #SETTING); \
return 0; \
}\

int PixelString::Init(int portNumber, int channelOffset, int startChannel,
		int pixelCount, std::string colorOrder, int nullNodes,
		int hybridMode, int reverse, int grouping, int zigZag,
        int brightness, float gamma)
{
	m_portNumber = portNumber;
	m_channelOffset = channelOffset;
	m_startChannel = startChannel - channelOffset;
	m_pixelCount = pixelCount;
	m_colorOrder = colorOrder;
	m_nullNodes = nullNodes;
	m_hybridMode = hybridMode;
	m_reverseDirection = reverse;
	m_grouping = grouping;
	m_zigZag = zigZag;
    m_brightness = brightness;
    m_gamma = gamma;
    if (m_brightness > 100 || m_brightness < 0) {
        m_brightness = 100;
    }
    if (m_gamma < 0.01) {
        m_gamma = 0.01;
    }
    if (m_gamma > 50.0f) {
        m_gamma = 50.0f;
    }
    CHECKPS_SETTING(m_startChannel < 0);
    CHECKPS_SETTING(m_startChannel > FPPD_MAX_CHANNELS);
    CHECKPS_SETTING(m_pixelCount < 0);
    CHECKPS_SETTING(m_pixelCount > MAX_PIXEL_STRING_LENGTH);
    CHECKPS_SETTING(m_nullNodes < 0);
    CHECKPS_SETTING(m_nullNodes > MAX_PIXEL_STRING_LENGTH);
    CHECKPS_SETTING((m_nullNodes + m_pixelCount) > MAX_PIXEL_STRING_LENGTH);
    CHECKPS_SETTING(m_hybridMode < 0);
    CHECKPS_SETTING(m_hybridMode > 1);
    CHECKPS_SETTING(m_reverseDirection < 0);
    CHECKPS_SETTING(m_reverseDirection > 1);
    CHECKPS_SETTING(m_grouping < 0);
    CHECKPS_SETTING(m_grouping > m_pixelCount);
    CHECKPS_SETTING(m_zigZag < 0);
    CHECKPS_SETTING(m_zigZag > m_pixelCount);

	if (m_grouping == 1)
		m_grouping = 0;

	if (m_grouping)
	{
		m_inputChannels = m_pixelCount / m_grouping * 3;
	}
	else
	{
		m_inputChannels = m_pixelCount * 3;
	}

	if (m_hybridMode)
	{
		m_inputChannels += 3;
	}

	if ((m_zigZag == m_pixelCount) || (m_zigZag == 1))
		m_zigZag = 0;

	m_outputChannels = m_pixelCount * 3;

	SetupMap();

    float bf = m_brightness;
    float maxB = bf * 2.55f;
    for (int x = 0; x < 256; x++) {
        float f = x;
        f = maxB * pow(f / 255.0f, gamma);
        if (f > 255.0) {
            f = 255.0;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        m_brightnessMap[x] = std::round(f);
    }
    
	return 1;
}

/*
 *
 */
void PixelString::SetupMap(void)
{
	int offset        = 0;
	int group         = 0;
	int maxGroups     = 0;
	int itemsInGroup  = 0;
	int ch            = 0;
	int pStart        = 0;
	int ch1           = 0;
	int ch2           = 0;
	int ch3           = 0;

	if (m_hybridMode)
	{
		m_outputMap.resize(m_outputChannels + 3);
		pStart = -1;
	}
	else
	{
		m_outputMap.resize(m_outputChannels);
	}

	if (m_grouping)
		maxGroups = m_pixelCount / m_grouping;

	for (int p = pStart; p < m_pixelCount; p++)
	{
		if (p == -1)
		{
			ch = m_startChannel;
		}
		else
		{
			if (m_grouping)
			{
				if (itemsInGroup >= m_grouping)
				{
					group++;
					itemsInGroup = 0;
				}

				ch = m_startChannel + (group * 3);

				itemsInGroup++;
			}
			else
			{
				ch = m_startChannel + (p * 3);
			}
		}

		if (m_colorOrder == "RGB")
		{
			ch1 = ch;
			ch2 = ch + 1;
			ch3 = ch + 2;
		}
		else if (m_colorOrder == "RBG")
		{
			ch1 = ch;
			ch2 = ch + 2;
			ch3 = ch + 1;
		}
		else if (m_colorOrder == "GRB")
		{
			ch1 = ch + 1;
			ch2 = ch;
			ch3 = ch + 2;
		}
		else if (m_colorOrder == "GBR")
		{
			ch1 = ch + 1;
			ch2 = ch + 2;
			ch3 = ch;
		}
		else if (m_colorOrder == "BRG")
		{
			ch1 = ch + 2;
			ch2 = ch;
			ch3 = ch + 1;
		}
		else if (m_colorOrder == "BGR")
		{
			ch1 = ch + 2;
			ch2 = ch + 1;
			ch3 = ch;
		}

		m_outputMap[offset++] = ch1;
		m_outputMap[offset++] = ch2;
		m_outputMap[offset++] = ch3;

		ch += 3;

LogExcess(VB_CHANNELOUT, "offset: %d (loop bottom), iig: %d\n", offset, itemsInGroup);
	}
LogExcess(VB_CHANNELOUT, "offset: %d (after loop)\n", offset++);

	if (m_hybridMode)
	{
		for (int i = 0; i < m_outputChannels + 3; i++)
		{
LogExcess(VB_CHANNELOUT, "map[%d] = %d\n", i, m_outputMap[i]);
		}
	}
	else
	{
		for (int i = 0; i < m_outputChannels; i++)
		{
LogExcess(VB_CHANNELOUT, "map[%d] = %d\n", i, m_outputMap[i]);
		}
	}

	if (m_zigZag)
	{
		int segment = 0;
		int pixel = 0;
		int zigChannelCount = m_zigZag * 3;

		for (int i = 0; i < m_outputChannels; i += zigChannelCount)
		{
			segment = i / zigChannelCount;
			if (segment % 2)
			{
				int offset1 = i;
				int offset2 = i + zigChannelCount - 3;

				if ((offset2 + 2) < m_outputChannels)
					FlipPixels(offset1, offset2);
			}
		}

LogExcess(VB_CHANNELOUT, "AFTER ZIGZAG\n");
		if (m_hybridMode)
		{
			for (int i = 0; i < m_outputChannels + 3; i++)
			{
LogExcess(VB_CHANNELOUT, "map[%d] = %d\n", i, m_outputMap[i]);
			}
		}
		else
		{
			for (int i = 0; i < m_outputChannels; i++)
			{
LogExcess(VB_CHANNELOUT, "map[%d] = %d\n", i, m_outputMap[i]);
			}
		}
	}

	if (m_reverseDirection && (m_pixelCount > 1))
	{
		offset = 0;
		if (m_hybridMode)
			offset += 3;

		FlipPixels(offset, offset + (m_pixelCount * 3) - 3);

LogExcess(VB_CHANNELOUT, "AFTER REVERSE\n");
		if (m_hybridMode)
		{
			for (int i = 0; i < m_outputChannels + 3; i++)
			{
LogExcess(VB_CHANNELOUT, "map[%d] = %d\n", i, m_outputMap[i]);
			}
		}
		else
		{
			for (int i = 0; i < m_outputChannels; i++)
			{
LogExcess(VB_CHANNELOUT, "map[%d] = %d\n", i, m_outputMap[i]);
			}
		}
	}
}

/*
 *
 */
void PixelString::FlipPixels(int offset1, int offset2)
{
	int ch1 = 0;
	int ch2 = 0;
	int ch3 = 0;
	int flipPixels = (offset2 - offset1 + 3) / 3 / 2;

	for (int i = 0; i < flipPixels; i++)
	{
		ch1 = m_outputMap[offset1    ];
		ch2 = m_outputMap[offset1 + 1];
		ch3 = m_outputMap[offset1 + 2];

		m_outputMap[offset1    ] = m_outputMap[offset2    ];
		m_outputMap[offset1 + 1] = m_outputMap[offset2 + 1];
		m_outputMap[offset1 + 2] = m_outputMap[offset2 + 2];

		m_outputMap[offset2    ] = ch1;
		m_outputMap[offset2 + 1] = ch2;
		m_outputMap[offset2 + 2] = ch3;

		offset1 += 3;
		offset2 -= 3;
	}
}

/*
 *
 */
void PixelString::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "        port number      : %d\n",
		m_portNumber);
	LogDebug(VB_CHANNELOUT, "        channel offset   : %d\n",
		m_channelOffset);
	LogDebug(VB_CHANNELOUT, "        start channel    : %d\n",
		m_startChannel + m_channelOffset);
	LogDebug(VB_CHANNELOUT, "        pixel count      : %d\n",
		m_pixelCount);
	LogDebug(VB_CHANNELOUT, "        color order      : %s\n",
		m_colorOrder.c_str());
	LogDebug(VB_CHANNELOUT, "        null nodes       : %d\n",
		m_nullNodes);
	LogDebug(VB_CHANNELOUT, "        hybrid mode      : %d\n",
		m_hybridMode);
	LogDebug(VB_CHANNELOUT, "        reverse direction: %d\n",
		m_reverseDirection);
	LogDebug(VB_CHANNELOUT, "        grouping         : %d\n",
		m_grouping);
	LogDebug(VB_CHANNELOUT, "        zig zag          : %d\n",
		m_zigZag);
}
