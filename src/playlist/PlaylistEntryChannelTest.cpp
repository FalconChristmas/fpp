/*
 *   Playlist Entry Channel Test Class for Falcon Player (FPP)
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
#include "fppd.h" // for channelTester
#include "log.h"
#include "PlaylistEntryChannelTest.h"

#define STOP_CONFIG_JSON "{ \"enabled\": 0 }"

/*
 *
 */
PlaylistEntryChannelTest::PlaylistEntryChannelTest(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_duration(0),
	m_startTime(0),
	m_endTime(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryChannelTest::PlaylistEntryChannelTest()\n");

	m_type = "channelTest";
}

/*
 *
 */
PlaylistEntryChannelTest::~PlaylistEntryChannelTest()
{
}

/*
 *
 */
int PlaylistEntryChannelTest::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryChannelTest::Init()\n");

	m_duration = config["duration"].asInt();
	m_endTime = 0;
	m_finishTime = 0;

	m_testConfig = config["testConfig"].asString();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryChannelTest::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryChannelTest::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	// Calculate end time as m_duation number of seconds from now
	m_startTime = GetTime();
	m_endTime = m_startTime + (m_duration * 1000000);

    ChannelTester::INSTANCE.SetupTest(m_testConfig);

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryChannelTest::Process(void)
{
	if (!m_isStarted || !m_isPlaying || m_isFinished)
		return 0;

	if (m_isStarted && m_isPlaying && (GetTime() >= m_endTime))
	{
		std::string stopConfig(STOP_CONFIG_JSON);
		ChannelTester::INSTANCE.SetupTest(stopConfig);

		m_finishTime = GetTime();

		FinishPlay();
	}

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryChannelTest::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryChannelTest::Stop()\n");

	std::string stopConfig(STOP_CONFIG_JSON);
	ChannelTester::INSTANCE.SetupTest(stopConfig);

	m_finishTime = GetTime();

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryChannelTest::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Duration: %d\n", m_duration);
	LogDebug(VB_PLAYLIST, "Cur Time: %lld\n", GetTime());
	LogDebug(VB_PLAYLIST, "Start Time: %lld\n", m_startTime);
	LogDebug(VB_PLAYLIST, "End Time: %lld\n", m_endTime);
	LogDebug(VB_PLAYLIST, "Finish Time: %lld\n", m_finishTime);
	LogDebug(VB_PLAYLIST, "Test Config: %s\n", m_testConfig.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryChannelTest::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["duration"] = m_duration;
	result["startTime"] = (Json::UInt64)m_startTime;
	result["endTime"] = (Json::UInt64)m_endTime;
	result["finishTime"] = (Json::UInt64)m_finishTime;
	result["testConfig"] = m_testConfig;

	if (m_isPlaying)
		result["remaining"] = (Json::UInt64)((m_endTime - GetTime()) / 1000000);
	else
		result["remaining"] = (Json::UInt64)0;

	return result;
}

