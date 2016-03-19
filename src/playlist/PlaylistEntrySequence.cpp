/*
 *   Playlist Entry Sequence Class for Falcon Player (FPP)
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

#include "log.h"
#include "Player.h"
#include "PlaylistEntrySequence.h"

PlaylistEntrySequence::PlaylistEntrySequence()
  : m_duration(0),
	m_sequenceID(0),
	m_priority(0),
	m_startSeconds(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::PlaylistEntrySequence()\n");

	m_type = "sequence";
}

PlaylistEntrySequence::~PlaylistEntrySequence()
{
}

/*
 *
 */
int PlaylistEntrySequence::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::Init()\n");

	if (!config.isMember("sequenceName"))
	{
		LogErr(VB_PLAYLIST, "Missing sequenceName entry\n");
		return 0;
	}

	m_sequenceName = config["sequenceName"].asString();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntrySequence::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	m_sequenceID = player->StartSequence(m_sequenceName, m_priority, m_startSeconds);

	if (!m_sequenceID)
		return 0;

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntrySequence::Process(void)
{
	if (!player->SequenceIsRunning(m_sequenceID))
		FinishPlay();

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntrySequence::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::Stop()\n");

	if (!player->StopSequence(m_sequenceName))
		return 0;

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntrySequence::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Sequence Filename: %s\n", m_sequenceName.c_str());
}

/*
 *
 */
Json::Value PlaylistEntrySequence::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["sequenceName"]    = m_sequenceName;

	// FIXME, need to get things like elapsed/remaining here

	return result;
}
