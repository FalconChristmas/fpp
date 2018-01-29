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

#include "common.h"
#include "log.h"
#include "PixelString.h"
#include "Sequence.h" // for FPPD_MAX_CHANNELS

/////////////////////////////////////////////////////////////////////////////

char *vsColorOrderStr[] = { "RGB", "RBG", "GBR", "GRB", "BGR", "BRG" };

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

		if (vsc["colorOrder"].asString() == "RGB")
			vs.colorOrder = vsColorOrderRGB;
		else if (vsc["colorOrder"].asString() == "RBG")
			vs.colorOrder = vsColorOrderRBG;
		else if (vsc["colorOrder"].asString() == "GBR")
			vs.colorOrder = vsColorOrderGBR;
		else if (vsc["colorOrder"].asString() == "GRB")
			vs.colorOrder = vsColorOrderGRB;
		else if (vsc["colorOrder"].asString() == "BGR")
			vs.colorOrder = vsColorOrderBGR;
		else if (vsc["colorOrder"].asString() == "BRG")
			vs.colorOrder = vsColorOrderBRG;

		if (vs.groupCount == 1)
			vs.groupCount = 0;

		if (vs.groupCount)
			m_inputChannels += (vs.pixelCount / vs.groupCount * 3);
		else
			m_inputChannels += (vs.pixelCount * 3);

		if ((vs.zigZag == vs.pixelCount) || (vs.zigZag == 1))
			vs.zigZag = 0;

		m_virtualStrings.push_back(vs);

		m_pixels += vs.pixelCount + vs.nullNodes;
	}

	m_outputChannels = m_pixels * 3;

	m_outputMap.resize(m_outputChannels);

	// Initialize all maps to an unused location which should be zero.
	// We need this so that null nodes in the middle of a string are sent
	// all zeroes to keep them dark.
	for (int i = 0; i < m_outputChannels; i++)
		m_outputMap[i] = FPPD_MAX_CHANNELS - 2;

	int offset = 0;
	for (int i = 0; i < m_virtualStrings.size(); i++)
	{
		offset += m_virtualStrings[i].nullNodes * 3;

		SetupMap(offset, m_virtualStrings[i]);
		offset += m_virtualStrings[i].pixelCount * 3;
	}

	return 1;
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
		atoi(elems[7].c_str()), atoi(elems[8].c_str()));
}

int PixelString::Init(int portNumber, int channelOffset, int startChannel,
		int pixelCount, std::string colorOrder, int nullNodes,
		int hybridMode, int reverse, int grouping, int zigZag)
{
	m_portNumber = portNumber;
	m_channelOffset = channelOffset;
	m_startChannel = startChannel - channelOffset;
	m_pixelCount = pixelCount;
	m_colorOrder = colorOrder;
	m_nullNodes = nullNodes;
	m_hybridMode = 0; // We don't support hybrid mode in v2.x
	m_reverseDirection = reverse;
	m_grouping = grouping;
	m_zigZag = zigZag;

	if ((m_startChannel < 0) || (m_startChannel > FPPD_MAX_CHANNELS) ||
		(m_pixelCount < 0) || (m_pixelCount > 600) ||
		(m_nullNodes < 0) || (m_nullNodes > 600) ||
		((m_nullNodes + m_pixelCount) > 600) ||
		(m_reverseDirection < 0) || (m_reverseDirection > 1) ||
		(m_grouping < 0) || (m_grouping > m_pixelCount) ||
		(m_zigZag < 0) || (m_zigZag > m_pixelCount))
	{
		LogErr(VB_CHANNELOUT, "Invalid PixelString Config\n");
		return 0;
	}

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

	if ((m_zigZag == m_pixelCount) || (m_zigZag == 1))
		m_zigZag = 0;

	m_outputChannels = m_pixelCount * 3;

	SetupMap();

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

			ch = vs.startChannel + (group * 3);

			itemsInGroup++;
		}
		else
		{
			ch = vs.startChannel + (p * 3);
		}

		if (vs.colorOrder == vsColorOrderRGB)
		{
			ch1 = ch;
			ch2 = ch + 1;
			ch3 = ch + 2;
		}
		else if (vs.colorOrder == vsColorOrderRBG)
		{
			ch1 = ch;
			ch2 = ch + 2;
			ch3 = ch + 1;
		}
		else if (vs.colorOrder == vsColorOrderGRB)
		{
			ch1 = ch + 1;
			ch2 = ch;
			ch3 = ch + 2;
		}
		else if (vs.colorOrder == vsColorOrderGBR)
		{
			ch1 = ch + 1;
			ch2 = ch + 2;
			ch3 = ch;
		}
		else if (vs.colorOrder == vsColorOrderBRG)
		{
			ch1 = ch + 2;
			ch2 = ch;
			ch3 = ch + 1;
		}
		else if (vs.colorOrder == vsColorOrderBGR)
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

	DumpMap("BEFORE ZIGZAG");

	if (vs.zigZag)
	{
		int segment = 0;
		int pixel = 0;
		int zigChannelCount = vs.zigZag * 3;

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

		DumpMap("AFTER ZIGZAG");
	}

	if (vs.reverse && (vs.pixelCount > 1))
	{
		FlipPixels(vsOffset, vsOffset + (vs.pixelCount * 3) - 3);

		DumpMap("AFTER REVERSE");
	}
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

	m_outputMap.resize(m_outputChannels);

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

	DumpMap("BEFORE ZIGZAG");

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

		DumpMap("AFTER ZIGZAG");
	}

	if (m_reverseDirection && (m_pixelCount > 1))
	{
		offset = 0;

		FlipPixels(offset, offset + (m_pixelCount * 3) - 3);

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

	if (m_virtualStrings.size())
	{
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
				vsColorOrderStr[vs.colorOrder]);
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
	else
	{
		LogDebug(VB_CHANNELOUT, "        channel offset: %d\n",
			m_channelOffset);
		LogDebug(VB_CHANNELOUT, "        start channel : %d\n",
			m_startChannel + m_channelOffset);
		LogDebug(VB_CHANNELOUT, "        pixel count   : %d\n",
			m_pixelCount);
		LogDebug(VB_CHANNELOUT, "        color order   : %s\n",
			m_colorOrder.c_str());
		LogDebug(VB_CHANNELOUT, "        null nodes    : %d\n",
			m_nullNodes);
		LogDebug(VB_CHANNELOUT, "        hybrid mode   : %d\n",
			m_hybridMode);
		LogDebug(VB_CHANNELOUT, "        reverse       : %d\n",
			m_reverseDirection);
		LogDebug(VB_CHANNELOUT, "        grouping      : %d\n",
			m_grouping);
		LogDebug(VB_CHANNELOUT, "        zig zag       : %d\n",
			m_zigZag);
	}
}
