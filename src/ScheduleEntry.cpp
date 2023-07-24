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

#include <ctime>
#include <ctype.h>
#include <fstream>
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

void ScheduleEntry::GetTimeFromSun(time_t& when, const std::string info,
                                   const int infoOffset,
                                   int& h, int& m, int& s) {
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

    snprintf(timeText, sizeof(timeText), "%02d:%02d:%02d", endHour, endMinute, endSecond);
    e["endTime"] = timeText;

    e["repeat"] = (int)repeat;
    e["repeatInterval"] = (int)repeatInterval;
    e["startDate"] = startDateStr;
    e["endDate"] = endDateStr;
    e["stopType"] = stopType;
    e["stopTypeStr"] =
        stopType == 2 ? "Graceful Loop" : stopType == 1 ? "Hard"
                                                        : "Graceful";

    return e;
}
