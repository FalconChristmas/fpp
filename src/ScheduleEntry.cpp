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

#include <cerrno>
#include <cmath>
#include <ctime>
#include <ctype.h>
#include <math.h>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <utility>
#include <vector>

#include "FPPLocale.h"
#include "SunRise.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"

#include "ScheduleEntry.h"

static std::mutex s_loadWarningsLock;
static std::vector<std::pair<int, std::string>> s_loadWarnings;

// The innermost SchedulePreviewScope on this thread, if any.
static thread_local SchedulePreviewScope* s_previewScope = nullptr;

SchedulePreviewScope::SchedulePreviewScope() :
    m_prev(s_previewScope) {
    s_previewScope = this;
}

SchedulePreviewScope::~SchedulePreviewScope() {
    s_previewScope = m_prev;
}

void ScheduleLoadWarning(int id, const std::string& msg) {
    if (s_previewScope) {
        // A preview expansion (the calendar) is not a load: nothing here
        // would ever retract a banner it raised, and a year the viewer merely
        // browsed to is not one the scheduler will run.
        LogDebug(VB_SCHEDULE, "Schedule preview: %s\n", msg.c_str());
        for (auto& w : s_previewScope->m_warnings) {
            if ((w.first == id) && (w.second == msg))
                return;
        }
        s_previewScope->m_warnings.emplace_back(id, msg);
        return;
    }
    WarningHolder::AddWarning(id, msg);
    std::lock_guard<std::mutex> l(s_loadWarningsLock);
    for (auto& w : s_loadWarnings) {
        if ((w.first == id) && (w.second == msg))
            return;
    }
    s_loadWarnings.emplace_back(id, msg);
}

void ClearScheduleLoadWarnings() {
    std::vector<std::pair<int, std::string>> old;
    {
        std::lock_guard<std::mutex> l(s_loadWarningsLock);
        old.swap(s_loadWarnings);
    }
    for (auto& w : old) {
        WarningHolder::RemoveWarning(w.first, w.second);
    }
}

// Days since 1970-01-01 for a proleptic Gregorian civil date (Howard
// Hinnant's days_from_civil).  Pure calendar arithmetic: no time zone, no DST.
static long DaysFromCivil(long y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long)doe - 719468;
}

// The inverse of DaysFromCivil (Hinnant's civil_from_days).
static void CivilFromDays(long z, int& y, int& m, int& d) {
    z += 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    y = (int)(yoe + era * 400 + (m <= 2));
}

// 'base' is the schedule load's single "now".  Reading the clock here, per
// call, let a load that ran across midnight anchor an occurrence's start to
// one week and its end to the next (Saturday->Sunday): a week-long window.
static time_t GetTimeOnDOW(time_t base, int day, int hour, int minute, int second) {
    struct tm now;
    localtime_r(&base, &now);

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
    startDateOffset(0),
    endDateOffset(0),
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
    
    // Not in the table: say so rather than silently scheduling on a made-up
    // date.  The table needs extending for later years.
    ScheduleLoadWarning(47, "Schedule: no Chinese New Year date known for " + std::to_string(year) + "; FPP's table needs updating");
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
    
    // Not in the table (or not a date it covers): say so rather than silently
    // scheduling on Jan 1.
    ScheduleLoadWarning(47, "Schedule: no Hebrew calendar date known for " + std::to_string(year) + "; FPP's table needs updating");
    month = 1;
    day = 1;
}

void ScheduleEntry::CalculateIslamicDate(int year, int islamicMonth, int islamicDay, int& month, int& day) {
    // Simplified Islamic calendar conversion
    // Islamic calendar moves ~11 days earlier each year
    
    // Ramadan (month 9) approximate start dates for 2024-2035.  The Islamic
    // year is ~354 days, so in 2030 Ramadan starts twice (January 6 and
    // December 26); the table holds the first.  2031-2035 used to hold the
    // previous Gregorian year's second date (Ramadan 2032 on December 15
    // rather than the 4th); these are the tabular Islamic calendar's dates,
    // which observation can move by a day.
    int ramadanDates[][3] = {
        {2024, 3, 11}, {2025, 3, 1}, {2026, 2, 18}, {2027, 2, 8},
        {2028, 1, 28}, {2029, 1, 17}, {2030, 1, 6}, {2031, 12, 15},
        {2032, 12, 4}, {2033, 11, 23}, {2034, 11, 12}, {2035, 11, 2}
    };
    
    // Eid al-Fitr (month 10, day 1) - approximately 30 days after Ramadan.
    // 2033 has two (January 3 and December 23); the table holds the first.
    // 2034 and 2035 used to hold the previous year's second date.
    int eidFitrDates[][3] = {
        {2024, 4, 10}, {2025, 3, 31}, {2026, 3, 20}, {2027, 3, 10},
        {2028, 2, 27}, {2029, 2, 16}, {2030, 2, 5}, {2031, 1, 25},
        {2032, 1, 14}, {2033, 1, 3}, {2034, 12, 12}, {2035, 12, 2}
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
    
    // Not in the table (or not a date it covers): say so rather than silently
    // scheduling on Jan 1.
    ScheduleLoadWarning(47, "Schedule: no Islamic calendar date known for " + std::to_string(year) + "; FPP's table needs updating");
    month = 1;
    day = 1;
}

void ScheduleEntry::CalculateHinduDiwali(int year, int& month, int& day) {
    // Diwali falls on the new moon of Kartika (October/November)
    // Pre-calculated dates for 2024-2035
    int diwaliDates[][3] = {
        {2024, 11, 1}, {2025, 10, 21}, {2026, 11, 9}, {2027, 10, 29},
        {2028, 10, 17}, {2029, 11, 5}, {2030, 10, 26}, {2031, 11, 14},
        {2032, 11, 2}, {2033, 10, 23}, {2034, 11, 10}, {2035, 10, 30}
    };
    
    for (int i = 0; i < 12; i++) {
        if (diwaliDates[i][0] == year) {
            month = diwaliDates[i][1];
            day = diwaliDates[i][2];
            return;
        }
    }
    
    // Not in the table: say so rather than silently scheduling on Nov 1.
    ScheduleLoadWarning(47, "Schedule: no Diwali date known for " + std::to_string(year) + "; FPP's table needs updating");
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
                ScheduleLoadWarning(47, "Schedule: invalid month in holiday definition");
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
                // Calendar arithmetic, no time zone.  This used to build UTC
                // midnight of the day after and convert with localtime(),
                // which is right west of UTC and a day late east of it (UK
                // Mothering Sunday, Easter - 21, came out a Monday).
                int y = year;
                CivilFromDays(DaysFromCivil(year, month, day) + offset, y, month, day);
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
                ScheduleLoadWarning(47, "Schedule: unknown lunar calendar type '" + calendar + "'");
                return "0000-00-00";
            }
            
            // Check if the calculated date has passed this year
            if (rollForward &&
                ((month < (now.tm_mon + 1)) ||
                 (month == (now.tm_mon + 1) && day < now.tm_mday))) {
                int thisMonth = month;
                int thisDay = day;
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
                // Yearless, next year's date matches this year too if it is
                // still to come: keep this year's, which has passed. The daily
                // reload picks up next year's.
                if ((month > (now.tm_mon + 1)) || (month == (now.tm_mon + 1) && day >= now.tm_mday)) {
                    month = thisMonth;
                    day = thisDay;
                }
            }
        } else {
            LogErr(VB_SCHEDULE, "Unknown holiday calculation type %s\n", type.c_str());
            ScheduleLoadWarning(47, "Schedule: unknown holiday calculation type '" + type + "'");
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

/*
 * Shift a YYYY-MM-DD date by a number of days.  Used to schedule relative to
 * a floating holiday ("7 days before Thanksgiving") the same way
 * startTimeOffset shifts a sunrise/sunset time.
 *
 * Holidays resolve to a yearless "0000-MM-DD" (see DateFromLocaleHoliday) and
 * DateInRange() treats MMDD values as wrapping across Dec 31/Jan 1, so the
 * arithmetic is done in a real year (refYear, else the current year, so leap
 * days land where they should) and the result is stamped back to year 0.
 * Anything DateStrToInt() rejects - an unresolved holiday name or the
 * "0000-00-00" error sentinel - is passed through untouched.
 */
static std::string ApplyDateOffset(const std::string& date, int offsetDays, int refYear) {
    if ((offsetDays == 0) || (date.length() != 10) || (DateStrToInt(date.c_str()) == 0))
        return date;

    int year = atoi(date.c_str());
    int calcYear = year;
    if (calcYear == 0) {
        if (refYear != 0) {
            calcYear = refYear;
        } else {
            time_t currTime = time(NULL);
            struct tm now;
            localtime_r(&currTime, &now);
            calcYear = now.tm_year + 1900;
        }
    }

    struct tm t = {};
    t.tm_year = calcYear - 1900;
    t.tm_mon = atoi(date.c_str() + 5) - 1;
    t.tm_mday = atoi(date.c_str() + 8) + offsetDays;
    if (timegm(&t) == (time_t)-1)
        return date;

    char str[11];
    snprintf(str, sizeof(str), "%04d-%02d-%02d", year == 0 ? 0 : t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return str;
}

/*
 * Turn a schedule start/end date string (literal YYYY-MM-DD or holiday name)
 * into a YYYY-MM-DD string.  The day offset only applies to holiday names.
 *
 * With no refYear, DateFromLocaleHoliday rolls a floating holiday forward to
 * next year once this year's has passed.  That is fine for the bare holiday
 * but wrong once an offset extends the range past it - "Diwali +7" reloaded
 * the day after Diwali would anchor on next year's date and drop the rest of
 * the window.  So with an offset we resolve explicitly in the current year,
 * apply the offset, and only roll to next year if the *offset* date has passed.
 */
std::string ScheduleEntry::ResolveDate(const std::string& date, int offsetDays, int refYear) {
    if (date.empty() || isdigit((unsigned char)date[0]))
        return date;

    if ((refYear != 0) || (offsetDays == 0))
        return ApplyDateOffset(CheckHoliday(date, refYear), offsetDays, refYear);

    time_t currTime = time(NULL);
    struct tm now;
    localtime_r(&currTime, &now);
    int year = now.tm_year + 1900;
    int today = ((now.tm_mon + 1) * 100) + now.tm_mday;

    std::string resolved = ApplyDateOffset(CheckHoliday(date, year), offsetDays, year);
    if (resolved.length() == 10) {
        int mmdd = DateStrToInt(resolved.c_str());
        // yearless (< 10000) and already behind us this year
        if ((mmdd > 0) && (mmdd < 10000) && (mmdd < today))
            resolved = ApplyDateOffset(CheckHoliday(date, year + 1), offsetDays, year + 1);
    }
    return resolved;
}

static int WallSeconds(int h, int m, int s) {
    return (h * SECONDS_PER_HOUR) + (m * SECONDS_PER_MINUTE) + s;
}

static void mapTimeString(const std::string& tm, int& h, int& m, int& s) {
    if (!ParseTimeString(tm, h, m, s)) {
        LogErr(VB_SCHEDULE, "Invalid time '%s' in schedule, using midnight\n", tm.c_str());
        ScheduleLoadWarning(47, "Schedule: invalid time '" + tm + "'");
        h = 0;
        m = 0;
        s = 0;
    }
}

void ScheduleEntry::pushStartEndTimes(int dow, int& delta, time_t deltaThreshold, time_t now) {
    // Local copies, as in GetOccurrencesOnDate(): GetTimeFromSun() writes the
    // day's resolved time into them, and writing it into the entry's own
    // fields made each day start from the previous day's sun time.
    int sh = startHour, sm = startMinute, ss = startSecond;
    int eh = endHour, em = endMinute, es = endSecond;
    time_t startTime = GetTimeOnDOW(now, dow, sh, sm, ss);
    time_t endTime = GetTimeOnDOW(now, dow, eh, em, es);

    if (startTimeStr.find(":") == std::string::npos) {
        GetTimeFromSun(startTime, startTimeStr, startTimeOffset, sh, sm, ss);
    }
    if (endTimeStr.find(":") == std::string::npos) {
        GetTimeFromSun(endTime, endTimeStr, endTimeOffset, eh, em, es);
    }
    // Decide "runs past midnight" from the wall-clock times, not the epoch
    // times: on a DST change day those differ by an hour, and a 02:30 start
    // on the spring-forward day normalizes to 03:30, past a 03:00 end, which
    // turned a half-hour entry into a 23.5 hour one.  And build the next-day
    // end with mktime rather than adding 86400, which is an hour off across
    // the change.
    // A sun-relative time can itself fall after midnight, so with one of
    // those compare the actual times; the wall-clock test is for fixed times.
    bool sunTimes = (startTimeStr.find(":") == std::string::npos) || (endTimeStr.find(":") == std::string::npos);
    bool crossesMidnight = sunTimes ? (endTime < startTime)
                                    : (WallSeconds(eh, em, es) < WallSeconds(sh, sm, ss));
    if (crossesMidnight) {
        endTime = GetTimeOnDOW(now, dow + 1, eh, em, es);
        if (endTimeStr.find(":") == std::string::npos) {
            GetTimeFromSun(endTime, endTimeStr, endTimeOffset, eh, em, es);
        }
    } else if (endTime < startTime) {
        // Same-day window squeezed by the spring-forward gap.
        endTime = startTime;
    }

    // A command occurrence has no duration (the UI saves endTime = startTime),
    // so once its second passed it used to be dropped here -- by any reload in
    // the same second or a few seconds later, which is exactly the stall or
    // clock step the scheduler's command catch-up exists for.  Keep commands
    // through the catch-up window; the scheduler's check window stops them
    // firing twice.
    time_t cutoff = playlist.empty() ? now - SCHEDULE_COMMAND_CATCHUP_S : now;
    if (endTime < cutoff)
        return;

    if (!DateInRange(startTime, startDate, endDate)) {
        return;
    }

    if (delta != 0) {
        // Only adjust if the threshold and start times are in the future
        if (deltaThreshold > now) {
            if (deltaThreshold > startTime && startTime > now) {
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

    // Occurrences end where the next one starts, half-open like every endTime
    // check.  Ending a second early let a lower entry start in the gap (#3141).
    if ((repeatInterval > 0) && (startTime != endTime)) {
        time_t horizon = now + SCHEDULE_REPEAT_HORIZON_S;
        time_t newEnd = startTime + repeatInterval;
        while ((startTime < endTime) && (startTime < horizon)) {
            if (newEnd >= cutoff) {
                startEndTimes.emplace_back(startTime, newEnd);
            }
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

    std::string resolved = ResolveDate(startDateStr, startDateOffset, year);
    if (resolved.length() == 10)
        sDate = DateStrToInt(resolved.c_str());

    resolved = ResolveDate(endDateStr, endDateOffset, year);
    if (resolved.length() == 10)
        eDate = DateStrToInt(resolved.c_str());
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
    // the FPP repository on github, July 15, 2013.  Count calendar days, not
    // seconds: the epoch used to be built with tm_isdst=0, an hour off during
    // DST, so from 00:00 to 00:59 in summer -- exactly when the midnight
    // schedule reload runs -- the count came out a day short and odd entries
    // were scheduled on even days.
    struct tm local;
    localtime_r(&dayTime, &local);
    long daysSince = DaysFromCivil(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday) -
                     DaysFromCivil(2013, 7, 15);

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

    int sh = startHour, sm = startMinute, ss = startSecond;
    int eh = endHour, em = endMinute, es = endSecond;
    if (startTimeStr.find(":") == std::string::npos) {
        GetTimeFromSun(startTime, startTimeStr, startTimeOffset, sh, sm, ss);
    }
    if (endTimeStr.find(":") == std::string::npos) {
        GetTimeFromSun(endTime, endTimeStr, endTimeOffset, eh, em, es);
    }
    // As in pushStartEndTimes(): wall-clock crossover test, next-day end via
    // mktime.
    bool sunTimes = (startTimeStr.find(":") == std::string::npos) || (endTimeStr.find(":") == std::string::npos);
    bool crossesMidnight = sunTimes ? (endTime < startTime) : (WallSeconds(eh, em, es) < WallSeconds(sh, sm, ss));
    if (crossesMidnight) {
        endTime = GetTimeOnDate(year, month, mday + 1, eh, em, es);
        if (endTimeStr.find(":") == std::string::npos) {
            GetTimeFromSun(endTime, endTimeStr, endTimeOffset, eh, em, es);
        }
    } else if (endTime < startTime) {
        endTime = startTime;
    }

    int sDate = 0;
    int eDate = 0;
    GetDateRangeForYear(year, sDate, eDate);

    if (!DateInRange(startTime, sDate, eDate))
        return;

    // Occurrences end where the next starts, as in pushStartEndTimes().
    if ((repeatInterval > 0) && (startTime != endTime)) {
        time_t newEnd = startTime + repeatInterval;
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
// string, so "38.9N" is rejected rather than silently becoming 38.9.  It also
// has to be a real angle within +-limit: strtod() happily accepts "nan",
// "inf" and 1e300, which went on into the sun calculation as garbage instead
// of raising the "not usable" warning.
static bool ParseCoordinate(const std::string& str, double limit, double& out) {
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

    if (!std::isfinite(v) || (std::fabs(v) > limit)) {
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
    if (!ParseCoordinate(latStr, 90.0, lat) || !ParseCoordinate(lonStr, 180.0, lon)) {
        lat = 38.938524;
        lon = -104.600945;

        LogErr(VB_SCHEDULE, "Error, Latitude/Longitude not usable ('%s', '%s'), using Falcon, Colorado coordinates!\n",
               latStr.c_str(), lonStr.c_str());
        // A load warning, so the next load retracts it: removing it only here,
        // when sun times are next computed with good coordinates, left it up
        // for good once the last sun-relative entry was deleted instead.  A
        // Latitude/Longitude change reloads the schedule (see Scheduler()).
        ScheduleLoadWarning(48, LATLON_WARNING);
    }
    time_t currTime = when;
    struct tm local;
    struct tm mtm;

    localtime_r(&currTime, &local);
    localtime_r(&currTime, &mtm);

    LogExcess(VB_SCHEDULE, "Lat/Lon: %.6f, %.6f\n", lat, lon);
    LogExcess(VB_SCHEDULE, "Lookup Date is %02d/%02d/%04d\n",
              local.tm_mon + 1, local.tm_mday, local.tm_year + 1900);

    // Ask about local noon of the day. SunRise finds the events within 12
    // hours either side of the time it is given, so asked from a time near
    // midnight it returned the other day's sunset or sunrise -- and the time
    // passed in here can be any time on the day.
    struct tm noonTm = local;
    noonTm.tm_hour = 12;
    noonTm.tm_min = 0;
    noonTm.tm_sec = 0;
    noonTm.tm_isdst = -1;
    time_t noon = mktime(&noonTm);

    SunRise sr;
    if ((info == "Dawn") || (info == "Dusk"))
        sr.calculate(lat, lon, noon, TT_CIVIL);
    else
        sr.calculate(lat, lon, noon);

    // Find midnight on the 'when' day.  tm_isdst = -1: on a DST change day
    // midnight and 'when' are on different sides of the change.
    mtm.tm_hour = 0;
    mtm.tm_min = 0;
    mtm.tm_sec = 0;
    mtm.tm_isdst = -1;
    time_t midnight = mktime(&mtm);

    const bool isRise = (info == "SunRise") || (info == "Dawn");
    const bool isSet = (info == "SunSet") || (info == "Dusk");

    // Far enough north or south the sun can stay up (or down) all day, and
    // then there is no rise or set to anchor to.  SunRise reports that as
    // hasRise/hasSet false with the time left at 0, which the clamp below
    // read as "before the day starts" and put a SunSet show at 8AM.  Use the
    // same conventional times the clamp falls back to -- 8AM for a rise,
    // 8PM for a set -- with the entry's offset still applied, so entries
    // keep their order and a dusk show stays in the evening.
    if ((isRise && !sr.hasRise) || (isSet && !sr.hasSet)) {
        struct tm ftm = local;
        ftm.tm_hour = isRise ? 8 : 20;
        ftm.tm_min = 0;
        ftm.tm_sec = 0;
        ftm.tm_isdst = -1;
        time_t target = mktime(&ftm) + (time_t)infoOffset * SECONDS_PER_MINUTE;
        struct tm tt;
        localtime_r(&target, &tt);
        h = tt.tm_hour;
        m = tt.tm_min;
        s = tt.tm_sec;
        when = target;
        LogWarn(VB_SCHEDULE, "No %s on %02d/%02d/%04d at %.6f, %.6f (sun %s all day), using %02d:%02d:%02d\n",
                info.c_str(), local.tm_mon + 1, local.tm_mday, local.tm_year + 1900, lat, lon,
                sr.isVisible ? "up" : "down", h, m, s);
        return;
    }

    time_t event = midnight;
    if (isRise)
        event = sr.riseTime;
    else if (isSet)
        event = sr.setTime;

    // Work from the absolute event time and let localtime() give the wall
    // clock.  This used to add "seconds since midnight" to the wall-clock
    // midnight, which is an hour off on a DST change day: a 17:00 sunset on
    // the fall-back day was scheduled at 18:00.
    time_t target = event + (time_t)infoOffset * SECONDS_PER_MINUTE;
    struct tm tt;
    localtime_r(&target, &tt);
    struct tm et;
    localtime_r(&event, &et);

    // Clamp only when the sun event itself is not on this day, which means
    // the time zone or location is wrong.  An offset may legitimately carry
    // the time past midnight (SunSet + 4 hours far north in summer); that used
    // to be clamped to 8PM, hours from what was asked for.
    if ((et.tm_year != local.tm_year) || (et.tm_yday != local.tm_yday)) {
        if (event < midnight) {
            LogWarn(VB_SCHEDULE, "%s with offset %d falls before the day starts, using 8AM. "
                                 "Check your time zone to make sure it is valid.\n",
                    info.c_str(), infoOffset);
            h = 8;
        } else {
            LogWarn(VB_SCHEDULE, "%s with offset %d falls after the day ends, using 8PM. "
                                 "Check your time zone to make sure it is valid.\n",
                    info.c_str(), infoOffset);
            h = 20;
        }
        m = 0;
        s = 0;

        // FIXME, move the following to Excess once the issues are worked out with SunRise class
        LogWarn(VB_SCHEDULE, "w: %lld, m: %lld, rt: %lld, st: %lld\n",
                (long long)when, (long long)midnight, (long long)sr.riseTime, (long long)sr.setTime);
        LogWarn(VB_SCHEDULE, "info: %s, infoOffset: %d, lat: %.6f, lon: %.6f, date: %02d/%02d/%04d\n",
                info.c_str(), infoOffset, lat, lon,
                local.tm_mon + 1, local.tm_mday, local.tm_year + 1900);

        mtm.tm_hour = h;
        mtm.tm_min = 0;
        mtm.tm_sec = 0;
        mtm.tm_isdst = -1;
        when = mktime(&mtm);
        return;
    }

    h = tt.tm_hour;
    m = tt.tm_min;
    s = tt.tm_sec;
    when = target;

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
        ScheduleLoadWarning(47, "Schedule: negative repeat interval for playlist '" + playlist + "'");
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
        ScheduleLoadWarning(47, "Schedule: missing or invalid start time for playlist '" + playlist + "'");
        return 0;
    }
    startTimeStr = entry["startTime"].asString();
    std::size_t found = startTimeStr.find(":");
    if (found != std::string::npos)
        mapTimeString(startTimeStr, startHour, startMinute, startSecond);

    if (!entry.isMember("endTime") || (entry["endTime"].asString() == "")) {
        LogErr(VB_SCHEDULE, "Missing or invalid endTime for playlist %s\n",
               playlist.c_str());
        ScheduleLoadWarning(47, "Schedule: missing or invalid end time for playlist '" + playlist + "'");
        return 0;
    }
    endTimeStr = entry["endTime"].asString();
    found = endTimeStr.find(":");
    if (found != std::string::npos)
        mapTimeString(endTimeStr, endHour, endMinute, endSecond);

    // The UI limits these offsets (date offsets to a year, sun-relative time
    // offsets to two hours); keep API/hand-edited files honest too, checking
    // before the int64 -> int narrowing so huge values can't wrap into range.
    // An unbounded time offset put an occurrence decades away, and the repeat
    // expansion then stepped from there: billions of iterations on the main
    // loop at every schedule load.
    auto offsetFromJson = [&entry](const char* key, int limit, const char* unit) -> int {
        if (!entry.isMember(key))
            return 0;
        const Json::Value& e = entry[key];
        Json::Value::LargestInt v;
        if (e.isNull()) {
            return 0;
        } else if (e.isString()) {
            v = atoll(e.asString().c_str());
        } else if (e.isIntegral()) {
            // asLargestInt() throws on an object, array or non-integral
            // double; a bad value in schedule.json must not abort fppd.
            v = e.asLargestInt();
        } else {
            LogErr(VB_SCHEDULE, "Invalid %s in schedule (not a number), using 0\n", key);
            ScheduleLoadWarning(47, std::string("Schedule: ") + key + " is not a number");
            return 0;
        }
        if ((v < -limit) || (v > limit)) {
            LogErr(VB_SCHEDULE, "Invalid %s %lld in schedule, using 0\n", key, (long long)v);
            ScheduleLoadWarning(47, std::string("Schedule: ") + key + " out of range (-" + std::to_string(limit) +
                                        ".." + std::to_string(limit) + " " + unit + ")");
            return 0;
        }
        return (int)v;
    };
    startTimeOffset = offsetFromJson("startTimeOffset", 1440, "minutes");
    endTimeOffset = offsetFromJson("endTimeOffset", 1440, "minutes");
    startDateOffset = offsetFromJson("startDateOffset", 366, "days");
    endDateOffset = offsetFromJson("endDateOffset", 366, "days");

    if (!entry.isMember("startDate") || (entry["startDate"].asString() == "")) {
        entry["startDate"] = "2019-01-01";
    }
    startDateStr = entry["startDate"].asString();
    std::string tempStr = ResolveDate(startDateStr, startDateOffset);

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
    tempStr = ResolveDate(endDateStr, endDateOffset);

    if (tempStr.length() == 10) {
        endDate = DateStrToInt(tempStr.c_str());
    } else {
        endDate = 20991231;
        endDateStr = "20991231";
    }

    stopType = entry["stopType"].asInt();
    if ((stopType < 0) || (stopType > 2)) {
        // 0 graceful, 1 hard, 2 graceful after loop.  Anything else indexed
        // past SchedulePrint's table; fall back to the default.
        LogErr(VB_SCHEDULE, "Invalid stop type %d for playlist %s, using graceful stop\n",
               stopType, playlist.c_str());
        stopType = 0;
    }

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
    e["startDateOffset"] = startDateOffset;
    e["startDateInt"] = startDate;
    e["endDate"] = endDateStr;
    e["endDateOffset"] = endDateOffset;
    e["endDateInt"] = endDate;
    e["stopType"] = stopType;
    e["stopTypeStr"] =
        stopType == 2 ? "Graceful Loop" : stopType == 1 ? "Hard"
                                                        : "Graceful";

    return e;
}
