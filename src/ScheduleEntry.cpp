/*
 *   ScheduleEntry class for the Falcon Player (FPP) 
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

#include <vector>

#include "common.h"
#include "log.h"
#include "ScheduleEntry.h"

ScheduleEntry::ScheduleEntry()
  : m_enabled(0),
	m_playlistName(""),
	m_playlist(NULL),
	m_priority(0),
	m_repeating(0),
	m_dayIndex(0),
	m_weeklySecondCount(0),
	m_startTime(0),
	m_endTime(0),
	m_startHour(0),
	m_startMinute(0),
	m_startSecond(0),
	m_endHour(0),
	m_endMinute(0),
	m_endSecond(0),
	m_startDate(0),
	m_endDate(0),
	m_state(SS_IDLE),
	m_lastStartTime(0),
	m_lastEndTime(0),
	m_thisStartTime(0),
	m_thisEndTime(0),
	m_nextStartTime(0),
	m_nextEndTime(0)
{
	for (int i = 0; i < DAYS_PER_WEEK; i++)
	{
		m_weeklyStartSeconds[i] = 0;
		m_weeklyEndSeconds[i] = 0;
	}

	m_state = SS_IDLE;
}

ScheduleEntry::~ScheduleEntry()
{
}

int ScheduleEntry::LoadFromString(std::string entryStr)
{
	std::vector<std::string> elems = split(entryStr, ',');

	if (elems.size() < 10)
	{
		LogErr(VB_SCHEDULE, "Invalid Schedule Entry: '%s', %d elements\n",
			entryStr.c_str(), elems.size());
		return 0;
	}

	m_enabled            = atoi(elems[0].c_str());
	m_playlistName       = elems[1];
	m_dayIndex           = atoi(elems[2].c_str());
	m_startHour          = atoi(elems[3].c_str());
	m_startMinute        = atoi(elems[4].c_str());
	m_startSecond        = atoi(elems[5].c_str());
	m_endHour            = atoi(elems[6].c_str());
	m_endMinute          = atoi(elems[7].c_str());
	m_endSecond          = atoi(elems[8].c_str());
	m_repeating          = atoi(elems[9].c_str());

	if ((elems.size() > 10) &&
		(elems[10].length() == 10))
		m_startDate = DateStrToInt(elems[10].c_str());
	else
		m_startDate = 20150101;

	if ((elems.size() > 11) &&
		(elems[11].length() == 10))
		m_endDate = DateStrToInt(elems[11].c_str());
	else
		m_startDate = 20991231;

	m_state = SS_IDLE;

	return 1;
}

void ScheduleEntry::CalculateTimes(void)
{
	if (!m_enabled)
	{
		m_nextStartTime = 0;
		m_nextEndTime   = 0;
		m_thisStartTime = 0;
		m_thisEndTime   = 0;

		return;
	}

	time_t currTime = time(NULL);
	struct tm now;
	int nextStartDate = GetCurrentDateInt();

	localtime_r(&currTime, &now);

	if (CurrentDateInRange(m_startDate, m_endDate))
	{
		if (now.tm_hour > m_endHour)
		{
			nextStartDate = GetCurrentDateInt(1);
		}
		else if (now.tm_hour < m_startHour)
		{
		}
		else if (now.tm_hour == m_startHour)
		{
		}
	}
	else
	{
		nextStartDate = m_startDate;
	}



	if (m_state != ScheduleEntry::SS_IDLE)
	{
		m_thisStartTime = m_nextStartTime;
		m_thisEndTime   = m_nextEndTime;
	}

	return;
}


