/*
 *   Playlist Entry Branch Class for Falcon Player (FPP)
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

#include <log.h>

#include "PlaylistEntryBranch.h"

PlaylistEntryBranch::PlaylistEntryBranch()
  : m_branchType(PE_BRANCH_TYPE_UNDEFINED),
	m_comparisonMode(PE_BRANCH_COMP_MODE_UNDEFINED),
	m_hour(0),
	m_minute(0),
	m_second(0),
	m_loopCount(0),
	m_branch(NULL)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBranch::PlaylistEntryBranch()\n");

	m_type = "branch";
}

PlaylistEntryBranch::~PlaylistEntryBranch()
{
}

/*
 *
 */
int PlaylistEntryBranch::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBranch::Init()\n");

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryBranch::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBranch::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	// FIXME PLAYLIST, finish this

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryBranch::Process(void)
{
	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryBranch::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBranch::Stop()\n");

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryBranch::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Branch Type: %d\n", m_branchType);
	LogDebug(VB_PLAYLIST, "Comparison Mode: %d\n", m_comparisonMode);
	LogDebug(VB_PLAYLIST, "Hour: %d\n", m_hour);
	LogDebug(VB_PLAYLIST, "Minute: %d\n", m_minute);
	LogDebug(VB_PLAYLIST, "Second: %d\n", m_second);
	LogDebug(VB_PLAYLIST, "Loop Count: %d\n", m_loopCount);

	// FIXME PLAYLIST, add some output for the branch here
}

/*
 *
 */
Json::Value PlaylistEntryBranch::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["branchType"] = m_branchType;
	result["comparisonMode"] = m_comparisonMode;
	result["hour"] = m_hour;
	result["minute"] = m_minute;
	result["second"] = m_second;

	return result;
}

