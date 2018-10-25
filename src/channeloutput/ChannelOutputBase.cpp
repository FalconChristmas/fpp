/*
 *   ChannelOutputBase class for Falcon Player (FPP)
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

#include <vector>

#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "ChannelOutputBase.h"
#include "common.h"
#include "log.h"

ChannelOutputBase::ChannelOutputBase(unsigned int startChannel,
	 unsigned int channelCount)
  : m_outputType(""),
	m_maxChannels(0),
	m_startChannel(startChannel),
	m_channelCount(channelCount)
{
}

ChannelOutputBase::~ChannelOutputBase()
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::~ChannelOutputBase()\n");
}

int ChannelOutputBase::Init(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Init()\n");

	if (m_channelCount == -1)
		m_channelCount = m_maxChannels;

	DumpConfig();
	return 1;
}

int ChannelOutputBase::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Init(JSON)\n");
	m_outputType = config["type"].asString();
	return Init();
}

int ChannelOutputBase::Init(char *configStr)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Init('%s')\n", configStr);

	std::vector<std::string> configElems = split(configStr, ';');

	for (int i = 0; i < configElems.size(); i++) {
		std::vector<std::string> elem = split(configElems[i], '=');
		if (elem.size() < 2)
			continue;

		if (elem[0] == "type")
			m_outputType = elem[1];
	}
	return Init();
}

int ChannelOutputBase::Close(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Close()\n");
	return 1;
}

void ChannelOutputBase::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Output Type      : %s\n", m_outputType.c_str());
	LogDebug(VB_CHANNELOUT, "    Max Channels     : %u\n", m_maxChannels);
	LogDebug(VB_CHANNELOUT, "    Start Channel    : %u\n", m_startChannel + 1);
	LogDebug(VB_CHANNELOUT, "    Channel Count    : %u\n", m_channelCount);
}
