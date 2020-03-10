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

#include <math.h>
#include <stdlib.h>

#include <vector>

#include "common.h"
#include "FPPLocale.h"
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

void ScheduleEntry::CalculateEaster(int year, int &month, int &day)
{
    // Formula gleaned from various sources around the 'net.
    int Y,a,c,e,h,k,L;
    double b,d,f,g,i,m;

    Y = year;
    a = Y%19;
    b = floor(Y/100);
    c = Y%100;
    d = floor(b/4);
    e = (int)b%4;
    f = floor((b+8)/25);
    g = floor((b-f+1)/3);
    h = (19*a+(int)b-(int)d-(int)g+15)%30;
    i = floor(c/4);
    k = c%4;
    L = (32+2*e+2*(int)i-h-k)%7;
    m = floor((a+11*h+22*L)/451);
    month = (int)floor((h+L-7*m+114)/31);
    day = (int)((h+L-7*(int)m+114)%31)+1;
}

std::string ScheduleEntry::DateFromLocaleHoliday(Json::Value &holiday)
{
    int  year = 0;
    int  month = 0;
    int  day = 0;

    if (holiday.isMember("calc"))
    {
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

        if ((type == "head") || (type == "tail"))
        {
            month = c["month"].asInt();
            if (month < (now.tm_mon + 1))
                year++;
        }

        // https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week
        int mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        int d = 1;
        int m = month;
        int y = year - (m < 3);
        int wday = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;

        if ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)))
            mdays[2]++; // add one day to February for leap year

        if (type == "easter")
        {
            CalculateEaster(year, month, day);

            // Check if Easter was more than 4 months ago.  We do this since some Easter-related
            // special days are 2 months after Easter so we want to calculate those based on the
            // current year.
            if ((month + 3) < now.tm_mon)
            {
                year++;
                CalculateEaster(year, month, day);
            }

            // We have Easter, see if we are looking for something offset from Easter
            if (offset != 0)
            {
                time_t t = (365 * year) + (year / 4) - (year / 100) + (year / 400); // get years
                t += (30 * month) + (3 * (month + 1) / 5) + (day+1);                // add months and days
                t -= 719561;                                                        // convert to epoch
                t += offset;                                                        // apply our days offset
                t *= 86400;                                                         // multiple to seconds

                struct tm ts;
                localtime_r(&t, &ts);

                month = ts.tm_mon + 1;
                day = ts.tm_mday;
            }
        }
        else if (type == "head")
        {
            day = ((dow + 7 - wday) % 7) + ((week - 1) * 7) + 1;
            if (dow == wday)
                day += 7;

            day += offset;

            // See if offset pushed us into last month
            if (day < 1)
            {
                day -= offset;
                month--;
                if (month < 1)
                    month = 12;

                offset += day;
                day = mdays[month - 1] + offset;
            }
        }
        else if (type == "tail")
        {
            int ldow = wday + ((mdays[month - 1] - 1) % 7);
            day = mdays[month - 1] - ((ldow + 7 - dow) % 7) - ((week - 1) * 7);
            day += offset;
        }
        else
        {
            LogErr(VB_SCHEDULE, "Unknown holiday calculation type %s\n", type.c_str());
            return "0000-00-00";
        }

        year = 0;
    }
    else
    {
        year = holiday["year"].asInt();
        month = holiday["month"].asInt();
        day = holiday["day"].asInt();
    }

    char str[11];
    snprintf(str, 11, "%04d-%02d-%02d", year, month, day);

    std::string result = str;

    return result;
}

std::string ScheduleEntry::CheckHoliday(std::string date)
{
    Json::Value locale = LocaleHolder::GetLocale();
    std::string result;

    if (!locale.isMember("holidays"))
        return date;

    if (isdigit(date.c_str()[0]))
        return date;

    for (int i = 0; i < locale["holidays"].size(); i++)
    {
        if (date == locale["holidays"][i]["shortName"].asString())
            return DateFromLocaleHoliday(locale["holidays"][i]);
    }

    return date;
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

int ScheduleEntry::LoadFromJson(Json::Value &entry)
{
    enabled            = entry["enabled"].asInt();
    playlist           = entry["playlist"].asString();
    dayIndex           = entry["day"].asInt();

    std::vector<std::string> sparts = split(entry["startTime"].asString(), ':');
    startHour          = atoi(sparts[0].c_str());
    startMinute        = atoi(sparts[1].c_str());
    startSecond        = atoi(sparts[2].c_str());

    std::vector<std::string> eparts = split(entry["endTime"].asString(), ':');
    endHour            = atoi(eparts[0].c_str());
    endMinute          = atoi(eparts[1].c_str());
    endSecond          = atoi(eparts[2].c_str());

    repeat             = entry["repeat"].asInt();

    startDateStr = entry["startDate"].asString();
    std::string tempStr = CheckHoliday(startDateStr);

    if (tempStr.length() == 10) {
        startDate = DateStrToInt(tempStr.c_str());
    } else {
        startDate = 20190101;
        startDateStr = "20190101";
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

Json::Value ScheduleEntry::GetJson(void)
{
    Json::Value e;
    char timeText[9];

    e["enabled"] = (int)enabled;
    e["playlist"] = playlist;
    e["day"] = dayIndex;

    sprintf(timeText, "%02d:%02d:%02d", startHour, startMinute, startSecond);
    e["startTime"] = timeText;

    sprintf(timeText, "%02d:%02d:%02d", endHour, endMinute, endSecond);
    e["endTime"] = timeText;

    e["repeat"] = (int)repeat;
    e["startDate"] = startDateStr;
    e["endDate"] = endDateStr;
    e["stopType"] = stopType;

    return e;
}

