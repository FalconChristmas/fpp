/*
 *   ScheduleEntry class for the Falcon Player (FPP) 
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

#include <vector>

#include "common.h"
#include "log.h"
#include "ScheduleEntry.h"

ScheduleEntry::ScheduleEntry()
  : enabled(false),
	playlist(""),
	repeat(false),
	dayIndex(0),
	weeklySecondCount(0),
	startHour(0),
	startMinute(0),
	startSecond(0),
	endHour(0),
	endMinute(0),
	endSecond(0),
	startDate(0),
	endDate(0),
	stopType(0)
{
	for (int i = 0; i < DAYS_PER_WEEK; i++) {
		weeklyStartSeconds[i] = 0;
		weeklyEndSeconds[i] = 0;
	}
}

ScheduleEntry::~ScheduleEntry()
{
}

int ScheduleEntry::LoadFromString(std::string entryStr)
{
	std::vector<std::string> elems = split(entryStr, ',');

	if (elems.size() < 10) {
		LogErr(VB_SCHEDULE, "Invalid Schedule Entry: '%s', %d elements\n",
			entryStr.c_str(), elems.size());
		return 0;
	}

	enabled            = atoi(elems[0].c_str());
	playlist           = elems[1];
	dayIndex           = atoi(elems[2].c_str());
	startHour          = atoi(elems[3].c_str());
	startMinute        = atoi(elems[4].c_str());
	startSecond        = atoi(elems[5].c_str());
	endHour            = atoi(elems[6].c_str());
	endMinute          = atoi(elems[7].c_str());
	endSecond          = atoi(elems[8].c_str());
	repeat             = atoi(elems[9].c_str());

	if ((elems.size() > 10) &&
		(elems[10].length() == 10))
		startDate = DateStrToInt(elems[10].c_str());
	else
		startDate = 20150101;

	if ((elems.size() > 11) &&
		(elems[11].length() == 10))
		endDate = DateStrToInt(elems[11].c_str());
	else
		endDate = 20991231;

	if (elems.size() > 12)
		stopType = atoi(elems[12].c_str());

	return 1;
}

