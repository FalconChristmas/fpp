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

#ifndef _SCHEDULEENTRY_H
#define _SCHEDULEENTRY_H

#include <string>

#include <time.h>


#define DAYS_PER_WEEK       7

class ScheduleEntry {
  public:
	ScheduleEntry();
	~ScheduleEntry();

	void        CalculateEaster(int year, int &month, int &day);
	std::string DateFromLocaleHoliday(Json::Value &holiday);
	std::string CheckHoliday(std::string date);

	int  LoadFromString(std::string entryStr);
	int  LoadFromJson(Json::Value &entry);
	Json::Value GetJson(void);

	bool         enabled;
	std::string  playlist;
    
    int          dayIndex;
    int          startHour;
    int          startMinute;
    int          startSecond;
    int          endHour;
    int          endMinute;
    int          endSecond;
	bool         repeat;
    int          weeklySecondCount;
	int          weeklyStartSeconds[DAYS_PER_WEEK];
	int          weeklyEndSeconds[DAYS_PER_WEEK];
	int          startDate; // YYYYMMDD format as an integer
	int          endDate;   // YYYYMMDD format as an integer
	int          stopType;

	std::string  startDateStr;
	std::string  endDateStr;
};

#endif /* _SCHEDULEENTRY_H */
