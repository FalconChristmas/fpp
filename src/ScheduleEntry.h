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
#include "fpp-json.h"
#include <time.h>
#include <utility>

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400
#define SECONDS_PER_WEEK 604800
#define DAYS_PER_WEEK 7

// Values for ScheduleEntry::dayIndex
#define INX_SUN 0
#define INX_MON 1
#define INX_TUE 2
#define INX_WED 3
#define INX_THU 4
#define INX_FRI 5
#define INX_SAT 6
#define INX_EVERYDAY 7
#define INX_WKDAYS 8
#define INX_WKEND 9
#define INX_M_W_F 10
#define INX_T_TH 11
#define INX_SUN_TO_THURS 12
#define INX_FRI_SAT 13
#define INX_ODD_DAY 14
#define INX_EVEN_DAY 15
#define INX_DAY_MASK 0x10000
#define INX_DAY_MASK_SUNDAY 0x04000
#define INX_DAY_MASK_MONDAY 0x02000
#define INX_DAY_MASK_TUESDAY 0x01000
#define INX_DAY_MASK_WEDNESDAY 0x00800
#define INX_DAY_MASK_THURSDAY 0x00400
#define INX_DAY_MASK_FRIDAY 0x00200
#define INX_DAY_MASK_SATURDAY 0x00100
// The masks below are for internal use only, not in UI
#define INX_DAY_MASK_EVERYDAY 0x07F00
#define INX_DAY_MASK_WEEKDAYS 0x03E00
#define INX_DAY_MASK_WEEKEND 0x04100
#define INX_DAY_MASK_M_W_F 0x02A00
#define INX_DAY_MASK_T_TH 0x01400
#define INX_DAY_MASK_SUN_TO_THURS 0x07C00
#define INX_DAY_MASK_FRI_SAT 0x00300
#define INX_DAY_MASK_START_ODD 0x05500
#define INX_DAY_MASK_START_EVEN 0x02A00

class ScheduleEntry {
public:
    ScheduleEntry();
    ~ScheduleEntry();

    void CalculateEaster(int year, int& month, int& day);
    void CalculateChineseNewYear(int year, int& month, int& day);
    void CalculateHebrewDate(int year, const std::string& hebrewMonth, int hebrewDay, int& month, int& day);
    void CalculateIslamicDate(int year, int islamicMonth, int islamicDay, int& month, int& day);
    void CalculateHinduDiwali(int year, int& month, int& day);

    // refYear resolves a floating holiday against a specific year.  Pass 0 (the
    // default) for the historic behaviour of resolving against the current year
    // and rolling forward to the next one once the date has passed.
    std::string DateFromLocaleHoliday(Json::Value& holiday, int refYear = 0);
    std::string CheckHoliday(std::string date, int refYear = 0);

    int LoadFromJson(Json::Value& entry);

    // Map a UI dayIndex (INX_SUN..INX_FRI_SAT) to its INX_DAY_MASK_* bitmask.
    // A custom mask or an odd/even index is returned unchanged, matching the
    // switch this replaces: both the live scheduler and the calendar preview
    // resolve day selections through here so they cannot drift apart.
    static int DayIndexToMask(int dayIndex);

    // For an INX_ODD_DAY / INX_EVEN_DAY entry, does the given day's parity
    // (counted from the FPP epoch, the first commit on 2013-07-15) match this
    // entry's odd/even selection?  Meaningless for non-odd/even entries.
    bool IsOddEvenMatch(time_t dayTime) const;

    void pushStartEndTimes(int day, int &delta, int deltaThreshold);

    // Occurrence expansion anchored to an absolute calendar date rather than to
    // an offset from today.  pushStartEndTimes() can only ever describe the
    // rolling window fppd actually schedules out; these let the schedule be
    // expanded over an arbitrary range for the calendar preview.
    void GetDateRangeForYear(int year, int& sDate, int& eDate);
    bool OccursOnDate(int year, int month, int mday);
    void GetOccurrencesOnDate(int year, int month, int mday,
                              std::vector<std::pair<time_t, time_t>>& occurrences);

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
