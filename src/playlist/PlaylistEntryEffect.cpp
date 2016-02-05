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
PlaylistEntryEffect::PlaylistEntryEffect()
  : m_startChannel(0),
	m_loop(0),
	m_blocking(0),
	m_effectID(-1)
{
	m_type = "Effect";
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
	m_effectName = config["effectName"].asString();

	if (config["startChannel"].asInt())
		m_startChannel = config["startChannel"].asInt();

	if (config["loop"].asInt())
		m_loop = config["loop"].asInt();

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

	m_effectID = StartEffect(m_effectName.c_str(), m_startChannel, m_loop);

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
int PlaylistEntryEffect::Stop(void)
{
LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::Stop()\n");
	if (!m_isPlaying)
		return 1;

	if (IsEffectRunning(m_effectName.c_str()))
		StopEffect(m_effectName.c_str());

	return PlaylistEntryBase::Stop();
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

	result = IsEffectRunning(m_effectName.c_str());

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
void PlaylistEntryEffect::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Effect Name  : %s\n", m_effectName.c_str());
	LogDebug(VB_PLAYLIST, "Start Channel: %d\n", m_startChannel);
	LogDebug(VB_PLAYLIST, "Loop         : %d\n", m_loop);
}

