#pragma once
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

#include <string>
#include <time.h>
#include <utility>

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400
#define SECONDS_PER_WEEK 604800
#define DAYS_PER_WEEK 7

class ScheduleEntry {
public:
    ScheduleEntry();
    ~ScheduleEntry();

    void CalculateEaster(int year, int& month, int& day);
    std::string DateFromLocaleHoliday(Json::Value& holiday);
    std::string CheckHoliday(std::string date);

    int LoadFromString(std::string entryStr);
    int LoadFromJson(Json::Value& entry);

    void pushStartEndTimes(int day);

    void GetTimeFromSun(time_t& when, bool setStart);

    Json::Value GetJson(void);

    bool enabled;
    std::string playlist;
    bool sequence;
    std::string command;
    Json::Value args;
    bool multisyncCommand;
    std::string multisyncHosts;

    int dayIndex;
    int startHour;
    int startMinute;
    int startSecond;
    int endHour;
    int endMinute;
    int endSecond;
    bool repeat;
    int repeatInterval;

    std::vector<std::pair<int, int>> startEndSeconds;
    std::vector<std::pair<time_t, time_t>> startEndTimes;

    int startDate; // YYYYMMDD format as an integer
    int endDate;   // YYYYMMDD format as an integer
    int stopType;

    int startTimeOffset;
    int endTimeOffset;
    std::string startTimeStr;
    std::string endTimeStr;
    std::string startDateStr;
    std::string endDateStr;
};
