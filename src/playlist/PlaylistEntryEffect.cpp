/*
 *   Playlist Entry Effect Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2016 the Falcon Player Developers
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

#include "effects.h"
#include "log.h"
#include "PlaylistEntryEffect.h"

/*
 *
 */
PlaylistEntryEffect::PlaylistEntryEffect(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_startChannel(0),
	m_repeat(0),
	m_blocking(0),
	m_effectID(-1)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::PlaylistEntryEffect()\n");

	m_type = "effect";
}

/*
 *
 */
PlaylistEntryEffect::~PlaylistEntryEffect()
{
}

/*
 *
 */
int PlaylistEntryEffect::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::Init()\n");

	if (!config.isMember("mediaFilename"))
	{
		LogDebug(VB_PLAYLIST, "Missing effectName entry\n");
		return 0;
	}

	m_effectName = config["effectName"].asString();

	if (config["startChannel"].asInt())
		m_startChannel = config["startChannel"].asInt();

	if (config["repeat"].asInt())
		m_repeat = config["repeat"].asInt();

	if (config["blocking"].asInt())
		m_blocking = config["blocking"].asInt();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryEffect::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	m_effectID = StartEffect(m_effectName.c_str(), m_startChannel, m_repeat);

	if (m_effectID == -1)
	{
		FinishPlay();
		return 0;
	}

	if (!m_blocking)
		m_effectID = -1;

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryEffect::Process(void)
{
	int result = 0;

	if (!m_blocking)
	{
		FinishPlay();
		return 1;
	}

// FIXME
//	result = IsEffectRunning(m_effectID);

	if (!result)
	{
		FinishPlay();
		m_effectID = -1;
	}

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryEffect::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::Stop()\n");

	if (!m_isPlaying)
		return 1;

	if (m_effectID == -1)
		return 1;

// FIXME
//	if (IsEffectRunning(m_effectID))
//		StopEffect(m_effectID);

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryEffect::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Effect Name  : %s\n", m_effectName.c_str());
	LogDebug(VB_PLAYLIST, "Start Channel: %d\n", m_startChannel);
	LogDebug(VB_PLAYLIST, "Repeat       : %d\n", m_repeat);
	LogDebug(VB_PLAYLIST, "Blocking     : %d\n", m_blocking);
}

/*
 *
 */
Json::Value PlaylistEntryEffect::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["effectName"]   = m_effectName;
	result["startChannel"] = m_startChannel;
	result["repeat"]       = m_repeat;
	result["blocking"]     = m_blocking;

	return result;
}
