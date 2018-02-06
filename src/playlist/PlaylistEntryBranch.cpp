/*
 *   Playlist Entry Branch Class for Falcon Player (FPP)
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

#include <log.h>
//#include <Player.h>

#include "PlaylistEntryBranch.h"

PlaylistEntryBranch::PlaylistEntryBranch(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_branchType(PE_BRANCH_TYPE_UNDEFINED),
	m_comparisonMode(PE_BRANCH_COMP_MODE_UNDEFINED),
	m_sHour(0),
	m_sMinute(0),
	m_sSecond(0),
	m_sDaySecond(-1),
	m_sHourSecond(-1),
	m_eHour(0),
	m_eMinute(0),
	m_eSecond(0),
	m_eDaySecond(-1),
	m_eHourSecond(-1),
	m_trueNextItem(-1),
	m_falseNextItem(-1)
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

	if (config.isMember("trueNextSection"))
		m_trueNextSection = config["trueNextSection"].asString();

	if (config.isMember("trueNextItem"))
		m_trueNextItem = config["trueNextItem"].asInt();

	if (config.isMember("falseNextSection"))
		m_falseNextSection = config["falseNextSection"].asString();

	if (config.isMember("falseNextItem"))
		m_falseNextItem = config["falseNextItem"].asInt();

	m_branchType = config["branchType"].asInt();
	m_comparisonMode = config["compMode"].asInt();

	if (config.isMember("compInfo"))
	{
		Json::Value ci = config["compInfo"];

		if (m_branchType == PE_BRANCH_TYPE_CLOCK_TIME)
		{
			if (ci.isMember("startHour"))
				m_sHour = ci["startHour"].asInt();

			if (ci.isMember("startMinute"))
				m_sMinute = ci["startMinute"].asInt();

			if (ci.isMember("startSecond"))
				m_sSecond = ci["startSecond"].asInt();

			if (ci.isMember("endHour"))
				m_eHour = ci["endHour"].asInt();

			if (ci.isMember("endMinute"))
				m_eMinute = ci["endMinute"].asInt();

			if (ci.isMember("endSecond"))
				m_eSecond = ci["endSecond"].asInt();

			m_sHourSecond = (m_sMinute * 60) + m_sSecond;
			m_eHourSecond = (m_eMinute * 60) + m_eSecond;

			if (m_sHour >= 0)
				m_sDaySecond  = (m_sHour * 3600) + (m_sMinute * 60) + m_sSecond;

			if (m_eHour >= 0)
				m_eDaySecond  = (m_eHour * 3600) + (m_eMinute * 60) + m_eSecond;
		}
	}

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

		int daySecond = (now.tm_hour * 3600) + (now.tm_min * 60) + now.tm_sec;

		if ((m_sDaySecond >= 0) && (m_eDaySecond >= 0))
		{
			if ((daySecond >= m_sDaySecond) && (daySecond < m_eDaySecond))
				SetNext(1);
			else
				SetNext(0);
		}
		else if (m_sDaySecond >= 0)
		{
			if (daySecond >= m_sDaySecond)
				SetNext(1);
			else
				SetNext(0);
		}
		else if (m_sDaySecond >= 0)
		{
			if (daySecond < m_eDaySecond)
				SetNext(1);
			else
				SetNext(0);
		}
		else // Just compare minutes & seconds
		{
			int hourSecond = (now.tm_min * 60) + now.tm_sec;

			if ((hourSecond >= m_sHourSecond) && (hourSecond < m_eHourSecond))
				SetNext(1);
			else
				SetNext(0);
		}

LogDebug(VB_PLAYLIST, "Now: %02d:%02d:%02d, Start: %02d:%02d:%02d, End: %02d:%02d:%02d, NS: %s, NI: %d\n", now.tm_hour, now.tm_min, now.tm_sec, m_sHour, m_sMinute, m_sSecond, m_eHour, m_eMinute, m_eSecond, m_nextSection.c_str(), m_nextItem);
	}
	else if (m_branchType == PE_BRANCH_TYPE_PLAYLIST_TIME)
	{
		LogErr(VB_PLAYLIST, "ERROR: Playlist time branch is not yet supported!\n");
		SetNext(0);
	}
	else if (m_branchType == PE_BRANCH_TYPE_LOOP_COUNT)
	{
		LogErr(VB_PLAYLIST, "ERROR: Playlist loop count branch is not yet supported!\n");
		SetNext(0);
	}

	PlaylistEntryBase::StartPlaying();

	FinishPlay();

	return 1;
}

/*
 *
 */
void PlaylistEntryBranch::SetNext(int isTrue)
{
	if (isTrue)
	{
		m_nextSection = m_trueNextSection;
		m_nextItem = m_trueNextItem;
	}
	else
	{
		m_nextSection = m_falseNextSection;
		m_nextItem = m_falseNextItem;
	}
}

/*
 *
 */
void PlaylistEntryBranch::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Branch Type       : %d\n", m_branchType);
	LogDebug(VB_PLAYLIST, "Comparison Mode   : %d\n", m_comparisonMode);
	LogDebug(VB_PLAYLIST, "Start Hour        : %d\n", m_sHour);
	LogDebug(VB_PLAYLIST, "Start Minute      : %d\n", m_sMinute);
	LogDebug(VB_PLAYLIST, "Start Second      : %d\n", m_sSecond);
	LogDebug(VB_PLAYLIST, "Start Day Second  : %d\n", m_sDaySecond);
	LogDebug(VB_PLAYLIST, "Start Hour Second : %d\n", m_sHourSecond);
	LogDebug(VB_PLAYLIST, "End Hour          : %d\n", m_eHour);
	LogDebug(VB_PLAYLIST, "End Minute        : %d\n", m_eMinute);
	LogDebug(VB_PLAYLIST, "End Second        : %d\n", m_eSecond);
	LogDebug(VB_PLAYLIST, "End Day Second    : %d\n", m_eDaySecond);
	LogDebug(VB_PLAYLIST, "End Hour Second   : %d\n", m_eHourSecond);
	LogDebug(VB_PLAYLIST, "True Next Section : %s\n", m_trueNextSection.c_str());
	LogDebug(VB_PLAYLIST, "True Next Item    : %d\n", m_trueNextItem);
	LogDebug(VB_PLAYLIST, "False Next Section: %s\n", m_falseNextSection.c_str());
	LogDebug(VB_PLAYLIST, "False Next Item   : %d\n", m_falseNextItem);

}

/*
 *
 */
Json::Value PlaylistEntryBranch::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["branchType"] = m_branchType;
	result["comparisonMode"] = m_comparisonMode;
	result["startHour"] = m_sHour;
	result["startMinute"] = m_sMinute;
	result["endHour"] = m_eHour;
	result["endMinute"] = m_eMinute;

	return result;
}

