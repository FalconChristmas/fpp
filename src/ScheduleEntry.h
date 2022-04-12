#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
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

    void pushStartEndTimes(int day, int &delta, int deltaThreshold);

    void GetTimeFromSun(time_t& when, const std::string info,
                        const int infoOffset, int& h, int& m, int& s);

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
