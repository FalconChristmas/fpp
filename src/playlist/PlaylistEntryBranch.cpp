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
#include <Player.h>

#include "PlaylistEntryBranch.h"

PlaylistEntryBranch::PlaylistEntryBranch()
  : m_branchType(PE_BRANCH_TYPE_UNDEFINED),
	m_comparisonMode(PE_BRANCH_COMP_MODE_UNDEFINED),
	m_hour(0),
	m_minute(0),
	m_second(0),
	m_loopCount(0),
	m_truePlaylist(NULL),
	m_falsePlaylist(NULL),
	m_conditionTrue(0),
	m_livePlaylist(NULL)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBranch::PlaylistEntryBranch()\n");

	m_type = "branch";
}

PlaylistEntryBranch::~PlaylistEntryBranch()
{
	if (m_truePlaylist)
	{
		m_truePlaylist->Cleanup();
		delete m_truePlaylist;
	}

	if (m_falsePlaylist)
	{
		m_falsePlaylist->Cleanup();
		delete m_falsePlaylist;
	}
}

/*
 *
 */
int PlaylistEntryBranch::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBranch::Init()\n");

	if (((config.isMember("truePlaylistName")) &&
		 (!config["truePlaylistName"].asString().empty())) ||
		(config.isMember("truePlaylist")))
	{
		m_truePlaylist = new Playlist(player, 1);

		if (!m_truePlaylist)
		{
			LogErr(VB_PLAYLIST, "Error creating 'true' playlist\n");
			return 0;
		}

		if (config.isMember("truePlaylist"))
		{
			m_truePlaylist->Load(config["truePlaylist"]);
		}
		else
		{
			m_truePlaylistName = config["truePlaylistName"].asString();
		}
	}

	if (((config.isMember("truePlaylistName")) &&
		 (!config["truePlaylistName"].asString().empty())) ||
		(config.isMember("truePlaylist")))
	{
		m_falsePlaylist = new Playlist(player, 1);

		if (!m_falsePlaylist)
		{
			LogErr(VB_PLAYLIST, "Error creating 'false' playlist\n");
			return 0;
		}

		if (config.isMember("falsePlaylist"))
		{
			m_falsePlaylist->Load(config["falsePlaylist"]);
		}
		else
		{
			m_falsePlaylistName = config["falsePlaylistName"].asString();
		}
	}

	m_branchType = config["branchType"].asInt();
	m_comparisonMode = config["compMode"].asInt();

	m_hour = config["compInfo"]["hour"].asInt();
	m_minute = config["compInfo"]["minute"].asInt();
	m_second = config["compInfo"]["second"].asInt();
	m_loopCount = config["compInfo"]["loopCount"].asInt();

	m_conditionTrue = 0;
	m_livePlaylist = NULL;

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

	if (m_branchType == PE_BRANCH_TYPE_CLOCK_TIME)
	{
		time_t currTime = time(NULL);
		struct tm now;

		localtime_r(&currTime, &now);

		// FIXME PLAYLIST, how do we handle passing midnight???
		if ((m_hour <= now.tm_hour) &&
			(m_minute <= now.tm_min) &&
			(m_second <= now.tm_sec))
		{
			m_conditionTrue = 1;
		}
		else
		{
			m_conditionTrue = 0;
		}
LogDebug(VB_PLAYLIST, "Now: %02d:%02d:%02d, Test: %02d:%02d:%02d, Res: %d\n", now.tm_hour, now.tm_min, now.tm_sec, m_hour, m_minute, m_second, m_conditionTrue);
	}
	else if (m_branchType == PE_BRANCH_TYPE_PLAYLIST_TIME)
	{
		m_conditionTrue = 0;
	}
	else if (m_branchType == PE_BRANCH_TYPE_LOOP_COUNT)
	{
		m_conditionTrue = 0;
	}

	if ((m_conditionTrue) && (m_truePlaylist))
	{
		if (!m_truePlaylistName.empty())
		{
			if (!m_truePlaylist->Load(m_truePlaylistName.c_str()))
				return 0;
		}

		if (!m_truePlaylist->Start())
			return 0;
	}
	else if (m_falsePlaylist)
	{
		if (!m_falsePlaylistName.empty())
		{
			if (!m_falsePlaylist->Load(m_falsePlaylistName.c_str()))
				return 0;
		}

		if (!m_falsePlaylist->Start())
			return 0;
	}

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

