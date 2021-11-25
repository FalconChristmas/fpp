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

#include "fpp-pch.h"

#include "FPPLocale.h"
#include "ScheduleEntry.h"
#include "sunset.h"
#include <math.h>

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

ScheduleEntry::ScheduleEntry() :
    enabled(false),
    playlist(""),
    sequence(false),
    repeat(false),
    dayIndex(0),
    startHour(0),
    startMinute(0),
    startSecond(0),
    endHour(0),
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

std::string ScheduleEntry::DateFromLocaleHoliday(Json::Value& holiday) {
    int year = 0;
    int month = 0;
    int day = 0;

    if (holiday.isMember("calc")) {
        // We need to calculate the month/day of the holiday since it changes yearly
        Json::Value c = holiday["calc"];
        std::string type = c["type"].asString();
        int week = c["week"].asInt();
        int dow = c["dow"].asInt();
        int offset = c["offset"].asInt();
        time_t currTime = time(NULL);
        struct tm now;
        localtime_r(&currTime, &now);

        year = now.tm_year + 1900;

        if ((type == "head") || (type == "tail")) {
            month = c["month"].asInt();
            if (month < (now.tm_mon + 1))
                year++;
        }

        // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
        int mdays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
        int d = 1;
        int m = month;
        int y = year - (m < 3);
        int wday = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;

        if ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)))
            mdays[2]++; // add one day to February for leap year

        if (type == "easter") {
            CalculateEaster(year, month, day);

            // Check if Easter was more than 4 months ago.  We do this since some Easter-related
            // special days are 2 months after Easter so we want to calculate those based on the
            // current year.
            if ((month + 3) < now.tm_mon) {
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
            if (dow == wday)
                day += 7;

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
        } else {
            LogErr(VB_SCHEDULE, "Unknown holiday calculation type %s\n", type.c_str());
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

std::string ScheduleEntry::CheckHoliday(std::string date) {
    Json::Value locale = LocaleHolder::GetLocale();
    std::string result;

    if (!locale.isMember("holidays"))
        return date;

    if (isdigit(date.c_str()[0]))
        return date;

    for (int i = 0; i < locale["holidays"].size(); i++) {
        if (date == locale["holidays"][i]["shortName"].asString())
            return DateFromLocaleHoliday(locale["holidays"][i]);
    }

    return date;
}

int ScheduleEntry::LoadFromString(std::string entryStr) {
    std::vector<std::string> elems = split(entryStr, ',');

    if (elems.size() < 10) {
        LogErr(VB_SCHEDULE, "Invalid Schedule Entry: '%s', %d elements\n",
               entryStr.c_str(), elems.size());
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
    std::vector<std::string> sparts = split(tm, ':');
    h = atoi(sparts[0].c_str());
    m = atoi(sparts[1].c_str());
    s = atoi(sparts[2].c_str());
}

void ScheduleEntry::pushStartEndTimes(int dow) {
    time_t startTime = GetTimeOnDOW(dow, startHour, startMinute, startSecond);
    time_t endTime = GetTimeOnDOW(dow, endHour, endMinute, endSecond);

    if (startTimeStr.find(":") == std::string::npos) {
        GetTimeFromSun(startTime, true);
    }
    if (endTimeStr.find(":") == std::string::npos) {
        GetTimeFromSun(endTime, false);
    }
    if (endTime < startTime) {
        endTime += 24 * 60 * 60;
        if (endTimeStr.find(":") == std::string::npos) {
            GetTimeFromSun(endTime, false);
        }
    }

    if (endTime < time(NULL))
        return;

    if (!DateInRange(startTime, startDate, endDate)) {
        return;
    }

    if ((repeatInterval) && (startTime != endTime)) {
        time_t newEnd = startTime + repeatInterval - 1;
        while (startTime < endTime) {
            startEndTimes.push_back(std::pair<int, int>(startTime, newEnd));
            newEnd += repeatInterval;
            if (newEnd > endTime) {
                newEnd = endTime;
            }
            startTime += repeatInterval;
        }
    } else {
        startEndTimes.push_back(std::pair<int, int>(startTime, endTime));
    }
}

void ScheduleEntry::GetTimeFromSun(time_t& when, bool setStart) {
    time_t origWhen = when;
    std::string latStr = getSetting("Latitude");
    std::string lonStr = getSetting("Longitude");

    if ((latStr == "") || (lonStr == "")) {
        latStr = "38.938524";
        lonStr = "-104.600945";

        LogErr(VB_SCHEDULE, "Error, Latitude/Longitude not filled in, using Falcon, Colorado coordinates!\n");
    }

    std::string::size_type sz;
    double lat = std::stod(latStr, &sz);
    double lon = std::stod(lonStr, &sz);
    double sunOffset = 0;
    time_t currTime = when;
    struct tm utc;
    struct tm local;
    std::string info = setStart ? startTimeStr : endTimeStr;

    gmtime_r(&currTime, &utc);
    localtime_r(&currTime, &local);

    LogExcess(VB_SCHEDULE, "Lat/Lon: %.6f, %.6f\n", lat, lon);
    LogExcess(VB_SCHEDULE, "Today (UTC) is %02d/%02d/%04d, UTC offset is %d hours\n",
              utc.tm_mon + 1, utc.tm_mday, utc.tm_year + 1900, local.tm_gmtoff / 3600);

    SunSet sun(lat, lon, (int)(local.tm_gmtoff / 3600));
    sun.setCurrentDate(utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday);

    if (info == "SunRise")
        sunOffset = sun.calcSunrise();
    else if (info == "SunSet")
        sunOffset = sun.calcSunset();
    else if (info == "Dawn")
        sunOffset = sun.calcCivilSunrise();
    else if (info == "Dusk")
        sunOffset = sun.calcCivilSunset();
    else
        sunOffset = 0;

    if (setStart) {
        sunOffset += startTimeOffset;
    } else {
        sunOffset += endTimeOffset;
    }

    int daySecond = (local.tm_hour * SECONDS_PER_HOUR) + (local.tm_min * SECONDS_PER_MINUTE) + local.tm_sec;

    if (sunOffset < 0) {
        LogWarn(VB_SCHEDULE, "Sunrise calculated as before midnight last night, using 8AM.  Check your time zone to make sure it is valid.\n");
        if (setStart) {
            startHour = 8;
            startMinute = 0;
            startSecond = 0;
        } else {
            endHour = 8;
            endMinute = 0;
            endSecond = 0;
        }
        when = when - daySecond + (8 * SECONDS_PER_HOUR);
        return;
    } else if (sunOffset >= (24 * 60 * 60)) {
        LogWarn(VB_SCHEDULE, "Sunrise calculated as after midnight tomorrow, using 8PM.  Check your time zone to make sure it is valid.\n");
        if (setStart) {
            startHour = 20;
            startMinute = 0;
            startSecond = 0;
        } else {
            endHour = 20;
            endMinute = 0;
            endSecond = 0;
        }
        when = when - daySecond + (20 * SECONDS_PER_HOUR);
        return;
    }

    LogExcess(VB_SCHEDULE, "%s Time Offset: %.2f minutes\n", info.c_str(), sunOffset);
    int hour = (int)sunOffset / 60;
    int minute = (int)sunOffset % 60;
    int second = (int)(((int)(sunOffset * 100) % 100) * 0.01 * 60);

    when = when - daySecond + (hour * SECONDS_PER_HOUR) + (minute * SECONDS_PER_MINUTE) + second;

    LogExcess(VB_SCHEDULE, "%s is at %02d:%02d:%02d     %d\n", info.c_str(), hour, minute, second, when % 1000000);

    if (setStart) {
        startHour = hour;
        startMinute = minute;
        startSecond = second;
    } else {
        endHour = hour;
        endMinute = minute;
        endSecond = second;
    }
}

int ScheduleEntry::LoadFromJson(Json::Value& entry) {
    enabled = entry["enabled"].asInt();
    playlist = entry["playlist"].asString();
    dayIndex = entry["day"].asInt();
    repeatInterval = entry["repeat"].asInt();
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
        return 0;
    }
    startTimeStr = entry["startTime"].asString();
    std::size_t found = startTimeStr.find(":");
    if (found != std::string::npos)
        mapTimeString(startTimeStr, startHour, startMinute, startSecond);

    if (!entry.isMember("endTime") || (entry["endTime"].asString() == "")) {
        LogErr(VB_SCHEDULE, "Missing or invalid endTime for playlist %s\n",
               playlist.c_str());
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
        LogErr(VB_SCHEDULE, "Missing or invalid endTime for playlist %s\n",
               playlist.c_str());
        return 0;
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
        LogErr(VB_SCHEDULE, "Missing or invalid endTime for playlist %s\n",
               playlist.c_str());
        return 0;
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

    sprintf(timeText, "%02d:%02d:%02d", startHour, startMinute, startSecond);
    e["startTime"] = timeText;

    sprintf(timeText, "%02d:%02d:%02d", endHour, endMinute, endSecond);
    e["endTime"] = timeText;

    e["repeat"] = (int)repeat;
    e["repeatInterval"] = (int)repeatInterval;
    e["startDate"] = startDateStr;
    e["endDate"] = endDateStr;
    e["stopType"] = stopType;
    e["stopTypeStr"] =
        stopType == 2 ? "Graceful Loop" : stopType == 1 ? "Hard" : "Graceful";

    return e;
}
