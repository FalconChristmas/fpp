/*
 *   Scheduler Class for Falcon Player (FPP)
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

#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>

#include "command.h"
#include "fpp.h"
#include "Player.h"
#include "Scheduler.h"
#include "sunset.h"

#define SCHEDULE_FILE     "/home/fpp/media/config/schedule.json"
#define SCHEDULE_FILE_CSV "/home/fpp/media/schedule"

Scheduler *scheduler = NULL;


static int GetWeeklySeconds(int day, int hour, int minute, int second)
{
  int weeklySeconds = (day*SECONDS_PER_DAY) + (hour*SECONDS_PER_HOUR) + (minute*SECONDS_PER_MINUTE) + second;

  if (weeklySeconds >= SECONDS_PER_WEEK)
    weeklySeconds -= SECONDS_PER_WEEK;

  return weeklySeconds;
}


void SchedulePlaylistDetails::SetTimes(time_t currTime, int nowWeeklySeconds)
{
    actualStartTime = currTime;
    actualStartWeeklySeconds = nowWeeklySeconds;

    scheduledStartTime = currTime - (nowWeeklySeconds - startWeeklySeconds);

    if (nowWeeklySeconds < startWeeklySeconds)
        scheduledStartTime -= 7 * 24 * 60 * 60;

    scheduledEndTime = currTime + (endWeeklySeconds - nowWeeklySeconds);

    if (endWeeklySeconds < startWeeklySeconds)
        scheduledEndTime += 7 * 24 * 60 * 60;

    actualEndTime = scheduledEndTime;
}

/////////////////////////////////////////////////////////////////////////////

Scheduler::Scheduler()
  : m_schedulerDisabled(false),
    m_loadSchedule(true),
	m_lastLoadDate(0),
	m_lastProcTime(0),
    m_forcedNextPlaylist(SCHEDULE_INDEX_INVALID)
{
	RegisterCommands();

    m_schedulerDisabled = getSettingInt("DisableScheduler") == 1;

    if (m_schedulerDisabled) {
        WarningHolder::AddWarning("FPP Scheduler is disabled");
        LogWarn(VB_SCHEDULE, "Scheduler is disabled!\n");
    }

	m_lastProcTime = time(NULL);
	LoadScheduleFromFile();
}

Scheduler::~Scheduler()
{
}

void Scheduler::ScheduleProc(void)
{
    time_t procTime = time(NULL);

    // Only check schedule once per second at most
    if (m_lastProcTime == procTime)
        return;

    m_lastProcTime = procTime;

    // reload the schedule file once per day or if 5 seconds has elapsed
    // since the last process time in case the time jumps
    if ((m_lastLoadDate != GetCurrentDateInt()) ||
        ((procTime - m_lastProcTime) > 5))
    {
        m_loadSchedule = true;

        // Cleanup any ran items older than 2 days
        time_t twoDaysAgo = time(NULL) - (2 * 24 * 60 * 60);
        std::vector<std::time_t> toBeDeleted;
        for (auto& itemTime: m_ranItems) {
            if (itemTime.first < twoDaysAgo) {
                toBeDeleted.push_back(itemTime.first);
            }
        }

        for (auto& deleteTime: toBeDeleted) {
            m_ranItems.erase(deleteTime);
        }
    }

    if (m_loadSchedule)
        LoadScheduleFromFile();

    CheckScheduledItems();

    return;
}

void Scheduler::CheckIfShouldBePlayingNow(int ignoreRepeat, int forceStopped)
{
    if (m_schedulerDisabled)
        return;

    LogDebug(VB_SCHEDULE, "CheckIfShouldBePlayingNow(%d, %d)\n", ignoreRepeat, forceStopped);

    std::time_t now = time(nullptr);

    for (auto& itemTime: m_scheduledItems) {
        if (itemTime.first > now) {
            break; // no need to look at items that are further in the future
        }

        for (auto& item: *itemTime.second) {
            if (item->command == "Start Playlist") {
                if (item->endTime <= now) {
                    continue;
                }

                bool ir = ignoreRepeat;
                if (m_forcedNextPlaylist == item->entryIndex) {
                    ir = true;
                }

                if (((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) ||
                     (Player::INSTANCE.GetPlaylistName() != item->entry->playlist)) &&
                    (forceStopped != item->entryIndex) &&
                    (item->entry->repeat || ir)) {
                    item->ran = false;
                }
            }
        }
    }

    CheckScheduledItems();
}

std::string Scheduler::GetPlaylistThatShouldBePlaying(int &repeat)
{
    if (m_schedulerDisabled)
        return "";

    std::time_t now = time(nullptr);

    for (auto& itemTime: m_scheduledItems) {
        if (itemTime.first > now) {
            break; // no need to look at items that are further in the future
        }

        for (auto& item: *itemTime.second) {
            if (item->command == "Start Playlist") {
                // Skip things that should have ended in the past
                if (item->endTime <= now) {
                    continue;
                }

                repeat = item->entry->repeat;
                return item->entry->playlist;
            }
        }
    }

	repeat = 0;
    return "";
}

void Scheduler::ReloadScheduleFile(void)
{
	m_loadSchedule = true;
}

void Scheduler::CalculateScheduledItems()
{
    LogDebug(VB_SCHEDULE, "Scheduler::CalculateScheduledItems()\n");

    std::map<std::time_t, std::vector<ScheduledItem*>*> oldItems = m_scheduledItems;
    for (auto& itemTime: m_scheduledItems) {
        for (auto& item: *itemTime.second) {
            delete item;
        }
        delete itemTime.second;
    }
    m_scheduledItems.clear();

    std::time_t currTime = std::time(NULL);
    struct tm now;

    localtime_r(&currTime, &now);
    int nowWeeklySeconds = GetWeeklySeconds(now.tm_wday, now.tm_hour, now.tm_min, now.tm_sec);
    time_t weekEpoch = currTime - nowWeeklySeconds;
    time_t startTime = 0;
    time_t endTime = 0;
    std::string playlistFile;

    LogDebug(VB_SCHEDULE, "CurrTime: %lld, NowWeeklySeconds: %d, WeekEpoch: %lld\n", (long long)currTime, nowWeeklySeconds, (long long)weekEpoch);

    for (int i = 0; i < m_Schedule.size(); i++) {
		if (m_Schedule[i].enabled) {
            if (m_Schedule[i].playlist != "") {
                playlistFile = getPlaylistDirectory();
                playlistFile += "/";
                playlistFile += m_Schedule[i].playlist + ".json";

                std::string warning = "Scheduled Playlist '";
                warning += m_Schedule[i].playlist + "' does not exist";

                if (FileExists(playlistFile)) {
                    WarningHolder::RemoveWarning(warning);
                } else {
                    WarningHolder::AddWarning(warning);
                    m_Schedule[i].enabled = false;
                    continue;
                }
            }

            for (auto & startEnd : m_Schedule[i].startEndSeconds) {
                startTime = weekEpoch + startEnd.first;
                if (startEnd.second < startEnd.first) {
                    // wraps over the Sat->Sun week end
                    endTime = weekEpoch + startEnd.second + (7 * 24 * 60 * 60);
                } else {
                    endTime = weekEpoch + startEnd.second;
                }

                if (startTime < (currTime - (24 * 60 * 60))) {
                    startTime += (7 * 24 * 60 * 60);
                    endTime += (7 * 24 * 60 * 60);
                }

                // Check to see if this occurrence is within the date range
                struct tm later;
                localtime_r(&startTime, &later);
                int dateInt = 0;
                dateInt += (later.tm_year + 1900) * 10000;
                dateInt += (later.tm_mon + 1)     *   100;
                dateInt += (later.tm_mday)               ;

                // Skip if occurrence is outside the date range
                if (!DateInRange(dateInt, m_Schedule[i].startDate, m_Schedule[i].endDate))
                    continue;

                ScheduledItem *newItem = new ScheduledItem;

                newItem->entry = &m_Schedule[i];
                newItem->entryIndex = i;
                newItem->priority = i; // change this if we add a priority field
                newItem->startTime = startTime;
                newItem->endTime = startTime;

                if (m_Schedule[i].playlist != "") {
                    // Old style schedule entry without a FPP Command
                    std::vector<std::string> startArgs;
                    startArgs.push_back(m_Schedule[i].playlist);
                    startArgs.push_back(m_Schedule[i].repeat ? "true" : "false");
                    startArgs.push_back("false");

                    newItem->command = "Start Playlist";
                    newItem->args = startArgs;
                    newItem->endTime = endTime;

                    // Check to see if this item already ran
                    auto sVec = m_ranItems.find(newItem->startTime);
                    if (sVec != m_ranItems.end()) {
                        for (auto& item: *sVec->second) {
                            if ((newItem->command == item->command) &&
                                (newItem->startTime == item->startTime) &&
                                (newItem->endTime == item->endTime) &&
                                (newItem->args.size() == item->args.size()) &&
                                ((!newItem->args.size() && !item->args.size()) ||
                                 (newItem->args[0] == item->args[0]))) {
                                LogDebug(VB_SCHEDULE, "Marking playlist item as already ran:\n");
                                DumpScheduledItem(newItem->startTime, newItem);
                                newItem->ran = true;
                            }
                        }
                    }
                } else {
                    // New style schedule entry with a FPP Command
                    std::vector<std::string> startArgs;
                    for (int j = 0; j < m_Schedule[i].args.size(); j++) {
                        startArgs.push_back(m_Schedule[i].args[j].asString());
                    }

                    newItem->command = m_Schedule[i].command;
                    newItem->args = startArgs;
                }

                auto sVec = m_scheduledItems.find(startTime);
                if (sVec != m_scheduledItems.end()) {
                    sVec->second->push_back(newItem);
                } else {
                    std::vector<ScheduledItem*> *newVec = new std::vector<ScheduledItem*>;
                    newVec->push_back(newItem);
                    m_scheduledItems[startTime] = newVec;
                }
            }
        }
    }

    DumpScheduledItems();
}

void Scheduler::DumpScheduledItem(std::time_t itemTime, ScheduledItem *item)
{
    std::string timeStr = ctime(&itemTime);

    std::string argStr;
    for (const auto& arg: item->args) {
        argStr += "\"";
        argStr += arg;
        argStr += "\" ";
    }

    LogDebug(VB_SCHEDULE, "%s: %2d '%s' - '%s'\n",
        timeStr.substr(0, timeStr.length() - 1).c_str(),
        item->priority,
        item->command.c_str(),
        argStr.c_str()
    );
}

void Scheduler::DumpScheduledItems()
{
    LogDebug(VB_SCHEDULE, "Scheduler::DumpScheduledItems()\n");

    for (const auto& itemTime: m_scheduledItems) {
        for (const auto& item: *itemTime.second) {
            DumpScheduledItem(itemTime.first, item);
        }
    }
}

void Scheduler::CheckScheduledItems()
{
    if (m_schedulerDisabled)
        return;

    std::time_t now = time(nullptr);

    for (auto& itemTime: m_scheduledItems) {
        if (itemTime.first > now) {
            // Check to see if we should be counting down to the next item
            bool logItems = false;
            int diff = itemTime.first - now;

            // Print the countdown more frequently as we get closer
            if (((diff > 300) &&                  ((diff % 300) == 0)) ||
                ((diff >  60) && (diff <= 300) && ((diff %  60) == 0)) ||
                ((diff >  10) && (diff <=  60) && ((diff %  10) == 0)) ||
                (                (diff <=  10)))
            {
                logItems = true;
            }

            if (logItems) {
                LogDebug(VB_SCHEDULE, "Scheduled Item%s running in %d second%s:\n",
                    itemTime.second->size() == 1 ? "" : "s",
                    diff,
                    diff == 1 ? "" : "s");

                for (auto& item: *itemTime.second) {
                    DumpScheduledItem(itemTime.first, item);
                }
            }

            break; // no need to look at items that are further in the future
        }

        for (auto& item: *itemTime.second) {
            if (item->ran)
                continue; // skip over any items that ran already

            if (item->command == "Start Playlist") {
                LogDebug(VB_SCHEDULE, "Checking scheduled 'Start Playlist'\n");
                DumpScheduledItem(itemTime.first, item);

                if (item->endTime <= now) {
                    SetItemRan(item, true);

                    // don't try to start if scheduled end has passed
                    continue;
                }

                if (Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) {
                    // Check to see if we are already playing this item, for
                    // instance if we reloaded the schedule while a scheduled
                    // playlist was playing.
                    if ((Player::INSTANCE.GetPlaylistName() == item->entry->playlist) &&
                        (Player::INSTANCE.WasScheduled()) &&
                        (Player::INSTANCE.GetRepeat() == item->entry->repeat) &&
                        (Player::INSTANCE.GetStopTime() == item->endTime) &&
                        (Player::INSTANCE.GetStopMethod() == item->entry->stopType)) {
                        SetItemRan(item, true);
                        continue;
                    }

                    // Don't restart a playlist if it was just force stopped
                    if ((Player::INSTANCE.GetForceStopped()) &&
                        (Player::INSTANCE.GetOrigStartTime() == item->startTime) &&
                        (Player::INSTANCE.GetForceStoppedPlaylist() == item->entry->playlist)) {
                        SetItemRan(item, true);
                        continue;
                    }

                    if (!Player::INSTANCE.WasScheduled()) {
                        // Manually started playlist is running so stop it
                        while (Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) {
                            Player::INSTANCE.StopNow(1);
                        }
                    } else if (Player::INSTANCE.GetPriority() > item->priority) {
                        // Lower priority (higher number) playlist is running
                        // Reset the ran status on the schedule entry for the
                        // running playlist to false so it shows as 'next' again
                        std::time_t oldStartTime = Player::INSTANCE.GetOrigStartTime();
                        std::string playlistName = Player::INSTANCE.GetPlaylistName();
                        std::vector<ScheduledItem*> *oldItems = m_scheduledItems[oldStartTime];
                        for (auto& oldItem: *oldItems) {
                            if ((oldItem->command == "Start Playlist") &&
                                (oldItem->entry->playlist == playlistName)) {
                                oldItem->ran = false;
                                m_forcedNextPlaylist == oldItem->entryIndex;
                            }
                        }


                        // and now stop it
                        while (Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) {
                            Player::INSTANCE.StopNow(1);
                        }
                    } else {
                        // Need to wait for current Scheduled Higher-Priority
                        // item to end until we can play multiple playlists
                        // concurrently
                        continue;
                    }
                }

                LogDebug(VB_SCHEDULE, "Starting Scheduled Playlist:\n");
                DumpScheduledItem(itemTime.first, item);

                Player::INSTANCE.StartScheduledPlaylist(item->entry->playlist,
                    item->entry->repeat, item->entryIndex,
                    item->entryIndex, // priority is entry index for now
                    item->startTime, item->endTime, item->entry->stopType);
            } else {
                if (itemTime.first < now) {
                    SetItemRan(item, true);
                    continue; // skip any FPP Commands that are in the past
                }

                LogDebug(VB_SCHEDULE, "Running scheduled item:\n");
                DumpScheduledItem(itemTime.first, item);
                std::thread *t = new std::thread([this](std::string command, std::vector<std::string> args) {
                    CommandManager::INSTANCE.run(command, args);
                }, item->command, item->args);
                t->detach();
            }

            SetItemRan(item, true);
        }
    }
}

void Scheduler::SetItemRan(ScheduledItem *item, bool ran)
{
    item->ran = ran;

    auto sVec = m_ranItems.find(item->startTime);
    if (sVec != m_ranItems.end()) {
        bool found = false;
        for (auto& ranItem: *sVec->second) {
            if ((item->command == ranItem->command) &&
                (item->startTime == ranItem->startTime) &&
                (item->endTime == ranItem->endTime) &&
                (item->args.size() == ranItem->args.size()) &&
                ((!item->args.size() && !ranItem->args.size()) ||
                 (item->args[0] == ranItem->args[0]))) {
                ranItem->ran = ran;
                found = true;
            }
        }

        if (!found)
            sVec->second->push_back(new ScheduledItem(item));
    } else {
        std::vector<ScheduledItem*> *newVec = new std::vector<ScheduledItem*>;
        newVec->push_back(new ScheduledItem(item));
        m_ranItems[item->startTime] = newVec;
    }
}

void Scheduler::GetSunInfo(const std::string &info, int moffset, int &hour, int &minute, int &second)
{
	std::string latStr = getSetting("Latitude");
	std::string lonStr = getSetting("Longitude");

	if ((latStr == "") || (lonStr == ""))
	{
		latStr = "38.938524";
		lonStr = "-104.600945";

		LogErr(VB_SCHEDULE, "Error, Latitude/Longitude not filled in, using Falcon, Colorado coordinates!\n");
	}

	std::string::size_type sz;
	double lat = std::stod(latStr, &sz);
	double lon = std::stod(lonStr, &sz);
	double sunOffset = 0;
	time_t currTime = time(NULL);
	struct tm utc;
	struct tm local;

	gmtime_r(&currTime, &utc);
	localtime_r(&currTime, &local);

	LogDebug(VB_SCHEDULE, "Lat/Lon: %.6f, %.6f\n", lat, lon);
	LogDebug(VB_SCHEDULE, "Today (UTC) is %02d/%02d/%04d, UTC offset is %d hours\n",
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
    
    sunOffset += moffset;

    if (sunOffset < 0) {
        LogDebug(VB_SCHEDULE, "Sunrise calculated as before midnight last night, using 8AM.  Check your time zone to make sure it is valid.\n");
        hour = 8;
        minute = 0;
        second = 0;
        return;
    } else if (sunOffset >= (24 * 60 * 60)) {
        LogDebug(VB_SCHEDULE, "Sunrise calculated as after midnight tomorrow, using 8PM.  Check your time zone to make sure it is valid.\n");
        hour = 20;
        minute = 0;
        second = 0;
        return;
    }

	LogDebug(VB_SCHEDULE, "%s Time Offset: %.2f minutes\n", info.c_str(), sunOffset);
	hour = (int)sunOffset / 60;
	minute = (int)sunOffset % 60;
	second = (int)(((int)(sunOffset * 100) % 100) * 0.01 * 60);

    LogDebug(VB_SCHEDULE, "%s is at %02d:%02d:%02d\n", info.c_str(), hour, minute, second);
}


void Scheduler::SetScheduleEntrysWeeklyStartAndEndSeconds(ScheduleEntry *entry)
{
	if (entry->dayIndex & INX_DAY_MASK) {
		if (entry->dayIndex & INX_DAY_MASK_SUNDAY) {
            entry->pushStartEndTimes(GetWeeklySeconds(INX_SUN,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_SUN,entry->endHour,entry->endMinute,entry->endSecond));
		}

		if (entry->dayIndex & INX_DAY_MASK_MONDAY) {
			entry->pushStartEndTimes(GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond));
		}

		if (entry->dayIndex & INX_DAY_MASK_TUESDAY) {
			entry->pushStartEndTimes(GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond));
		}

		if (entry->dayIndex & INX_DAY_MASK_WEDNESDAY) {
			entry->pushStartEndTimes(GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond));
		}

		if (entry->dayIndex & INX_DAY_MASK_THURSDAY) {
			entry->pushStartEndTimes(GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond));
		}

		if (entry->dayIndex & INX_DAY_MASK_FRIDAY) {
			entry->pushStartEndTimes(GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond));
		}

		if (entry->dayIndex & INX_DAY_MASK_SATURDAY) {
            entry->pushStartEndTimes(GetWeeklySeconds(INX_SAT,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_SAT,entry->endHour,entry->endMinute,entry->endSecond));
		}
		return;
    }

	// Some variables needed for odd/even day calculations
    struct std::tm FPPEpoch = {0,0,0,15,6,113}; //2013-07-15
    std::time_t FPPEpochTimeT = std::mktime(&FPPEpoch);
    std::time_t currTime = std::time(nullptr);
    double difference = std::difftime(currTime, FPPEpochTimeT) / (60 * 60 * 24);
	int daysSince = difference;
	int oddSunday = 0;
	int i = 0;

	struct tm now;
	localtime_r(&currTime, &now);

    if ((daysSince - now.tm_wday) % 2) {
		oddSunday = 1; // This past Sunday was an odd day
    }

	switch(entry->dayIndex) {
		case INX_SUN:
		case INX_MON:
		case INX_TUE:
		case INX_WED:
		case INX_THU:
		case INX_FRI:
        case INX_SAT:
            entry->pushStartEndTimes(GetWeeklySeconds(entry->dayIndex,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(entry->dayIndex,entry->endHour,entry->endMinute,entry->endSecond));
            break;
        case INX_EVERYDAY:
            entry->pushStartEndTimes(GetWeeklySeconds(INX_SUN,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_SUN,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_SAT,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_SAT,entry->endHour,entry->endMinute,entry->endSecond));
            break;
        case INX_WKDAYS:
            entry->pushStartEndTimes(GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond));
            break;
        case INX_WKEND:
            entry->pushStartEndTimes(GetWeeklySeconds(INX_SAT,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_SAT,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_SUN,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_SUN,entry->endHour,entry->endMinute,entry->endSecond));
            break;
        case INX_M_W_F:
            entry->pushStartEndTimes(GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond));
            break;
        case INX_T_TH:
            entry->pushStartEndTimes(GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond));
            break;
		case INX_SUN_TO_THURS:
            entry->pushStartEndTimes(GetWeeklySeconds(INX_SUN,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_SUN,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond));
			break;
		case INX_FRI_SAT:
            entry->pushStartEndTimes(GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond));
            entry->pushStartEndTimes(GetWeeklySeconds(INX_SAT,entry->startHour,entry->startMinute,entry->startSecond),
                                     GetWeeklySeconds(INX_SAT,entry->endHour,entry->endMinute,entry->endSecond));
            break;
        case INX_ODD_DAY: // Odd days starting at FPP epoch (2013-07-15 according to 'git log')
            for (int dow = 0; dow < 7; dow++) {
                if (((dow < now.tm_wday) &&
                     (( oddSunday && ((dow % 2) == 1)) ||
                      (!oddSunday && ((dow % 2) == 0)))) ||
                    ((dow >= now.tm_wday) &&
                     (( oddSunday && ((dow % 2) == 0)) ||
                      (!oddSunday && ((dow % 2) == 1))))) {
                    entry->pushStartEndTimes(GetWeeklySeconds(dow,entry->startHour,entry->startMinute,entry->startSecond),
                                             GetWeeklySeconds(dow,entry->endHour,entry->endMinute,entry->endSecond));
                }
            }
            break;
        case INX_EVEN_DAY: // Even days starting at FPP epoch (2013-07-15 according to 'git log')
            for (int dow = 0; dow < 7; dow++) {
                if (((dow < now.tm_wday) &&
                     (( oddSunday && ((dow % 2) == 0)) ||
                      (!oddSunday && ((dow % 2) == 1)))) ||
                    ((dow >= now.tm_wday) &&
                     (( oddSunday && ((dow % 2) == 1)) ||
                      (!oddSunday && ((dow % 2) == 0))))) {
                    entry->pushStartEndTimes(GetWeeklySeconds(dow,entry->startHour,entry->startMinute,entry->startSecond),
                                             GetWeeklySeconds(dow,entry->endHour,entry->endMinute,entry->endSecond));
                }
            }
            break;
        default:
            break;
    }
}

void Scheduler::LoadScheduleFromFile(void)
{
  LogDebug(VB_SCHEDULE, "Loading Schedule from %s\n", SCHEDULE_FILE);

  m_loadSchedule = false;
  m_lastLoadDate = GetCurrentDateInt();

  std::unique_lock<std::mutex> lock(m_scheduleLock);
  m_Schedule.clear();

  std::string playlistFile;

  Json::Value sch = LoadJsonFromFile(SCHEDULE_FILE);
  for (int i = 0; i < sch.size(); i++) {
	ScheduleEntry scheduleEntry;
	if (!scheduleEntry.LoadFromJson(sch[i]))
        continue;

    playlistFile = getPlaylistDirectory();
    playlistFile += "/";
    playlistFile += scheduleEntry.playlist + ".json";

    std::string warning = "Scheduled Playlist '";
    warning += scheduleEntry.playlist + "' does not exist";

    if ((scheduleEntry.enabled) &&
        (scheduleEntry.playlist != "") &&
        (!FileExists(playlistFile))) {
        LogErr(VB_SCHEDULE, "ERROR: Scheduled Playlist '%s' does not exist\n",
            scheduleEntry.playlist.c_str());
        WarningHolder::AddWarning(warning);
        continue;
    } else {
        WarningHolder::RemoveWarning(warning);
    }

	// Check for sunrise/sunset flags
    std::size_t found = scheduleEntry.startTimeStr.find(":");
    if (found == std::string::npos) {
		GetSunInfo( scheduleEntry.startTimeStr,
                    scheduleEntry.startTimeOffset,
					scheduleEntry.startHour,
					scheduleEntry.startMinute,
					scheduleEntry.startSecond);
    }

    found = scheduleEntry.endTimeStr.find(":");
    if (found == std::string::npos) {
		GetSunInfo( scheduleEntry.endTimeStr,
                    scheduleEntry.endTimeOffset,
					scheduleEntry.endHour,
					scheduleEntry.endMinute,
					scheduleEntry.endSecond);
    }

    // Set WeeklySecond start and end times
    SetScheduleEntrysWeeklyStartAndEndSeconds(&scheduleEntry);
    m_Schedule.push_back(scheduleEntry);
  }

  SchedulePrint();
  CalculateScheduledItems();

  lock.unlock();

  return;
}

void Scheduler::SchedulePrint(void)
{
  int i=0;
  char stopTypes[4] = "GHL";
  std::string dayStr;

  if (m_schedulerDisabled)
    LogInfo(VB_SCHEDULE, "WARNING: Scheduler is currently disabled\n");

  LogInfo(VB_SCHEDULE, "Current Schedule: (Status: '+' = Enabled, '-' = Disabled, '!' = Outside Date Range, '*' = Repeat, Stop (G)raceful/(L)oop/(H)ard\n");
  LogInfo(VB_SCHEDULE, "Stat Start & End Dates       Days         Start & End Times   Playlist\n");
  LogInfo(VB_SCHEDULE, "---- ----------------------- ------------ ------------------- ---------------------------------------------\n");
  for (i = 0; i < m_Schedule.size(); i++) {
    dayStr = GetDayTextFromDayIndex(m_Schedule[i].dayIndex);
    LogInfo(VB_SCHEDULE, "%c%c%c%c %04d-%02d-%02d - %04d-%02d-%02d %-12.12s %02d:%02d:%02d - %02d:%02d:%02d %s\n",
      m_Schedule[i].enabled ? '+': '-',
      CurrentDateInRange(m_Schedule[i].startDate, m_Schedule[i].endDate) ? ' ': '!',
      m_Schedule[i].repeat ? '*': ' ',
      stopTypes[m_Schedule[i].stopType],
      (int)(m_Schedule[i].startDate / 10000),
      (int)(m_Schedule[i].startDate % 10000 / 100),
      (int)(m_Schedule[i].startDate % 100),
      (int)(m_Schedule[i].endDate / 10000),
      (int)(m_Schedule[i].endDate % 10000 / 100),
      (int)(m_Schedule[i].endDate % 100),
      dayStr.c_str(),
      m_Schedule[i].startHour,m_Schedule[i].startMinute,m_Schedule[i].startSecond,
      m_Schedule[i].endHour,m_Schedule[i].endMinute,m_Schedule[i].endSecond,
      m_Schedule[i].playlist != "" ?
          m_Schedule[i].playlist.c_str() :
          m_Schedule[i].command.c_str());
  }

  LogDebug(VB_SCHEDULE, "//////////////////////////////////////////////////\n");
}

ScheduledItem *Scheduler::GetNextScheduledPlaylist()
{
    std::time_t now = time(nullptr);

    for (auto& itemTime: m_scheduledItems) {
        for (auto& item: *itemTime.second) {
            if (item->ran)
                continue; // skip over any items that ran already

            if (item->command == "Start Playlist")
                return item;
        }
    }

    return nullptr;
}

std::string Scheduler::GetNextPlaylistName()
{
    if (m_schedulerDisabled)
        return "Scheduler is disabled.";

    ScheduledItem *item = GetNextScheduledPlaylist();

    if (!item)
        return "No playlist scheduled.";

    return item->entry->playlist;
}

std::string Scheduler::GetNextPlaylistStartStr()
{
    if (m_schedulerDisabled)
        return "";

    const char timeFmt[] = "%a @ %H:%M:%S";
    std::string result;
    ScheduledItem *item = GetNextScheduledPlaylist();

    if (!item)
        return result;

    char timeStr[32];
    std::time_t timeT = 0;
    struct tm timeStruct;
    time_t currTime = time(NULL);

    localtime_r(&item->startTime, &timeStruct);
    strftime(timeStr, 32, timeFmt, &timeStruct);

    // Thursday @ 11:00:00 - (Everyday)
    result = timeStr;
    result += " - (";
    result += GetDayTextFromDayIndex(item->entry->dayIndex);
    result += ")";

    return result;
}

std::string Scheduler::GetDayTextFromDayIndex(const int index)
{
	if (index & INX_DAY_MASK)
		return "Day Mask";

	switch(index)
	{
		case 0:  return "Sunday";
		case 1:  return "Monday";
		case 2:  return "Tuesday";
		case 3:  return "Wednesday";
		case 4:  return "Thursday";
		case 5:  return "Friday";
		case 6:  return "Saturday";
		case 7:  return "Everyday";
		case 8:  return "Weekdays";
		case 9:  return "Weekends";
		case 10: return "Mon/Wed/Fri";
		case 11: return "Tues-Thurs";
		case 12: return "Sun-Thurs";
		case 13: return "Fri/Sat";
		case 14: return "Odd Days";
		case 15: return "Even Days";
	}

    return "Error";
}

Json::Value Scheduler::GetInfo(void)
{
    const char timeFmt[] = "%a @ %H:%M:%S";
    Json::Value result;

    Json::Value np;
    np["playlistName"] = GetNextPlaylistName();
    np["scheduledStartTime"] = 0;
    np["scheduledStartTimeStr"] = GetNextPlaylistStartStr();

    result["enabled"] = m_schedulerDisabled ? 0 : 1;
    result["nextPlaylist"] = np;

    if ((Player::INSTANCE.GetStatus() == FPP_STATUS_PLAYLIST_PLAYING) ||
        (Player::INSTANCE.GetStatus() == FPP_STATUS_STOPPING_GRACEFULLY) ||
        (Player::INSTANCE.GetStatus() == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)) {
        Json::Value cp;

        if (Player::INSTANCE.WasScheduled()) {
            char timeStr[32];
            std::time_t timeT = 0;
            struct tm timeStruct;
            time_t currTime = time(NULL);

            localtime_r(&timeT, &timeStruct);
            strftime(timeStr, 32, timeFmt, &timeStruct);
            cp["currentTime"] = (Json::UInt64)currTime;
            cp["currentTimeStr"] = timeStr;

            timeT = Player::INSTANCE.GetOrigStartTime();
            localtime_r(&timeT, &timeStruct);
            strftime(timeStr, 32, timeFmt, &timeStruct);
            cp["scheduledStartTime"] = (Json::UInt64)timeT;
            cp["scheduledStartTimeStr"] = timeStr;

            timeT = Player::INSTANCE.GetStartTime();
            localtime_r(&timeT, &timeStruct);
            strftime(timeStr, 32, timeFmt, &timeStruct);
            cp["actualStartTime"] = (Json::UInt64)timeT;
            cp["actualStartTimeStr"] = timeStr;

            timeT = Player::INSTANCE.GetOrigStopTime();
            localtime_r(&timeT, &timeStruct);
            strftime(timeStr, 32, timeFmt, &timeStruct);
            cp["scheduledEndTime"] = (Json::UInt64)timeT;
            cp["scheduledEndTimeStr"] = timeStr;

            timeT = Player::INSTANCE.GetStopTime();
            localtime_r(&timeT, &timeStruct);
            strftime(timeStr, 32, timeFmt, &timeStruct);
            cp["actualEndTime"] = (Json::UInt64)timeT;
            cp["actualEndTimeStr"] = timeStr;
            cp["secondsRemaining"] = (int)(timeT - currTime);

            int stopType = Player::INSTANCE.GetStopMethod();
            cp["stopType"] = stopType;
            cp["stopTypeStr"] = stopType == 2 ? "Graceful Loop" : stopType == 1 ? "Hard" : "Graceful";


            result["status"] = "playing";
        } else {
            result["status"] = "manual";
        }

        cp["playlistName"] = Player::INSTANCE.GetPlaylistName();

        result["currentPlaylist"] = cp;
    } else {
        result["status"] = "idle";
    }


    return result;
}

Json::Value Scheduler::GetSchedule()
{
    const char timeFmt[] = "%a @ %H:%M:%S";
    Json::Value result;
    Json::Value entries(Json::arrayValue);
    Json::Value entry;
    Json::Value items(Json::arrayValue);
    Json::Value scheduledItem;
    std::time_t now = time(nullptr);
    std::unique_lock<std::mutex> lock(m_scheduleLock);

    result["enabled"] = m_schedulerDisabled ? 0 : 1;

    for (int i = 0; i < m_Schedule.size(); i++) {
        entry = m_Schedule[i].GetJson();
        entry["id"] = i;

        if (m_Schedule[i].playlist != "")
            entry["type"] = "playlist";
        else
            entry["type"] = "command";

        entry["dayStr"] = GetDayTextFromDayIndex(m_Schedule[i].dayIndex);

        entries.append(entry);
    }
    result["entries"] = entries;

    struct tm timeStruct;
    char timeStr[32];

    for (auto& itemTime: m_scheduledItems) {
        for (auto& item: *itemTime.second) {
            if (item->ran)
                continue;

            scheduledItem["priority"] = item->priority;
            scheduledItem["id"] = item->entryIndex;

            localtime_r(&item->startTime, &timeStruct);
            strftime(timeStr, 32, timeFmt, &timeStruct);
            scheduledItem["startTimeStr"] = timeStr;
            scheduledItem["startTime"] = (Json::UInt64)item->startTime;

            localtime_r(&item->endTime, &timeStruct);
            strftime(timeStr, 32, timeFmt, &timeStruct);
            scheduledItem["endTimeStr"] = timeStr;
            scheduledItem["endTime"] = (Json::UInt64)item->endTime;

            scheduledItem["command"] = item->command;

            Json::Value args(Json::arrayValue);
            for (auto& arg: item->args) {
                args.append(arg);
            }
            scheduledItem["args"] = args;

            items.append(scheduledItem);
        }
    }
    result["items"] = items;

    return result;
}

class ScheduleCommand : public Command {
public:
    ScheduleCommand(const std::string &str, Scheduler *s) : Command(str), sched(s) {}
    virtual ~ScheduleCommand() {}

    Scheduler *sched;
};

class ExtendScheduleCommand : public ScheduleCommand {
public:
    ExtendScheduleCommand(Scheduler *s) : ScheduleCommand("Extend Schedule", s) {
        args.push_back(CommandArg("Seconds", "int", "Seconds").setRange(-12 * 60 * 60, 12 * 60 * 60).setDefaultValue("300").setAdjustable());
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() != 1) {
            return std::make_unique<Command::ErrorResult>("Command needs 1 argument, found " + std::to_string(args.size()));
        }

        if (Player::INSTANCE.AdjustPlaylistStopTime(std::stoi(args[0])))
            return std::make_unique<Command::Result>("Schedule Updated");

        return std::make_unique<Command::Result>("Error extending Schedule");
    }
};

void Scheduler::RegisterCommands() {
    CommandManager::INSTANCE.addCommand(new ExtendScheduleCommand(this));
}

ScheduledItem::ScheduledItem()
  : priority(1),
    entry(nullptr),
    ran(false)
{
}

ScheduledItem::ScheduledItem(ScheduledItem *item)
{
    priority = item->priority;
    entry = nullptr;
    entryIndex = item->entryIndex;
    ran = item->ran;
    startTime = item->startTime;
    endTime = item->endTime;
    command = item->command;
    args = item->args;
}

