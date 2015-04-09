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

/*
 *
 */
PixelString::PixelString()
  : m_portNumber(0),
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
int PixelString::Init(string configStr)
{
	std::vector<std::string> elems = split(configStr, ':');

	if (elems.size() != 9)
	{
		LogErr(VB_CHANNELOUT, "Invalid config string, wrong number of elements: '%s'\n",
			configStr.c_str());
		return 0;
	}

	return Init(atoi(elems[0].c_str()), atoi(elems[1].c_str()),
		atoi(elems[2].c_str()), elems[3], atoi(elems[4].c_str()),
		atoi(elems[5].c_str()), atoi(elems[6].c_str()),
		atoi(elems[7].c_str()), atoi(elems[8].c_str()));
}

int PixelString::Init(int portNumber, int startChannel, int pixelCount,
                string colorOrder, int nullNodes, int hybridMode,
                int reverse, int grouping, int zigZag)
{
	m_portNumber = portNumber;
        m_startChannel = startChannel;
        m_pixelCount = pixelCount;
        m_colorOrder = colorOrder;
        m_nullNodes = nullNodes;
        m_hybridMode = hybridMode;
        m_reverseDirection = reverse;
        m_grouping = grouping;
        m_zigZag = zigZag;

	if ((m_startChannel < 0) || (m_startChannel > FPPD_MAX_CHANNELS) ||
		(m_pixelCount < 0) || (m_pixelCount > 600) ||
		(m_nullNodes < 0) || (m_nullNodes > 600) ||
		((m_nullNodes + m_pixelCount) > 600) ||
		(m_hybridMode < 0) || (m_hybridMode > 1) ||
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

	if (m_hybridMode)
	{
		m_inputChannels += 3;
	}

	if (m_zigZag == m_pixelCount)
		m_zigZag = 0;

	m_outputChannels = m_pixelCount * 3;

	SetupMap();

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
		// FIXME, handle zig zag

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

				if (m_reverseDirection)
					ch = m_startChannel + ((maxGroups - 1 - group) * 3);
				else
					ch = m_startChannel + (group * 3);

				itemsInGroup++;
			}
			else
			{
				if (m_reverseDirection)
					ch = m_startChannel + ((m_pixelCount - 1 - p) * 3);
				else
					ch = m_startChannel + (p * 3);
			}
		}

		if (m_colorOrder == "RGB")
		{
			m_outputMap[offset++] = ch;
			m_outputMap[offset++] = ch + 1;
			m_outputMap[offset++] = ch + 2;
		}
		else if (m_colorOrder == "RBG")
		{
			m_outputMap[offset++] = ch;
			m_outputMap[offset++] = ch + 2;
			m_outputMap[offset++] = ch + 1;
		}
		else if (m_colorOrder == "GRB")
		{
			m_outputMap[offset++] = ch + 1;
			m_outputMap[offset++] = ch;
			m_outputMap[offset++] = ch + 2;
		}
		else if (m_colorOrder == "GBR")
		{
			m_outputMap[offset++] = ch + 1;
			m_outputMap[offset++] = ch + 2;
			m_outputMap[offset++] = ch;
		}
		else if (m_colorOrder == "BRG")
		{
			m_outputMap[offset++] = ch + 2;
			m_outputMap[offset++] = ch;
			m_outputMap[offset++] = ch + 1;
		}
		else if (m_colorOrder == "BGR")
		{
			m_outputMap[offset++] = ch + 2;
			m_outputMap[offset++] = ch + 1;
			m_outputMap[offset++] = ch;
		}
		ch += 3;

LogExcess(VB_CHANNELOUT, "offset: %d (loop bottom), iig: %d\n", offset, itemsInGroup);
	}
LogExcess(VB_CHANNELOUT, "offset: %d (after loop)\n", offset++);

	for (int i = 0; i < m_outputChannels; i++)
	{
LogExcess(VB_CHANNELOUT, "map[%d] = %d\n", i, m_outputMap[i]);
	}
}

/*
 *
 */
void PixelString::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "        port number      : %d\n",
		m_portNumber);
	LogDebug(VB_CHANNELOUT, "        start channel    : %d\n",
		m_startChannel);
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
}
