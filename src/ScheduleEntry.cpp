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

#include "fpp-pch.h"

#include "fpp-json.h"

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include <ctime>
#include <ctype.h>
#include <math.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "FPPLocale.h"
#include "SunRise.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"

#include "ScheduleEntry.h"

static time_t GetTimeOnDOW(int day, int hour, int minute, int second) {
    time_t currTime = time(NULL);
    struct tm now;
    localtime_r(&currTime, &now);

    // 'day' is an offset relative to Sunday of the current week so our
    // new mday is adjusted by the current day of the week we are on now
    now.tm_mday = now.tm_mday - now.tm_wday + day;
    now.tm_hour = hour;
    now.tm_min = minute;
    now.tm_sec = second;
    now.tm_isdst = -1; // take Daylight Saving Time into account on new date

    time_t result = mktime(&now);

    return result;
}

/*
 * Build a time_t for an absolute calendar date rather than an offset from the
 * current week.  Used by the arbitrary-range expansion behind the calendar view.
 */
static time_t GetTimeOnDate(int year, int month, int mday,
                            int hour, int minute, int second) {
    struct tm when = {};

    when.tm_year = year - 1900;
    when.tm_mon = month - 1;
    when.tm_mday = mday;
    when.tm_hour = hour;
    when.tm_min = minute;
    when.tm_sec = second;
    when.tm_isdst = -1; // take Daylight Saving Time into account on new date

    return mktime(&when);
}

/*
 * Day of week (0 == Sunday) for an absolute calendar date.
 */
static int GetDOWOnDate(int year, int month, int mday) {
    struct tm when = {};

    when.tm_year = year - 1900;
    when.tm_mon = month - 1;
    when.tm_mday = mday;
    when.tm_hour = 12; // midday avoids DST edges shifting the date
    when.tm_isdst = -1;

    time_t t = mktime(&when);
    struct tm result;
    localtime_r(&t, &result);

    return result.tm_wday;
}

ScheduleEntry::ScheduleEntry() :
    enabled(false),
    playlist(""),
    sequence(false),
    repeat(false),
    dayIndex(0),
    startHour(12),
    startMinute(0),
    startSecond(0),
    endHour(12),
    endMinute(0),
    endSecond(0),
    startDate(0),
    endDate(0),
    stopType(0),
    startTimeOffset(0),
    endTimeOffset(0),
    repeatInterval(0) {
}

ScheduleEntry::~ScheduleEntry() {
}

void ScheduleEntry::CalculateEaster(int year, int& month, int& day) {
    // Formula gleaned from various sources around the 'net.
    int Y, a, c, e, h, k, L;
    double b, d, f, g, i, m;

    Y = year;
    a = Y % 19;
    b = floor(Y / 100);
    c = Y % 100;
    d = floor(b / 4);
    e = (int)b % 4;
    f = floor((b + 8) / 25);
    g = floor((b - f + 1) / 3);
    h = (19 * a + (int)b - (int)d - (int)g + 15) % 30;
    i = floor(c / 4);
    k = c % 4;
    L = (32 + 2 * e + 2 * (int)i - h - k) % 7;
    m = floor((a + 11 * h + 22 * L) / 451);
    month = (int)floor((h + L - 7 * m + 114) / 31);
    day = (int)((h + L - 7 * (int)m + 114) % 31) + 1;
}

void ScheduleEntry::CalculateChineseNewYear(int year, int& month, int& day) {
    // Simplified Chinese New Year calculation
    // Second new moon after winter solstice (approximately)
    // This is an approximation - for exact dates, a full lunar calendar is needed
    
    // These are pre-calculated Chinese New Year dates for 2024-2035
    // Format: {year, month, day}
    int cnyDates[][3] = {
        {2024, 2, 10}, {2025, 1, 29}, {2026, 2, 17}, {2027, 2, 6},
        {2028, 1, 26}, {2029, 2, 13}, {2030, 2, 3}, {2031, 1, 23},
        {2032, 2, 11}, {2033, 1, 31}, {2034, 2, 19}, {2035, 2, 8}
    };
    
    for (int i = 0; i < 12; i++) {
        if (cnyDates[i][0] == year) {
            month = cnyDates[i][1];
            day = cnyDates[i][2];
            return;
        }
    }
    
    // Default fallback if year not in table
    month = 2;
    day = 1;
}

void ScheduleEntry::CalculateHebrewDate(int year, const std::string& hebrewMonth, int hebrewDay, int& month, int& day) {
    // Simplified Hebrew calendar conversion
    // For precise dates, a full Hebrew calendar library would be needed
    
    // Approximate Rosh Hashanah (1 Tishrei) dates for 2024-2035
    // Passover is typically ~6 months earlier
    int roshHashanahDates[][3] = {
        {2024, 10, 3}, {2025, 9, 23}, {2026, 9, 12}, {2027, 10, 2},
        {2028, 9, 21}, {2029, 9, 10}, {2030, 9, 28}, {2031, 9, 18},
        {2032, 9, 6}, {2033, 9, 24}, {2034, 9, 14}, {2035, 10, 4}
    };
    
    if (hebrewMonth == "Tishrei" && hebrewDay == 1) {
        // Rosh Hashanah
        for (int i = 0; i < 12; i++) {
            if (roshHashanahDates[i][0] == year) {
                month = roshHashanahDates[i][1];
                day = roshHashanahDates[i][2];
                return;
            }
        }
    } else if (hebrewMonth == "Nisan" && hebrewDay == 14) {
        // Passover - approximately 6 months before Rosh Hashanah
        int passoverDates[][3] = {
            {2024, 4, 23}, {2025, 4, 13}, {2026, 4, 2}, {2027, 4, 22},
            {2028, 4, 11}, {2029, 3, 31}, {2030, 4, 18}, {2031, 4, 8},
            {2032, 3, 27}, {2033, 4, 14}, {2034, 4, 4}, {2035, 4, 24}
        };
        
        for (int i = 0; i < 12; i++) {
            if (passoverDates[i][0] == year) {
                month = passoverDates[i][1];
                day = passoverDates[i][2];
                return;
            }
        }
    }
    
    // Default fallback
    month = 1;
    day = 1;
}

void ScheduleEntry::CalculateIslamicDate(int year, int islamicMonth, int islamicDay, int& month, int& day) {
    // Simplified Islamic calendar conversion
    // Islamic calendar moves ~11 days earlier each year
    
    // Ramadan (month 9) approximate start dates for 2024-2035
    int ramadanDates[][3] = {
        {2024, 3, 11}, {2025, 3, 1}, {2026, 2, 18}, {2027, 2, 8},
        {2028, 1, 28}, {2029, 1, 17}, {2030, 1, 6}, {2031, 12, 26},
        {2032, 12, 15}, {2033, 12, 5}, {2034, 11, 24}, {2035, 11, 14}
    };
    
    // Eid al-Fitr (month 10, day 1) - approximately 30 days after Ramadan
    int eidFitrDates[][3] = {
        {2024, 4, 10}, {2025, 3, 31}, {2026, 3, 20}, {2027, 3, 10},
        {2028, 2, 27}, {2029, 2, 16}, {2030, 2, 5}, {2031, 1, 25},
        {2032, 1, 14}, {2033, 1, 3}, {2034, 12, 24}, {2035, 12, 13}
    };
    
    if (islamicMonth == 9 && islamicDay == 1) {
        // Ramadan start
        for (int i = 0; i < 12; i++) {
            if (ramadanDates[i][0] == year) {
                month = ramadanDates[i][1];
                day = ramadanDates[i][2];
                return;
            }
        }
    } else if (islamicMonth == 10 && islamicDay == 1) {
        // Eid al-Fitr
        for (int i = 0; i < 12; i++) {
            if (eidFitrDates[i][0] == year) {
                month = eidFitrDates[i][1];
                day = eidFitrDates[i][2];
                return;
            }
        }
    }
    
    // Default fallback
    month = 1;
    day = 1;
}

void ScheduleEntry::CalculateHinduDiwali(int year, int& month, int& day) {
    // Diwali falls on the new moon of Kartika (October/November)
    // Pre-calculated dates for 2024-2035
    int diwaliDates[][3] = {
        {2024, 11, 1}, {2025, 10, 21}, {2026, 11, 9}, {2027, 10, 29},
        {2028, 10, 17}, {2029, 11, 5}, {2030, 10, 26}, {2031, 11, 14},
        {2032, 11, 2}, {2033, 10, 23}, {2034, 11, 12}, {2035, 11, 1}
    };
    
    for (int i = 0; i < 12; i++) {
        if (diwaliDates[i][0] == year) {
            month = diwaliDates[i][1];
            day = diwaliDates[i][2];
            return;
        }
    }
    
    // Default fallback
    month = 11;
    day = 1;
}

std::string ScheduleEntry::DateFromLocaleHoliday(Json::Value& holiday, int refYear) {
    int year = 0;
    int month = 0;
    int day = 0;
    time_t currTime = time(NULL);
    struct tm now;
    localtime_r(&currTime, &now);

    // When a reference year is supplied we resolve the holiday strictly within
    // that year.  The roll-forward heuristics below only make sense for "the
    // next occurrence from today" and would skew a date asked for explicitly.
    bool rollForward = (refYear == 0);

    if (holiday.isMember("calc")) {
        // We need to calculate the month/day of the holiday since it changes yearly
        Json::Value c = holiday["calc"];
        std::string type = c["type"].asString();
        int week = c["week"].asInt();
        int dow = c["dow"].asInt();
        int offset = c["offset"].asInt();

        year = rollForward ? (now.tm_year + 1900) : refYear;

        if ((type == "head") || (type == "tail")) {
            month = c["month"].asInt();
            if ((month < 1) || (month > 12)) {
                LogErr(VB_SCHEDULE, "Invalid month %d in holiday definition, using January\n", month);
                WarningHolder::AddWarning(47, "Schedule: invalid month in holiday definition");
                month = 1;
            }
            if (rollForward && (month < (now.tm_mon + 1)))
                year++;
        }

        int mdays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        int wday = 0;

        if ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)))
            mdays[1]++; // add one day to February for leap year

        if ((type == "head") || (type == "tail")) {
            // Day of week of the 1st of the month.  Only head/tail use wday,
            // and only they set `month` - computing it unconditionally up here
            // indexed t[month - 1] with month still 0 for every easter/lunar
            // holiday, i.e. t[-1].
            // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
            static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
            int d = 1;
            int y = year - (month < 3);
            wday = (y + y / 4 - y / 100 + y / 400 + t[month - 1] + d) % 7;
        }

        if (type == "easter") {
            CalculateEaster(year, month, day);

            // Check if Easter was more than 4 months ago.  We do this since some Easter-related
            // special days are 2 months after Easter so we want to calculate those based on the
            // current year.
            if (rollForward && ((month + 3) < now.tm_mon)) {
                year++;
                CalculateEaster(year, month, day);
            }

            // We have Easter, see if we are looking for something offset from Easter
            if (offset != 0) {
                time_t t = (365 * year) + (year / 4) - (year / 100) + (year / 400); // get years
                t += (30 * month) + (3 * (month + 1) / 5) + (day + 1);              // add months and days
                t -= 719561;                                                        // convert to epoch
                t += offset;                                                        // apply our days offset
                t *= 86400;                                                         // multiple to seconds

                struct tm ts;
                localtime_r(&t, &ts);

                month = ts.tm_mon + 1;
                day = ts.tm_mday;
            }
        } else if (type == "head") {
            day = ((dow + 7 - wday) % 7) + ((week - 1) * 7) + 1;
//            if (dow == wday)
//                day += 7;

            day += offset;

            // See if offset pushed us into last month
            if (day < 1) {
                day -= offset;
                month--;
                if (month < 1)
                    month = 12;

                offset += day;
                day = mdays[month - 1] + offset;
            }
        } else if (type == "tail") {
            int ldow = wday + ((mdays[month - 1] - 1) % 7);
            day = mdays[month - 1] - ((ldow + 7 - dow) % 7) - ((week - 1) * 7);
            day += offset;
        } else if (type == "lunar") {
            // Handle lunar calendar calculations
            std::string calendar = c.isMember("calendar") ? c["calendar"].asString() : "";
            
            if (calendar == "chinese") {
                CalculateChineseNewYear(year, month, day);
            } else if (calendar == "hebrew") {
                std::string hebrewMonth = c.isMember("hebrewMonth") ? c["hebrewMonth"].asString() : "";
                int hebrewDay = c.isMember("hebrewDay") ? c["hebrewDay"].asInt() : 1;
                CalculateHebrewDate(year, hebrewMonth, hebrewDay, month, day);
            } else if (calendar == "islamic") {
                int islamicMonth = c.isMember("islamicMonth") ? c["islamicMonth"].asInt() : 1;
                int islamicDay = c.isMember("islamicDay") ? c["islamicDay"].asInt() : 1;
                CalculateIslamicDate(year, islamicMonth, islamicDay, month, day);
            } else if (calendar == "hindu") {
                CalculateHinduDiwali(year, month, day);
            } else {
                LogErr(VB_SCHEDULE, "Unknown lunar calendar type %s\n", calendar.c_str());
                WarningHolder::AddWarning(47, "Schedule: unknown lunar calendar type '" + calendar + "'");
                return "0000-00-00";
            }
            
            // Check if the calculated date has passed this year
            if (rollForward &&
                ((month < (now.tm_mon + 1)) ||
                 (month == (now.tm_mon + 1) && day < now.tm_mday))) {
                year++;
                // Recalculate for next year
                if (calendar == "chinese") {
                    CalculateChineseNewYear(year, month, day);
                } else if (calendar == "hebrew") {
                    std::string hebrewMonth = c.isMember("hebrewMonth") ? c["hebrewMonth"].asString() : "";
                    int hebrewDay = c.isMember("hebrewDay") ? c["hebrewDay"].asInt() : 1;
                    CalculateHebrewDate(year, hebrewMonth, hebrewDay, month, day);
                } else if (calendar == "islamic") {
                    int islamicMonth = c.isMember("islamicMonth") ? c["islamicMonth"].asInt() : 1;
                    int islamicDay = c.isMember("islamicDay") ? c["islamicDay"].asInt() : 1;
                    CalculateIslamicDate(year, islamicMonth, islamicDay, month, day);
                } else if (calendar == "hindu") {
                    CalculateHinduDiwali(year, month, day);
                }
            }
        } else {
            LogErr(VB_SCHEDULE, "Unknown holiday calculation type %s\n", type.c_str());
            WarningHolder::AddWarning(47, "Schedule: unknown holiday calculation type '" + type + "'");
            return "0000-00-00";
        }

        year = 0;
    } else {
        year = holiday["year"].asInt();
        month = holiday["month"].asInt();
        day = holiday["day"].asInt();
    }

    char str[11];
    snprintf(str, 11, "%04d-%02d-%02d", year, month, day);

    std::string result = str;

    return result;
}

std::string ScheduleEntry::CheckHoliday(std::string date, int refYear) {
    Json::Value locale = LocaleHolder::GetLocale();
    std::string result;

    if (!locale.isMember("holidays"))
        return date;

    if (isdigit(date.c_str()[0]))
        return date;

    for (int i = 0; i < locale["holidays"].size(); i++) {
        if (date == locale["holidays"][i]["shortName"].asString())
            return DateFromLocaleHoliday(locale["holidays"][i], refYear);
    }

    return date;
}

int ScheduleEntry::LoadFromString(std::string entryStr) {
    std::vector<std::string> elems = split(entryStr, ',');

    if (elems.size() < 10) {
        LogErr(VB_SCHEDULE, "Invalid Schedule Entry: '%s', %d elements\n",
               entryStr.c_str(), elems.size());
        WarningHolder::AddWarning(47, "Schedule: incomplete entry (missing fields)");
        return 0;
    }

    enabled = atoi(elems[0].c_str());
    playlist = elems[1];
    dayIndex = atoi(elems[2].c_str());
    startHour = atoi(elems[3].c_str());
    startMinute = atoi(elems[4].c_str());
    startSecond = atoi(elems[5].c_str());
    endHour = atoi(elems[6].c_str());
    endMinute = atoi(elems[7].c_str());
    endSecond = atoi(elems[8].c_str());
    repeat = atoi(elems[9].c_str());

    if (elems.size() > 10) {
        startDateStr = elems[10];
        std::string tempStr = CheckHoliday(startDateStr);

        if (tempStr.length() == 10) {
            startDate = DateStrToInt(tempStr.c_str());
        } else {
            startDate = 20190101;
            startDateStr = "20190101";
        }
    }

    if (elems.size() > 11) {
        endDateStr = elems[11];
        std::string tempStr = CheckHoliday(endDateStr);

        if (tempStr.length() == 10) {
            endDate = DateStrToInt(tempStr.c_str());
        } else {
            endDate = 20991231;
            endDateStr = "20991231";
        }
    }

    if (elems.size() > 12)
        stopType = atoi(elems[12].c_str());

    return 1;
}

static void mapTimeString(const std::string& tm, int& h, int& m, int& s) {
    if (!ParseTimeString(tm, h, m, s)) {
        LogErr(VB_SCHEDULE, "Invalid time '%s' in schedule, using midnight\n", tm.c_str());
        WarningHolder::AddWarning(47, "Schedule: invalid time '" + tm + "'");
        h = 0;
        m = 0;
        s = 0;
    }
}

void ScheduleEntry::pushStartEndTimes(int dow, int& delta, int deltaThreshold) {
    time_t startTime = GetTimeOnDOW(dow, startHour, startMinute, startSecond);
    time_t endTime = GetTimeOnDOW(dow, endHour, endMinute, endSecond);

    if (startTimeStr.find(":") == std::string::npos) {
        GetTimeFromSun(startTime, startTimeStr, startTimeOffset, startHour, startMinute, startSecond);
    }
    if (endTimeStr.find(":") == std::string::npos) {
        GetTimeFromSun(endTime, endTimeStr, endTimeOffset, endHour, endMinute, endSecond);
    }
    if (endTime < startTime) {
        endTime += 24 * 60 * 60;
        if (endTimeStr.find(":") == std::string::npos) {
            GetTimeFromSun(endTime, endTimeStr, endTimeOffset, endHour, endMinute, endSecond);
        }
    }

    if (endTime < time(NULL))
        return;

    if (!DateInRange(startTime, startDate, endDate)) {
        return;
    }

    if (delta != 0) {
        // Only adjust if the threshold and start times are in the future
        if (deltaThreshold > time(NULL)) {
            if (deltaThreshold > startTime && startTime > time(NULL)) {
                startTime += delta;
            }

            if (deltaThreshold > endTime) {
                endTime += delta;
            }
        } else {
            // if threshold time has passed, set delta back to 0
            delta = 0;
        }
    }

    if ((repeatInterval > 0) && (startTime != endTime)) {
        time_t newEnd = startTime + repeatInterval - 1;
        while (startTime < endTime) {
            startEndTimes.emplace_back(startTime, newEnd);
            newEnd += repeatInterval;
            if (newEnd > endTime) {
                newEnd = endTime;
            }
            startTime += repeatInterval;
        }
    } else {
        startEndTimes.emplace_back(startTime, endTime);
    }
}

/*
 * Resolve this entry's start/end date range as it applies to a given year.
 *
 * startDate/endDate are resolved once at load time, so a holiday-anchored range
 * carries whichever date the holiday fell on for the year FPP was started in.
 * Floating holidays (Easter, Thanksgiving, the lunar calendars) move year to
 * year, so re-resolve them against the year actually being expanded.
 */
void ScheduleEntry::GetDateRangeForYear(int year, int& sDate, int& eDate) {
    sDate = startDate;
    eDate = endDate;

    if (!startDateStr.empty() && !isdigit(startDateStr[0])) {
        std::string resolved = CheckHoliday(startDateStr, year);
        if (resolved.length() == 10)
            sDate = DateStrToInt(resolved.c_str());
    }

    if (!endDateStr.empty() && !isdigit(endDateStr[0])) {
        std::string resolved = CheckHoliday(endDateStr, year);
        if (resolved.length() == 10)
            eDate = DateStrToInt(resolved.c_str());
    }
}

int ScheduleEntry::DayIndexToMask(int dayIndex) {
    switch (dayIndex) {
    case INX_SUN:
        return INX_DAY_MASK_SUNDAY;
    case INX_MON:
        return INX_DAY_MASK_MONDAY;
    case INX_TUE:
        return INX_DAY_MASK_TUESDAY;
    case INX_WED:
        return INX_DAY_MASK_WEDNESDAY;
    case INX_THU:
        return INX_DAY_MASK_THURSDAY;
    case INX_FRI:
        return INX_DAY_MASK_FRIDAY;
    case INX_SAT:
        return INX_DAY_MASK_SATURDAY;
    case INX_EVERYDAY:
        return INX_DAY_MASK_EVERYDAY;
    case INX_WKDAYS:
        return INX_DAY_MASK_WEEKDAYS;
    case INX_WKEND:
        return INX_DAY_MASK_WEEKEND;
    case INX_M_W_F:
        return INX_DAY_MASK_M_W_F;
    case INX_T_TH:
        return INX_DAY_MASK_T_TH;
    case INX_SUN_TO_THURS:
        return INX_DAY_MASK_SUN_TO_THURS;
    case INX_FRI_SAT:
        return INX_DAY_MASK_FRI_SAT;
    }

    // A custom day mask (INX_DAY_MASK set) or an odd/even index carries no
    // enumerated case; it is already a mask or is resolved separately, so
    // return it unchanged.
    return dayIndex;
}

bool ScheduleEntry::IsOddEvenMatch(time_t dayTime) const {
    // Odd/Even is based on the FPP 'epoch', the date of the first commit to
    // the FPP repository on github, July 15, 2013.
    struct std::tm FPPEpoch = { 0, 0, 0, 15, 6, 113 };
    std::time_t FPPEpochTimeT = std::mktime(&FPPEpoch);
    int daysSince = (int)std::difftime(dayTime, FPPEpochTimeT) / SECONDS_PER_DAY;

    if (daysSince % 2)
        return dayIndex == INX_ODD_DAY;

    return dayIndex == INX_EVEN_DAY;
}

/*
 * Does this entry's day selection include the given absolute date?
 */
bool ScheduleEntry::OccursOnDate(int year, int month, int mday) {
    if ((dayIndex == INX_ODD_DAY) || (dayIndex == INX_EVEN_DAY))
        return IsOddEvenMatch(GetTimeOnDate(year, month, mday, 12, 0, 0));

    int mask = DayIndexToMask(dayIndex);

    // Day masks run Sunday (0x04000) through Saturday (0x00100)
    int dowBit = INX_DAY_MASK_SUNDAY >> GetDOWOnDate(year, month, mday);

    return (mask & dowBit) != 0;
}

/*
 * Expand this entry into the occurrences that fall on a given absolute date.
 *
 * This mirrors pushStartEndTimes() but is anchored to a real date instead of an
 * offset from the current week, and keeps occurrences that are already in the
 * past, so the calendar view can show any range the user navigates to.  The
 * scheduler's time delta is deliberately not applied - it is a transient
 * adjustment to the running schedule, not part of the configured schedule.
 *
 * The entry's enabled flag is not consulted here so callers can choose to show
 * disabled entries; filter on it yourself if you only want live occurrences.
 */
void ScheduleEntry::GetOccurrencesOnDate(int year, int month, int mday,
                                         std::vector<std::pair<time_t, time_t>>& occurrences) {
    if (!OccursOnDate(year, month, mday))
        return;

    time_t startTime = GetTimeOnDate(year, month, mday, startHour, startMinute, startSecond);
    time_t endTime = GetTimeOnDate(year, month, mday, endHour, endMinute, endSecond);

    if (startTimeStr.find(":") == std::string::npos) {
        int h = startHour, m = startMinute, s = startSecond;
        GetTimeFromSun(startTime, startTimeStr, startTimeOffset, h, m, s);
    }
    if (endTimeStr.find(":") == std::string::npos) {
        int h = endHour, m = endMinute, s = endSecond;
        GetTimeFromSun(endTime, endTimeStr, endTimeOffset, h, m, s);
    }
    if (endTime < startTime) {
        endTime += SECONDS_PER_DAY;
        if (endTimeStr.find(":") == std::string::npos) {
            int h = endHour, m = endMinute, s = endSecond;
            GetTimeFromSun(endTime, endTimeStr, endTimeOffset, h, m, s);
        }
    }

    int sDate = 0;
    int eDate = 0;
    GetDateRangeForYear(year, sDate, eDate);

    if (!DateInRange(startTime, sDate, eDate))
        return;

    if ((repeatInterval > 0) && (startTime != endTime)) {
        time_t newEnd = startTime + repeatInterval - 1;
        while (startTime < endTime) {
            occurrences.push_back(std::pair<time_t, time_t>(startTime, newEnd));
            newEnd += repeatInterval;
            if (newEnd > endTime) {
                newEnd = endTime;
            }
            startTime += repeatInterval;
        }
    } else {
        occurrences.push_back(std::pair<time_t, time_t>(startTime, endTime));
    }
}

// Raised whenever the Latitude/Longitude settings can't be used, whether they
// are unset or unparseable, so that the add and the remove always pair on the
// same text.  See www/help/warning-helpers/warning-48.md.
static const char* LATLON_WARNING = "Schedule: Latitude/Longitude not set or not numeric — dawn/dusk times use default coordinates";

// strtod() plus an end-pointer check.  A coordinate has to consume the whole
// string, so "38.9N" is rejected rather than silently becoming 38.9.
static bool ParseCoordinate(const std::string& str, double& out) {
    if (str.empty()) {
        return false;
    }

    const char* start = str.c_str();
    char* end = nullptr;
    errno = 0;
    double v = strtod(start, &end);

    if ((end == start) || (errno == ERANGE)) {
        return false;
    }

    while ((*end == ' ') || (*end == '\t') || (*end == '\r') || (*end == '\n')) {
        end++;
    }

    if (*end != '\0') {
        return false;
    }

    out = v;
    return true;
}

void ScheduleEntry::GetTimeFromSun(time_t& when, const std::string info,
                                   const int infoOffset,
                                   int& h, int& m, int& s) {
    time_t origWhen = when;
    std::string latStr = getSetting("Latitude");
    std::string lonStr = getSetting("Longitude");

    double lat = 0.0;
    double lon = 0.0;

    // Latitude/Longitude are free-text settings living in a hand-editable file,
    // so they can hold anything - "N38.9", a stray unit, an empty string.
    // std::stod() throws on those, and this runs on the daemon startup path
    // (Scheduler ctor -> LoadScheduleFromFile -> pushStartEndTimes) with no
    // catch anywhere above it, which turned one bad character into a boot loop.
    // Parse without exceptions and fall back to the defaults instead.
    if (!ParseCoordinate(latStr, lat) || !ParseCoordinate(lonStr, lon)) {
        lat = 38.938524;
        lon = -104.600945;

        LogErr(VB_SCHEDULE, "Error, Latitude/Longitude not usable ('%s', '%s'), using Falcon, Colorado coordinates!\n",
               latStr.c_str(), lonStr.c_str());
        WarningHolder::AddWarning(48, LATLON_WARNING);
    } else {
        // Coordinates are usable - clear the warning if it was previously raised.
        WarningHolder::RemoveWarning(48, LATLON_WARNING);
    }
    double sunOffset = 0;
    time_t currTime = when;
    time_t midnight = when;
    struct tm local;
    struct tm mtm;

    localtime_r(&currTime, &local);
    localtime_r(&currTime, &mtm);

    int daySecond = (local.tm_hour * SECONDS_PER_HOUR) + (local.tm_min * SECONDS_PER_MINUTE) + local.tm_sec;

    LogExcess(VB_SCHEDULE, "Lat/Lon: %.6f, %.6f\n", lat, lon);
    LogExcess(VB_SCHEDULE, "Lookup Date is %02d/%02d/%04d\n",
              local.tm_mon + 1, local.tm_mday, local.tm_year + 1900);

    SunRise sr;
    if ((info == "Dawn") || (info == "Dusk"))
        sr.calculate(lat, lon, when, TT_CIVIL);
    else
        sr.calculate(lat, lon, when);

    // Find midnight on the 'when' day
    mtm.tm_hour = 0;
    mtm.tm_min = 0;
    mtm.tm_sec = 0;
    midnight = mktime(&mtm);

    if (info == "SunRise")
        sunOffset = sr.riseTime - midnight;
    else if (info == "SunSet")
        sunOffset = sr.setTime - midnight;
    else if (info == "Dawn")
        sunOffset = sr.riseTime - midnight;
    else if (info == "Dusk")
        sunOffset = sr.setTime - midnight;
    else
        sunOffset = 0;

    // convert sunOffset to minutes for later calculations
    sunOffset /= 60.0;

    sunOffset += infoOffset;

    if ((sunOffset < 0) || (sunOffset >= (24 * 60))) {
        if (sunOffset < 0) {
            LogWarn(VB_SCHEDULE, "%s offset calculated as %.2f minutes, using 8AM. "
                                 "Check your time zone to make sure it is valid.\n",
                    info.c_str(), sunOffset);
            h = 8;
        } else {
            LogWarn(VB_SCHEDULE, "%s offset calculated as %.2f minutes, using 8PM. "
                                 "Check your time zone to make sure it is valid.\n",
                    info.c_str(), sunOffset);
            h = 20;
        }
        m = 0;
        s = 0;

        // FIXME, move the following to Excess once the issues are worked out with SunRise class
        LogWarn(VB_SCHEDULE, "w: %lld, m: %lld, rt: %lld, st: %lld\n",
                (long long)when, (long long)midnight, (long long)sr.riseTime, (long long)sr.setTime);
        LogWarn(VB_SCHEDULE, "info: %s, infoOffset: %d, sunOffset: %.4f, lat: %.6f, lon: %.6f, date: %02d/%02d/%04d\n",
                info.c_str(), infoOffset, sunOffset, lat, lon,
                local.tm_mon + 1, local.tm_mday, local.tm_year + 1900);

        when = when - daySecond + (h * SECONDS_PER_HOUR);
        return;
    }

    LogExcess(VB_SCHEDULE, "%s Time Offset: %.2f minutes\n", info.c_str(), sunOffset);
    h = (int)sunOffset / 60;
    m = (int)sunOffset % 60;
    s = (int)(((int)(sunOffset * 100) % 100) * 0.01 * 60);

    when = when - daySecond + (h * SECONDS_PER_HOUR) + (m * SECONDS_PER_MINUTE) + s;

    LogExcess(VB_SCHEDULE, "%s on %0d/%d/%d is at %02d:%02d:%02d\n", info.c_str(),
              local.tm_mon + 1, local.tm_mday, local.tm_year + 1900, h, m, s);
}

int ScheduleEntry::LoadFromJson(Json::Value& entry) {
    enabled = entry["enabled"].asInt();
    playlist = entry["playlist"].asString();
    dayIndex = entry["day"].asInt();
    repeatInterval = entry["repeat"].asInt();
    if (repeatInterval < 0) {
        // The UI only offers values from a dropdown, but schedule.json is
        // hand- and API-editable.  A negative interval walks startTime
        // backwards in both occurrence-expansion loops, so they never reach
        // endTime and spin forever appending pairs until the box is out of
        // memory - on the scheduler thread or the calendar API thread.
        LogErr(VB_SCHEDULE, "Negative repeat interval %d for playlist %s, treating as no repeat\n",
               repeatInterval, playlist.c_str());
        WarningHolder::AddWarning(47, "Schedule: negative repeat interval for playlist '" + playlist + "'");
        repeatInterval = 0;
    }
    repeat = repeatInterval == 1;

    if (entry.isMember("sequence"))
        sequence = entry["sequence"].asBool();

    if (entry.isMember("command"))
        command = entry["command"].asString();

    if (entry.isMember("args"))
        args = entry["args"];

    if (entry.isMember("multisyncCommand"))
        multisyncCommand = (bool)entry["multisyncCommand"].asInt();
    else
        multisyncCommand = false;

    if (entry.isMember("multisyncCommand"))
        multisyncHosts = entry["multisyncHosts"].asString();
    else
        multisyncHosts = "";

    repeatInterval /= 100;
    repeatInterval *= 60; //seconds betweeen intervals

    if (!entry.isMember("startTime") || (entry["startTime"].asString() == "")) {
        LogErr(VB_SCHEDULE, "Missing or invalid startTime for playlist %s\n",
               playlist.c_str());
        WarningHolder::AddWarning(47, "Schedule: missing or invalid start time for playlist '" + playlist + "'");
        return 0;
    }
    startTimeStr = entry["startTime"].asString();
    std::size_t found = startTimeStr.find(":");
    if (found != std::string::npos)
        mapTimeString(startTimeStr, startHour, startMinute, startSecond);

    if (!entry.isMember("endTime") || (entry["endTime"].asString() == "")) {
        LogErr(VB_SCHEDULE, "Missing or invalid endTime for playlist %s\n",
               playlist.c_str());
        WarningHolder::AddWarning(47, "Schedule: missing or invalid end time for playlist '" + playlist + "'");
        return 0;
    }
    endTimeStr = entry["endTime"].asString();
    found = endTimeStr.find(":");
    if (found != std::string::npos)
        mapTimeString(endTimeStr, endHour, endMinute, endSecond);

    if (entry.isMember("startTimeOffset")) {
        if (entry["startTimeOffset"].isString()) {
            startTimeOffset = atoi(entry["startTimeOffset"].asString().c_str());
        } else {
            startTimeOffset = entry["startTimeOffset"].asLargestInt();
        }
    }

    if (entry.isMember("endTimeOffset")) {
        if (entry["endTimeOffset"].isString()) {
            endTimeOffset = atoi(entry["endTimeOffset"].asString().c_str());
        } else {
            endTimeOffset = entry["endTimeOffset"].asLargestInt();
        }
    }

    if (!entry.isMember("startDate") || (entry["startDate"].asString() == "")) {
        entry["startDate"] = "2019-01-01";
    }
    startDateStr = entry["startDate"].asString();
    std::string tempStr = CheckHoliday(startDateStr);

    if (tempStr.length() == 10) {
        startDate = DateStrToInt(tempStr.c_str());
    } else {
        startDate = 20190101;
        startDateStr = "20190101";
    }

    if (!entry.isMember("endDate") || (entry["endDate"].asString() == "")) {
        entry["endDate"] = "2099-12-31";
    }
    endDateStr = entry["endDate"].asString();
    tempStr = CheckHoliday(endDateStr);

    if (tempStr.length() == 10) {
        endDate = DateStrToInt(tempStr.c_str());
    } else {
        endDate = 20991231;
        endDateStr = "20991231";
    }

    stopType = entry["stopType"].asInt();

    return 1;
}

Json::Value ScheduleEntry::GetJson(void) {
    Json::Value e;
    char timeText[9];

    e["enabled"] = (int)enabled;
    e["playlist"] = playlist;
    if (playlist == "") {
        e["command"] = command;
        e["args"] = args;
        e["multisyncCommand"] = multisyncCommand;
        e["multisyncHosts"] = multisyncHosts;
    }

    e["day"] = dayIndex;

    snprintf(timeText, sizeof(timeText), "%02d:%02d:%02d", startHour, startMinute, startSecond);
    e["startTime"] = timeText;
    e["startTimeStr"] = startTimeStr;
    e["startTimeOffset"] = startTimeOffset;

    snprintf(timeText, sizeof(timeText), "%02d:%02d:%02d", endHour, endMinute, endSecond);
    e["endTime"] = timeText;
    e["endTimeStr"] = endTimeStr;
    e["endTimeOffset"] = endTimeOffset;

    e["repeat"] = (int)repeat;
    e["repeatInterval"] = (int)repeatInterval;
    e["startDate"] = startDateStr;
    e["startDateInt"] = startDate;
    e["endDate"] = endDateStr;
    e["endDateInt"] = endDate;
    e["stopType"] = stopType;
    e["stopTypeStr"] =
        stopType == 2 ? "Graceful Loop" : stopType == 1 ? "Hard"
                                                        : "Graceful";

    return e;
}
