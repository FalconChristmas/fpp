/*
 *   Playlist Entry MQTT Class for Falcon Player (FPP)
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
#include <string.h>

#include "log.h"
#include "mqtt.h"
#include "PlaylistEntryMQTT.h"

/*
 *
 */
PlaylistEntryMQTT::PlaylistEntryMQTT(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryMQTT::PlaylistEntryMQTT()\n");

	m_type = "mqtt";
	m_deprecated = 1;
}

/*
 *
 */
PlaylistEntryMQTT::~PlaylistEntryMQTT()
{
}

/*
 *
 */
int PlaylistEntryMQTT::Init(Json::Value &config)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryMQTT::Init()\n");

	if (!config.isMember("topic"))
	{
		LogErr(VB_PLAYLIST, "Missing topic entry\n");
		return 0;
	}

	if (!config.isMember("message"))
	{
		LogErr(VB_PLAYLIST, "Missing message entry\n");
		return 0;
	}

	m_topic = config["topic"].asString();
	m_message = config["message"].asString();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryMQTT::StartPlaying(void)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryMQTT::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	PlaylistEntryBase::StartPlaying();

	if (mqtt)
		mqtt->PublishRaw(m_topic.c_str(), m_message.c_str());

	FinishPlay();

	return 1;
}

/*
 *
 */
void PlaylistEntryMQTT::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Topic  : %s\n", m_topic.c_str());
	LogDebug(VB_PLAYLIST, "Message: %s\n", m_message.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryMQTT::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["topic"]   = m_topic;
	result["message"] = m_message;

	return result;
}

