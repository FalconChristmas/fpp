/*
 *   OLA Channel Output driver for Falcon Player (FPP)
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

#include "common.h"
#include "OLAOutput.h"
#include "log.h"

#include <ola/Logging.h>

/*
 *
 */
OLAOutput::OLAOutput(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_client(NULL)
{
	LogDebug(VB_CHANNELOUT, "OLAOutput::OLAOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = 1;
}

/*
 *
 */
OLAOutput::~OLAOutput()
{
	LogDebug(VB_CHANNELOUT, "OLAOutput::~OLAOutput()\n");
}

/*
 *
 */
int OLAOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "OLAOutput::Init(JSON)\n");

	for (int i = 0; i < config["universes"].size(); i++)
	{
		Json::Value p = config["universes"][i];
		Universe u;

		u.active = p["active"].asInt();
		u.universe = p["universe"].asInt();
		u.startChannel = p["startChannel"].asInt() - m_startChannel;
		u.channelCount = p["channelCount"].asInt();

		m_universes.push_back(u);
	}

	m_client = new ola::client::StreamingClient(ola::client::StreamingClient::Options());
	if (!m_client || !m_client->Setup())
	{
		LogErr(VB_CHANNELOUT, "Error during OLA setup\n");
		return -1;
	}

	ola::InitLogging(ola::OLA_LOG_WARN, ola::OLA_LOG_STDERR);

	return ThreadedChannelOutputBase::Init(config);
}

/*
 *
 */
int OLAOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "OLAOutput::Close()\n");

	delete m_client;
	m_client = NULL;

	return ThreadedChannelOutputBase::Close();
}
void OLAOutput::GetRequiredChannelRange(int &min, int & max) {
    min = FPPD_MAX_CHANNELS;
    max = 0;
    for (int universe = 0 ; universe < m_universes.size(); universe++) {
        Universe u = m_universes[universe];

        min = std::min(min, u.startChannel);
        max = std::max(max, u.startChannel + u.channelCount);
    }
}
/*
 *
 */
int OLAOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "OLAOutput::RawSendData(%p)\n", channelData);

	for (int universe = 0 ; universe < m_universes.size(); universe++)
	{
		Universe u = m_universes[universe];

		if (u.active)
		{
			m_buffer.Set(channelData + u.startChannel, u.channelCount);

			if (!m_client->SendDmx(u.universe, m_buffer))
			{
				LogErr(VB_CHANNELOUT, "OLA SendDMX failed!\n");
			}
		}
	}

	return m_channelCount;
}

/*
 *
 */
void OLAOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "OLAOutput::DumpConfig()\n");

	for (int i = 0; i < m_universes.size(); i++)
	{
		Universe u = m_universes[i];

		LogDebug(VB_CHANNELOUT, "    Universe: %d\n", u.universe);
		LogDebug(VB_CHANNELOUT, "        Active       : %d\n", u.active);
		LogDebug(VB_CHANNELOUT, "        Start Channel: %d\n",
			u.startChannel + m_startChannel);
		LogDebug(VB_CHANNELOUT, "        Channel Count: %d\n", u.channelCount);
	}

	ThreadedChannelOutputBase::DumpConfig();
}

