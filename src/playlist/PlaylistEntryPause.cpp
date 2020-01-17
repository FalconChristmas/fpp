/*
 *   Playlist Entry Pause Class for Falcon Player (FPP)
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

#include "common.h"
#include "log.h"
#include "PlaylistEntryPause.h"

/*
 *
 */
PlaylistEntryPause::PlaylistEntryPause(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_duration(0.0f),
	m_startTime(0),
	m_endTime(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryPause::PlaylistEntryPause()\n");

	m_type = "pause";
}

/*
 *
 */
PlaylistEntryPause::~PlaylistEntryPause()
{
}

/*
 *
 */
int PlaylistEntryPause::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryPause::Init()\n");

	m_duration = config["duration"].asFloat();
	m_endTime = 0;
	m_finishTime = 0;

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryPause::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryPause::StartPlaying()\n");

	if (!CanPlay())	{
		FinishPlay();
		return 0;
	}

	// Calculate end time as m_duation number of seconds from now
	m_startTime = GetTime();
    
    double tmp = m_duration;
    tmp *= 1000000.0;
	m_endTime = m_startTime + tmp;

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryPause::Process(void)
{
    if (!m_isStarted || !m_isPlaying || m_isFinished) {
		return 0;
    }

    long long now = GetTime();
	if (m_isStarted && m_isPlaying && (now >= m_endTime)) {
		m_finishTime = GetTime();
		FinishPlay();
	}

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryPause::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryPause::Stop()\n");

	m_finishTime = GetTime();
	FinishPlay();

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryPause::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Duration:    %f\n", m_duration);
	LogDebug(VB_PLAYLIST, "Cur Time:    %lld\n", GetTime());
	LogDebug(VB_PLAYLIST, "Start Time:  %lld\n", m_startTime);
	LogDebug(VB_PLAYLIST, "End Time:    %lld\n", m_endTime);
	LogDebug(VB_PLAYLIST, "Finish Time: %lld\n", m_finishTime);
}

/*
 *
 */
Json::Value PlaylistEntryPause::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["duration"] = m_duration;
	result["startTime"] = (Json::UInt64)m_startTime;
	result["endTime"] = (Json::UInt64)m_endTime;
	result["finishTime"] = (Json::UInt64)m_finishTime;

	if (m_isPlaying)
		result["remaining"] = (Json::UInt64)((m_endTime - GetTime()) / 1000000);
	else
		result["remaining"] = (Json::UInt64)0;

	return result;
}

