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
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <time.h>
#include <utility>
#include <vector>

#include "Player.h"
#include "Warnings.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"
#include "playlist/Playlist.h"

#include "Scheduler.h"

Scheduler* scheduler = NULL;

/////////////////////////////////////////////////////////////////////////////

// Test assumptions
static_assert(std::is_move_constructible_v<ScheduledItem>);

/////////////////////////////////////////////////////////////////////////////

Scheduler::Scheduler() :
    m_schedulerDisabled(false),
    m_loadSchedule(true),
    m_lastLoadDate(0),
    m_lastProcTime(0),
    m_timeDelta(0),
    m_timeDeltaThreshold(0),
    m_forcedNextPlaylist(SCHEDULE_INDEX_INVALID),
    m_lastBlockedTime(0),
    m_schedulesExtendBeyondDistance(false) {
    RegisterCommands();

    m_schedulerDisabled = getSettingInt("DisableScheduler") == 1;

    if ((m_schedulerDisabled) && (getFPPmode() == PLAYER_MODE)) {
        WarningHolder::AddWarning("FPP Scheduler is disabled");
        LogWarn(VB_SCHEDULE, "Scheduler is disabled!\n");
    }

    m_lastProcTime = time(NULL);
    LoadScheduleFromFile();
}

Scheduler::~Scheduler() = default;

void Scheduler::ScheduleProc(void) {
    time_t procTime = time(NULL);

    // Only check schedule once per second at most
    if (m_lastProcTime == procTime)
        return;

    int timeDelta = (int)(procTime - m_lastProcTime);
    m_lastProcTime = procTime;

    // reload the schedule file once per day or if 5 seconds has elapsed
    // since the last process time in case the time jumps
    if ((m_lastLoadDate != GetCurrentDateInt()) ||
        (abs(timeDelta) > 5)) {
        m_loadSchedule = true;

        // Cleanup any ran items older than 2 days
        const time_t twoDaysAgo = time(NULL) - (2 * 24 * 60 * 60);
        for (auto it = m_ranItems.begin(); it != m_ranItems.end(); /* in loop */) {
            if (it->first >= twoDaysAgo)
                break;
            it = m_ranItems.erase(it);
        }
        
        // If time jumped forward significantly (e.g., NTP sync after boot with wrong time),
        // check if we should be playing now
        if (timeDelta > 60) {
            LogInfo(VB_SCHEDULE, "Detected significant time jump forward (%d seconds), checking scheduled items\n", timeDelta);
            CheckIfShouldBePlayingNow();
        }
    }

    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);

    if (m_loadSchedule)
        LoadScheduleFromFile();

    CheckScheduledItems();

    return;
}

void Scheduler::CheckIfShouldBePlayingNow(int ignoreRepeat, int forceStopped) {
    if (m_schedulerDisabled)
        return;

    LogDebug(VB_SCHEDULE, "CheckIfShouldBePlayingNow(%d, %d)\n", ignoreRepeat, forceStopped);

    std::time_t now = time(nullptr);
    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);

    for (auto& itemTime : m_scheduledItems) {
        if (itemTime.first > now) {
            break; // no need to look at items that are further in the future
        }

        LogExcess(VB_SCHEDULE, "Checking scheduled items:\n");
        for (auto& item : itemTime.second) {
            if (WillLog(LOG_EXCESSIVE, VB_SCHEDULE))
                DumpScheduledItem(item.startTime, item);
            if (item.command == "Start Playlist") {
                if (item.endTime <= now) {
                    LogExcess(VB_SCHEDULE, "End time is in the past, skipping item\n");
                    continue;
                }

                bool ir = ignoreRepeat;
                if (m_forcedNextPlaylist == item.entryIndex) {
                    LogExcess(VB_SCHEDULE, "This item entry index == m_forcedNextPlaylist\n");
                    ir = true;
                }

                if (((Player::INSTANCE.GetStatus() == FPP_STATUS_IDLE) ||
                     (Player::INSTANCE.GetPlaylistName() != item.entry->playlist)) &&
                    (forceStopped != item.entryIndex) &&
                    (item.entry->repeat || ir)) {
                    LogExcess(VB_SCHEDULE, "Item run status reset\n");
                    item.ran = false;
                }
            }
        }
    }
    LogExcess(VB_SCHEDULE, "Done checking list, proceeding to call CheckScheduledItems()\n");

    CheckScheduledItems(getSettingInt("restarted") == 1);
}

std::string Scheduler::GetPlaylistThatShouldBePlaying(int& repeat) {
    if (m_schedulerDisabled)
        return "";

    std::time_t now = time(nullptr);
    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);

    for (auto& itemTime : m_scheduledItems) {
        if (itemTime.first > now) {
            break; // no need to look at items that are further in the future
        }

        for (auto& item : itemTime.second) {
            if (item.command == "Start Playlist") {
                // Skip things that should have ended in the past
                if (item.endTime <= now) {
                    continue;
                }

                repeat = item.entry->repeat;
                return item.entry->playlist;
            }
        }
    }

    repeat = 0;
    return "";
}

void Scheduler::ReloadScheduleFile(void) {
    m_loadSchedule = true;
}

void Scheduler::AddScheduledItems(ScheduleEntry* entry, int index) {
    if (!entry->enabled)
        return;

    if (!entry->playlist.empty()) {
        std::string playlistFile;
        std::string warning = "Scheduled ";

        if (entry->sequence) {
            playlistFile = FPP_DIR_SEQUENCE("/" + entry->playlist);

            warning = "Sequence";
        } else {
            playlistFile = FPP_DIR_PLAYLIST("/" + entry->playlist + ".json");

            warning = "Playlist";
        }
        warning = " '";
        warning += entry->playlist + "' does not exist";

        if (FileExists(playlistFile)) {
            WarningHolder::RemoveWarning(warning);
        } else {
            WarningHolder::AddWarning(warning);
            entry->enabled = false;
            return;
        }
    }

    int dayIndex = entry->dayIndex;
    std::time_t currTime = std::time(nullptr);
    struct tm now;
    localtime_r(&currTime, &now);
    int scheduleDistance = getSettingInt("ScheduleDistance");

    // Check if this schedule's end date extends beyond the schedule distance
    // Track this for warning display but respect the user's ScheduleDistance setting
    if (entry->endDate > 0 && entry->endDate < 10000000) {
        // This is a yearless date (< 10000000), calculate actual end date
        int currentDate = (now.tm_year + 1900) * 10000 + (now.tm_mon + 1) * 100 + now.tm_mday;
        int endMD = entry->endDate;

        // Reconstruct the full end date for the current or next year
        int endYear = now.tm_year + 1900;
        int calculatedEndDate = endYear * 10000 + endMD;

        // If calculated end date is before today, use next year
        if (calculatedEndDate < currentDate) {
            calculatedEndDate = (endYear + 1) * 10000 + endMD;
        }

        // Calculate days from now to end date
        int daysToEnd = (calculatedEndDate / 10000 - currentDate / 10000) * 365 +
                        ((calculatedEndDate / 100 % 100) - (currentDate / 100 % 100)) * 30 +
                        (calculatedEndDate % 100 - currentDate % 100);

        if (daysToEnd > scheduleDistance) {
            m_schedulesExtendBeyondDistance = true;
        }
    } else if (entry->endDate >= 10000000) {
        // Full date specified
        int currentDate = (now.tm_year + 1900) * 10000 + (now.tm_mon + 1) * 100 + now.tm_mday;
        int daysDiff = ((entry->endDate / 10000) - (currentDate / 10000)) * 365 +
                       ((entry->endDate / 100 % 100) - (currentDate / 100 % 100)) * 30 +
                       (entry->endDate % 100 - currentDate % 100);

        if (daysDiff > scheduleDistance) {
            m_schedulesExtendBeyondDistance = true;
        }
    }

    // Convert everything to a day mask to simplify code below
    switch (dayIndex) {
    case INX_SUN:
        dayIndex = INX_DAY_MASK_SUNDAY;
        break;
    case INX_MON:
        dayIndex = INX_DAY_MASK_MONDAY;
        break;
    case INX_TUE:
        dayIndex = INX_DAY_MASK_TUESDAY;
        break;
    case INX_WED:
        dayIndex = INX_DAY_MASK_WEDNESDAY;
        break;
    case INX_THU:
        dayIndex = INX_DAY_MASK_THURSDAY;
        break;
    case INX_FRI:
        dayIndex = INX_DAY_MASK_FRIDAY;
        break;
    case INX_SAT:
        dayIndex = INX_DAY_MASK_SATURDAY;
        break;
    case INX_EVERYDAY:
        dayIndex = INX_DAY_MASK_EVERYDAY;
        break;
    case INX_WKDAYS:
        dayIndex = INX_DAY_MASK_WEEKDAYS;
        break;
    case INX_WKEND:
        dayIndex = INX_DAY_MASK_WEEKEND;
        break;
    case INX_M_W_F:
        dayIndex = INX_DAY_MASK_M_W_F;
        break;
    case INX_T_TH:
        dayIndex = INX_DAY_MASK_T_TH;
        break;
    case INX_SUN_TO_THURS:
        dayIndex = INX_DAY_MASK_SUN_TO_THURS;
        break;
    case INX_FRI_SAT:
        dayIndex = INX_DAY_MASK_FRI_SAT;
        break;
    case INX_ODD_DAY:
    case INX_EVEN_DAY:
        // Odd/Even is based on the FPP 'epoch', the date of the first
        // commit to the FPP repository on github, July 15, 2013
        struct std::tm FPPEpoch = { 0, 0, 0, 15, 6, 113 };
        std::time_t FPPEpochTimeT = std::mktime(&FPPEpoch);
        int daysSince = (int)std::difftime(currTime, FPPEpochTimeT) / (60 * 60 * 24);

        int dayOffset = 0;
        if (daysSince % 2) { // Today is an odd day
            if (entry->dayIndex == INX_EVEN_DAY)
                dayOffset = 1;
        } else { // Today is an even day
            if (entry->dayIndex == INX_ODD_DAY)
                dayOffset = 1;
        }

        // Schedule yesterday if needed to handle midnight crossovers
        if (dayOffset)
            entry->pushStartEndTimes(now.tm_wday - 1, m_timeDelta, m_timeDeltaThreshold);

        for (int i = now.tm_wday + dayOffset; i <= scheduleDistance; i += 2) {
            entry->pushStartEndTimes(i, m_timeDelta, m_timeDeltaThreshold);
        }

        break;
    }

    // Special case if today is Sunday, handle any Saturday night crossovers
    if (now.tm_wday == 0)
        entry->pushStartEndTimes(-1, m_timeDelta, m_timeDeltaThreshold);

    if ((entry->dayIndex != INX_ODD_DAY) && (entry->dayIndex != INX_EVEN_DAY)) {
        for (int weekOffset = 0; weekOffset <= scheduleDistance; weekOffset += 7) {
            if (dayIndex & INX_DAY_MASK_SUNDAY)
                entry->pushStartEndTimes(INX_SUN + weekOffset, m_timeDelta, m_timeDeltaThreshold);

            if (dayIndex & INX_DAY_MASK_MONDAY)
                entry->pushStartEndTimes(INX_MON + weekOffset, m_timeDelta, m_timeDeltaThreshold);

            if (dayIndex & INX_DAY_MASK_TUESDAY)
                entry->pushStartEndTimes(INX_TUE + weekOffset, m_timeDelta, m_timeDeltaThreshold);

            if (dayIndex & INX_DAY_MASK_WEDNESDAY)
                entry->pushStartEndTimes(INX_WED + weekOffset, m_timeDelta, m_timeDeltaThreshold);

            if (dayIndex & INX_DAY_MASK_THURSDAY)
                entry->pushStartEndTimes(INX_THU + weekOffset, m_timeDelta, m_timeDeltaThreshold);

            if (dayIndex & INX_DAY_MASK_FRIDAY)
                entry->pushStartEndTimes(INX_FRI + weekOffset, m_timeDelta, m_timeDeltaThreshold);

            if (dayIndex & INX_DAY_MASK_SATURDAY)
                entry->pushStartEndTimes(INX_SAT + weekOffset, m_timeDelta, m_timeDeltaThreshold);
        }
    }

    // loop through entry->startEndTimes and add to m_scheduledItems
    time_t startTime = 0;
    time_t endTime = 0;
    for (auto& startEnd : entry->startEndTimes) {
        startTime = startEnd.first;
        endTime = startEnd.second;

        // Check to see if this occurrence is within the date range
        struct tm later;
        localtime_r(&startTime, &later);
        int dateInt = 0;
        dateInt += (later.tm_year + 1900) * 10000;
        dateInt += (later.tm_mon + 1) * 100;
        dateInt += (later.tm_mday);

        // Skip if occurrence is outside the date range
        if (!DateInRange(dateInt, entry->startDate, entry->endDate)) {
            continue;
        }

        auto& vec = m_scheduledItems[startTime];
        vec.push_back({ .entry = entry,
                        .priority = index, // change this if we add a priority field
                        .entryIndex = index,
                        .ran = false,
                        .startTime = startTime,
                        .endTime = startTime });
        ScheduledItem& newItem = vec.back();

        if (!entry->playlist.empty()) {
            // Old style schedule entry without a FPP Command
            Json::Value args(Json::arrayValue);
            args.append(entry->playlist);
            args.append(entry->repeat ? "true" : "false");
            args.append("false");

            newItem.command = "Start Playlist";
            newItem.args = std::move(args);
            newItem.endTime = endTime;

            // Check to see if this item already ran
            if (auto it = m_ranItems.find(newItem.startTime); it != m_ranItems.end()) {
                for (auto& item : it->second) {
                    if ((newItem.command == item.command) &&
                        (newItem.startTime == item.startTime) &&
                        (newItem.endTime == item.endTime) &&
                        (newItem.args.size() == item.args.size()) &&
                        ((!newItem.args.size() && !item.args.size()) ||
                         (newItem.args[0].asString() == item.args[0].asString()))) {
                        LogDebug(VB_SCHEDULE, "Marking playlist item as already ran:\n");
                        DumpScheduledItem(newItem.startTime, newItem);
                        newItem.ran = true;
                    }
                }
            }
        } else {
            // New style schedule entry with a FPP Command
            newItem.command = entry->command;
            newItem.args = entry->args;
        }
    }
}

void Scheduler::DumpScheduledItem(const std::time_t itemTime, const ScheduledItem& item) {
    std::string timeStr = ctime(&itemTime);

    std::string argStr;
    for (int i = 0; i < item.args.size(); i++) {
        argStr += "\"";
        argStr += item.args[i].asString();
        argStr += "\" ";
    }

    LogDebug(VB_SCHEDULE, "%s: %2d '%s' - '%s'\n",
             timeStr.substr(0, timeStr.length() - 1).c_str(),
             item.priority,
             item.command.c_str(),
             argStr.c_str());
}

void Scheduler::DumpScheduledItems() {
    LogDebug(VB_SCHEDULE, "DumpScheduledItems()\n");

    for (const auto& itemTime : m_scheduledItems) {
        for (const auto& item : itemTime.second) {
            DumpScheduledItem(itemTime.first, item);
        }
    }
}

void Scheduler::doCountdown(const std::time_t now, const std::time_t itemTime, const std::vector<ScheduledItem>& items) {
    // Check to see if we should be counting down to the next item
    bool logItems = false;
    int diff = itemTime - now;

    // Print the countdown more frequently as we get closer
    if (((diff > 300) && ((diff % 300) == 0)) ||
        ((diff > 60) && (diff <= 300) && ((diff % 60) == 0)) ||
        ((diff > 10) && (diff <= 60) && ((diff % 10) == 0)) ||
        ((diff <= 10))) {
        logItems = true;
    }

    if (logItems) {
        LogDebug(VB_SCHEDULE, "Scheduled Item%s running in %d second%s:\n",
                 items.size() == 1 ? "" : "s",
                 diff,
                 diff == 1 ? "" : "s");

        for (auto& item : items) {
            if ((item.command == "Start Playlist") &&
                (diff < 1000)) {
                char tmpStr[27];
                std::map<std::string, std::string> keywords;
                snprintf(tmpStr, 26, "PLAYLIST_START_TMINUS_%03d", diff);
                keywords["PLAYLIST_NAME"] = item.entry->playlist;
                CommandManager::INSTANCE.TriggerPreset(tmpStr, keywords);
            }

            DumpScheduledItem(itemTime, item);
        }
    }
}
void Scheduler::doScheduledCommand(const std::time_t itemTime, const ScheduledItem& item) {
    LogDebug(VB_SCHEDULE, "Running scheduled item:\n");
    DumpScheduledItem(itemTime, item);
    Json::Value cmd;
    cmd["command"] = item.command;
    cmd["args"] = item.args;
    cmd["multisyncCommand"] = item.entry->multisyncCommand;
    cmd["multisyncHosts"] = item.entry->multisyncHosts;

    std::thread th([this](Json::Value cmd) {
        SetThreadName("FPP-RunCmd");
        CommandManager::INSTANCE.run(cmd);
    },
                   std::move(cmd));
    th.detach();
}

bool Scheduler::doScheduledPlaylist(const std::time_t now, const std::time_t itemTime, ScheduledItem& item, bool restarted) {
    LogDebug(VB_SCHEDULE, "Checking scheduled 'Start Playlist'\n");
    DumpScheduledItem(itemTime, item);

    if (item.endTime <= now) {
        SetItemRan(item, true);

        // don't try to start if scheduled end has passed
        return false;
    }

    // Don't restart a playlist if it was just force stopped
    if ((Player::INSTANCE.GetForceStopped()) &&
        (Player::INSTANCE.GetOrigStartTime() == item.startTime) &&
        (Player::INSTANCE.GetForceStoppedPlaylist() == item.entry->playlist)) {
        SetItemRan(item, true);
        return false;
    }

    if (Player::INSTANCE.GetStatus() == FPP_STATUS_PLAYLIST_PLAYING) {
        // If we are playing, check to see if we should be playing something else

        // Check to see if we are already playing this item, for
        // instance if we reloaded the schedule while a scheduled
        // playlist was playing.
        if ((Player::INSTANCE.GetPlaylistName() == item.entry->playlist) &&
            (Player::INSTANCE.WasScheduled()) &&
            (Player::INSTANCE.GetRepeat() == item.entry->repeat) &&
            ((Player::INSTANCE.GetOrigStopTime() == item.endTime) ||
             (Player::INSTANCE.GetStopTime() == item.endTime)) &&
            (Player::INSTANCE.GetStopMethod() == item.entry->stopType)) {
            SetItemRan(item, true);
            return false;
        }

        if (!Player::INSTANCE.WasScheduled()) {
            // Manually started playlist is running

            // Check if playlist is protected from schedule override
            if (!Player::INSTANCE.GetAllowScheduleOverride()) {
                LogInfo(VB_SCHEDULE, "Manual playlist '%s' is protected from schedule override, not starting scheduled playlist '%s'\n",
                        Player::INSTANCE.GetPlaylistName().c_str(), item.entry->playlist.c_str());
                // Track this blocked scheduled item
                m_lastBlockedPlaylist = item.entry->playlist;
                m_lastBlockedTime = now;
                return false;
            }

            // Check priority
            if (Player::INSTANCE.GetPriority() > item.priority) {
                // Scheduled playlist has higher priority, stop the manual one
                LogInfo(VB_SCHEDULE, "Stopping manual playlist '%s' (priority %d) for scheduled playlist '%s' (priority %d)\n",
                        Player::INSTANCE.GetPlaylistName().c_str(), Player::INSTANCE.GetPriority(),
                        item.entry->playlist.c_str(), item.priority);
                while (Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) {
                    Player::INSTANCE.StopNow(1);
                }
            } else {
                // Manual playlist has higher or equal priority, let it continue
                LogDebug(VB_SCHEDULE, "Manual playlist '%s' (priority %d) has higher priority than scheduled playlist '%s' (priority %d), not overriding\n",
                         Player::INSTANCE.GetPlaylistName().c_str(), Player::INSTANCE.GetPriority(),
                         item.entry->playlist.c_str(), item.priority);
                return false;
            }
        } else if (Player::INSTANCE.GetPriority() > item.priority) {
            // Lower priority (higher number) playlist is running
            // Reset the ran status on the schedule entry for the
            // running playlist to false so it shows as 'next' again
            std::time_t oldStartTime = Player::INSTANCE.GetOrigStartTime();
            std::string playlistName = Player::INSTANCE.GetPlaylistName();
            if (auto it = m_scheduledItems.find(oldStartTime); it != m_scheduledItems.end()) {
                for (auto& oldItem : it->second) {
                    if ((oldItem.command == "Start Playlist") &&
                        (oldItem.entry->playlist == playlistName)) {
                        oldItem.ran = false;
                        m_forcedNextPlaylist = oldItem.entryIndex;
                    }
                }
            }

            int forceStop = 0;
            if (Player::INSTANCE.GetStopTime() < Player::INSTANCE.GetOrigStopTime()) {
                forceStop = 1;
            }

            // Stop whatever is playing, and the next time through this loop we'll
            switch (Player::INSTANCE.GetStopMethod()) {
            case 0:
                Player::INSTANCE.StopGracefully(forceStop);
                break;
            case 2:
                Player::INSTANCE.StopGracefully(forceStop, 1);
                break;
            case 1:
            default:
                Player::INSTANCE.StopNow(forceStop);
                break;
            }
        } else {
            // Need to wait for current Scheduled Higher-Priority
            // item to end until we can play multiple playlists
            // concurrently
            return false;
        }
    } else if (Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) {
        // We are either paused or stopping already so do nothing
        return false;
    }

    LogDebug(VB_SCHEDULE, "Starting Scheduled Playlist:\n");
    DumpScheduledItem(itemTime, item);

    int position = 0;
    if (restarted && (getSetting("resumePlaylist") == item.entry->playlist)) {
        position = getSettingInt("resumePosition");

        SetSetting("resumePlaylist", "");
        SetSetting("resumePosition", 0);
    }

    Player::INSTANCE.StartScheduledPlaylist(item.entry->playlist, position,
                                            item.entry->repeat, item.entryIndex,
                                            item.entryIndex, // priority is entry index for now
                                            item.startTime, item.endTime, item.entry->stopType);
    return true;
}

void Scheduler::CheckScheduledItems(bool restarted) {
    if (m_schedulerDisabled)
        return;

    std::time_t now = time(nullptr);

    for (auto& itemTime : m_scheduledItems) {
        if (itemTime.first > now) {
            doCountdown(now, itemTime.first, itemTime.second);
            break; // no need to look at items that are further in the future
        }

        for (auto& item : itemTime.second) {
            if (item.ran)
                continue; // skip over any items that ran already

            if (item.command == "Start Playlist") {
                if (!doScheduledPlaylist(now, itemTime.first, item, restarted)) {
                    continue;
                }
            } else {
                if (itemTime.first < now) {
                    SetItemRan(item, true);
                    continue; // skip any FPP Commands that are in the past
                }
                doScheduledCommand(itemTime.first, item);
            }

            SetItemRan(item, true);
        }
    }
}

void Scheduler::ClearScheduledItems() {
    m_scheduledItems.clear();
}

void Scheduler::SetItemRan(ScheduledItem& item, bool ran) {
    item.ran = ran;

    auto& vec = m_ranItems[item.startTime];
    auto it = std::find_if(vec.begin(), vec.end(), [&](const ScheduledItem& ranItem) {
        return item.command == ranItem.command &&
               item.startTime == ranItem.startTime &&
               item.endTime == ranItem.endTime &&
               item.args.size() == ranItem.args.size() &&
               (item.args.empty() || item.args[0].asString() == ranItem.args[0].asString());
    });
    if (it != vec.end()) {
        it->ran = ran;
    } else {
        vec.push_back(item);
    }
}

void Scheduler::LoadScheduleFromFile(void) {
    std::string SCHEDULE_FILE = FPP_DIR_CONFIG("/schedule.json");
    LogDebug(VB_SCHEDULE, "Loading Schedule from %s\n", SCHEDULE_FILE.c_str());

    m_loadSchedule = false;
    m_lastLoadDate = GetCurrentDateInt();
    m_schedulesExtendBeyondDistance = false;

    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);
    m_Schedule.clear();
    ClearScheduledItems();

    std::string playlistFile;

    Json::Value sch = LoadJsonFromFile(SCHEDULE_FILE);
    for (int i = 0; i < sch.size(); i++) {
        ScheduleEntry scheduleEntry;
        if (!scheduleEntry.LoadFromJson(sch[i]))
            continue;

        std::string warning = "Scheduled ";

        if (scheduleEntry.sequence) {
            playlistFile = FPP_DIR_SEQUENCE("/" + scheduleEntry.playlist);

            warning += "Sequence";
        } else {
            playlistFile = FPP_DIR_PLAYLIST("/" + scheduleEntry.playlist + ".json");

            warning += "Playlist";
        }

        warning += " '";
        warning += scheduleEntry.playlist + "' does not exist";

        if ((getFPPmode() != PLAYER_MODE) &&
            (scheduleEntry.enabled) &&
            (!scheduleEntry.playlist.empty())) {
            std::string warning = std::string("Playlist '") + scheduleEntry.playlist +
                                  "' was scheduled but system is running in Remote mode.";
            LogWarn(VB_SCHEDULE, "%s\n", warning.c_str());
            WarningHolder::AddWarning(warning);
            continue;
        }

        if ((scheduleEntry.enabled) &&
            (!scheduleEntry.playlist.empty()) &&
            (!FileExists(playlistFile))) {
            LogErr(VB_SCHEDULE, "ERROR: Scheduled %s '%s' does not exist\n",
                   scheduleEntry.sequence ? "Sequence" : "Playlist",
                   scheduleEntry.playlist.c_str());
            WarningHolder::AddWarning(warning);
            continue;
        } else {
            WarningHolder::RemoveWarning(warning);
        }

        m_Schedule.push_back(scheduleEntry);
    }

    for (int i = 0; i < m_Schedule.size(); i++) {
        AddScheduledItems(&m_Schedule[i], i);
    }

    SchedulePrint();

    if (WillLog(LOG_EXCESSIVE, VB_SCHEDULE))
        DumpScheduledItems();

    lock.unlock();

    return;
}

void Scheduler::SchedulePrint(void) {
    int i = 0;
    char stopTypes[4] = "GHL";
    std::string dayStr;

    if (m_schedulerDisabled)
        LogInfo(VB_SCHEDULE, "WARNING: Scheduler is currently disabled\n");

    LogInfo(VB_SCHEDULE, "Current Schedule: (Status: '+' = Enabled, '-' = Disabled, '!' = Outside Date Range, '*' = Repeat, Stop (G)raceful/(L)oop/(H)ard\n");
    LogInfo(VB_SCHEDULE, "Stat Start & End Dates       Days          Start & End Times   Playlist/Command\n");
    LogInfo(VB_SCHEDULE, "---- ----------------------- ------------- ------------------- ---------------------------------------------\n");
    for (i = 0; i < m_Schedule.size(); i++) {
        dayStr = GetDayTextFromDayIndex(m_Schedule[i].dayIndex);
        LogInfo(VB_SCHEDULE, "%c%c%c%c %04d-%02d-%02d - %04d-%02d-%02d %-13.13s %-8.8s %c %-8.8s %s\n",
                m_Schedule[i].enabled ? '+' : '-',
                CurrentDateInRange(m_Schedule[i].startDate, m_Schedule[i].endDate) ? ' ' : '!',
                m_Schedule[i].repeat ? '*' : ' ',
                stopTypes[m_Schedule[i].stopType],
                (int)(m_Schedule[i].startDate / 10000),
                (int)(m_Schedule[i].startDate % 10000 / 100),
                (int)(m_Schedule[i].startDate % 100),
                (int)(m_Schedule[i].endDate / 10000),
                (int)(m_Schedule[i].endDate % 10000 / 100),
                (int)(m_Schedule[i].endDate % 100),
                dayStr.c_str(),
                m_Schedule[i].startTimeStr.c_str(),
                m_Schedule[i].playlist.empty() ? ' ' : '-',
                m_Schedule[i].playlist.empty() ? "" : m_Schedule[i].endTimeStr.c_str(),
                m_Schedule[i].playlist.empty() ? m_Schedule[i].command.c_str() : m_Schedule[i].playlist.c_str());
    }

    LogDebug(VB_SCHEDULE, "//////////////////////////////////////////////////\n");
}

ScheduledItem* Scheduler::GetNextScheduledPlaylist() {
    std::time_t now = time(nullptr);
    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);

    for (auto& itemTime : m_scheduledItems) {
        for (auto& item : itemTime.second) {
            if (item.ran)
                continue; // skip over any items that ran already

            if (item.command == "Start Playlist")
                return &item;
        }
    }

    return nullptr;
}

std::string Scheduler::GetNextPlaylistName() {
    if (m_schedulerDisabled)
        return "Scheduler is disabled.";

    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);
    ScheduledItem* item = GetNextScheduledPlaylist();

    if (!item)
        return "No playlist scheduled.";

    return item->entry->playlist;
}

std::string Scheduler::GetNextPlaylistStartStr() {
    if (m_schedulerDisabled)
        return "";

    std::string timeFmt = getSetting("DateFormat") + " @ " + getSetting("TimeFormat");
    std::string result;
    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);
    ScheduledItem* item = GetNextScheduledPlaylist();

    if (!item)
        return result;

    char timeStr[32];
    std::time_t timeT = 0;
    struct tm timeStruct;
    time_t currTime = time(NULL);

    localtime_r(&item->startTime, &timeStruct);
    strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);

    // Thursday @ 11:00:00 - (Everyday)
    result = timeStr;
    result += " - (";
    result += GetDayTextFromDayIndex(item->entry->dayIndex);
    result += ")";

    return result;
}

std::string Scheduler::GetDayTextFromDayIndex(const int index) {
    if (index & INX_DAY_MASK) {
        char tmpStr[15];
        strcpy(tmpStr, "Mask: ");
        strcat(tmpStr, (index & INX_DAY_MASK_SUNDAY) ? "S" : "-");
        strcat(tmpStr, (index & INX_DAY_MASK_MONDAY) ? "M" : "-");
        strcat(tmpStr, (index & INX_DAY_MASK_TUESDAY) ? "T" : "-");
        strcat(tmpStr, (index & INX_DAY_MASK_WEDNESDAY) ? "W" : "-");
        strcat(tmpStr, (index & INX_DAY_MASK_THURSDAY) ? "T" : "-");
        strcat(tmpStr, (index & INX_DAY_MASK_FRIDAY) ? "F" : "-");
        strcat(tmpStr, (index & INX_DAY_MASK_SATURDAY) ? "S" : "-");
        return tmpStr;
    }

    switch (index) {
    case INX_SUN:
        return "Sunday";
    case INX_MON:
        return "Monday";
    case INX_TUE:
        return "Tuesday";
    case INX_WED:
        return "Wednesday";
    case INX_THU:
        return "Thursday";
    case INX_FRI:
        return "Friday";
    case INX_SAT:
        return "Saturday";
    case INX_EVERYDAY:
        return "Everyday";
    case INX_WKDAYS:
        return "Weekdays";
    case INX_WKEND:
        return "Weekends";
    case INX_M_W_F:
        return "Mon/Wed/Fri";
    case INX_T_TH:
        return "Tues-Thurs";
    case INX_SUN_TO_THURS:
        return "Sun-Thurs";
    case INX_FRI_SAT:
        return "Fri/Sat";
    case INX_ODD_DAY:
        return "Odd Days";
    case INX_EVEN_DAY:
        return "Even Days";
    }

    return "Error";
}

Json::Value Scheduler::GetInfo(void) {
    Json::Value result;

    std::string timeFmt = getSetting("DateFormat") + " @ " + getSetting("TimeFormat");
    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);
    Json::Value np;
    ScheduledItem* nextItem = GetNextScheduledPlaylist();
    np["playlistName"] = GetNextPlaylistName();
    np["scheduledStartTime"] = nextItem ? (Json::UInt64)(nextItem->startTime) : 0;
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
            strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);
            cp["currentTime"] = (Json::UInt64)currTime;
            cp["currentTimeStr"] = timeStr;

            timeT = Player::INSTANCE.GetOrigStartTime();
            localtime_r(&timeT, &timeStruct);
            strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);
            cp["scheduledStartTime"] = (Json::UInt64)timeT;
            cp["scheduledStartTimeStr"] = timeStr;

            timeT = Player::INSTANCE.GetStartTime();
            localtime_r(&timeT, &timeStruct);
            strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);
            cp["actualStartTime"] = (Json::UInt64)timeT;
            cp["actualStartTimeStr"] = timeStr;

            timeT = Player::INSTANCE.GetOrigStopTime();
            localtime_r(&timeT, &timeStruct);
            strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);
            cp["scheduledEndTime"] = (Json::UInt64)timeT;
            cp["scheduledEndTimeStr"] = timeStr;

            timeT = Player::INSTANCE.GetStopTime();
            localtime_r(&timeT, &timeStruct);
            strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);
            cp["actualEndTime"] = (Json::UInt64)timeT;
            cp["actualEndTimeStr"] = timeStr;
            cp["secondsRemaining"] = (int)(timeT - currTime);

            int stopType = Player::INSTANCE.GetStopMethod();
            cp["stopType"] = stopType;
            cp["stopTypeStr"] = stopType == 2 ? "Graceful Loop" : stopType == 1 ? "Hard"
                                                                                : "Graceful";

            result["status"] = "playing";
        } else {
            result["status"] = "manual";
        }

        cp["playlistName"] = Player::INSTANCE.GetPlaylistName();

        result["currentPlaylist"] = cp;
    } else {
        result["status"] = "idle";
    }

    // Add blocked schedule information
    if (m_lastBlockedTime > 0) {
        time_t now = time(NULL);
        // Only report if blocked within last 5 minutes
        if ((now - m_lastBlockedTime) < 300) {
            result["blockedSchedule"]["playlistName"] = m_lastBlockedPlaylist;
            result["blockedSchedule"]["blockedTime"] = (Json::UInt64)m_lastBlockedTime;
        }
    }

    return result;
}

Json::Value Scheduler::GetSchedule() {
    std::string timeFmt = getSetting("DateFormat") + " @ " + getSetting("TimeFormat");
    Json::Value result;
    Json::Value entries(Json::arrayValue);
    Json::Value entry;
    Json::Value items(Json::arrayValue);
    Json::Value scheduledItem;
    std::time_t now = time(nullptr);
    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);

    result["enabled"] = m_schedulerDisabled ? 0 : 1;

    for (int i = 0; i < m_Schedule.size(); i++) {
        entry = m_Schedule[i].GetJson();
        entry["id"] = i;
        entry["type"] = m_Schedule[i].playlist.empty() ? "command" : "playlist";
        entry["dayStr"] = GetDayTextFromDayIndex(m_Schedule[i].dayIndex);

        entries.append(entry);
    }
    result["entries"] = entries;

    struct tm timeStruct;
    char timeStr[32];
    time_t tmpTime;

    if ((Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) &&
        (Player::INSTANCE.WasScheduled())) {
        // If we are playing a scheduled playlist, then insert an item for
        // it since it is already marked as ran and will get skipped by.
        // We do this here instead of just using a more complicated if continue
        // statement below because the player may have more up to date info
        // on the end time than our old schedule entry if the end time has
        // been extended or shortened.
        scheduledItem["priority"] = Player::INSTANCE.GetPriority();
        scheduledItem["id"] = Player::INSTANCE.GetScheduleEntry();

        tmpTime = Player::INSTANCE.GetOrigStartTime();
        localtime_r(&tmpTime, &timeStruct);
        strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);
        scheduledItem["startTimeStr"] = timeStr;
        scheduledItem["startTime"] = (Json::UInt64)tmpTime;

        strftime(timeStr, 32, "%Y%m%d", &timeStruct);
        scheduledItem["startDateStr"] = timeStr;
        scheduledItem["startDateInt"] = atoi(timeStr);

        tmpTime = Player::INSTANCE.GetStopTime();
        localtime_r(&tmpTime, &timeStruct);
        strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);
        scheduledItem["endTimeStr"] = timeStr;
        scheduledItem["endTime"] = (Json::UInt64)tmpTime;

        strftime(timeStr, 32, "%Y%m%d", &timeStruct);
        scheduledItem["endDateStr"] = timeStr;
        scheduledItem["endDateInt"] = atoi(timeStr);

        scheduledItem["command"] = "Start Playlist";

        Json::Value args(Json::arrayValue);
        args.append(Player::INSTANCE.GetPlaylistName());
        scheduledItem["args"] = args;

        items.append(scheduledItem);
    }

    for (auto& itemTime : m_scheduledItems) {
        for (auto& item : itemTime.second) {
            if (item.ran)
                continue;

            scheduledItem["priority"] = item.priority;
            scheduledItem["id"] = item.entryIndex;

            localtime_r(&item.startTime, &timeStruct);
            strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);
            scheduledItem["startTimeStr"] = timeStr;
            scheduledItem["startTime"] = (Json::UInt64)item.startTime;

            strftime(timeStr, 32, "%Y%m%d", &timeStruct);
            scheduledItem["startDateStr"] = timeStr;
            scheduledItem["startDateInt"] = atoi(timeStr);

            localtime_r(&item.endTime, &timeStruct);
            strftime(timeStr, 32, timeFmt.c_str(), &timeStruct);
            scheduledItem["endTimeStr"] = timeStr;
            scheduledItem["endTime"] = (Json::UInt64)item.endTime;

            strftime(timeStr, 32, "%Y%m%d", &timeStruct);
            scheduledItem["endDateStr"] = timeStr;
            scheduledItem["endDateInt"] = atoi(timeStr);

            scheduledItem["command"] = item.command;

            scheduledItem["args"] = item.args;
            scheduledItem["multisyncCommand"] = item.entry->multisyncCommand;
            scheduledItem["multisyncHosts"] = item.entry->multisyncHosts;

            items.append(scheduledItem);
        }
    }
    result["items"] = items;
    result["schedulesExtendBeyondDistance"] = m_schedulesExtendBeyondDistance;
    result["scheduleDistance"] = getSettingInt("ScheduleDistance");

    return result;
}

void Scheduler::SetTimeDelta(int delta, int timeLimit) {
    m_timeDelta = m_timeDelta + delta;
    m_timeDeltaThreshold = time(NULL) + timeLimit;
    // Trigger a reload the next time Scheduler::ScheduleProc() is called
    m_loadSchedule = true;
}

bool Scheduler::StartNextScheduledItemNow() {
    std::unique_lock<std::recursive_mutex> lock(m_scheduleLock);

    ScheduledItem* item = GetNextScheduledPlaylist();

    if (item) {
        if (Player::INSTANCE.GetStatus() != FPP_STATUS_IDLE) {
            LogDebug(VB_SCHEDULE, "Force-Stopping running playlist\n");
            Player::INSTANCE.StopNow(1);
        }

        LogDebug(VB_SCHEDULE, "Force-Starting next scheduled playlist now:\n");
        DumpScheduledItem(item->startTime, *item);

        time_t now = time(NULL);
        Player::INSTANCE.StartScheduledPlaylist(item->entry->playlist, 0,
                                                item->entry->repeat, item->entryIndex,
                                                item->entryIndex, // priority is entry index for now
                                                now, item->endTime, item->entry->stopType);
        return true;
    }

    return false;
}

class ScheduleCommand : public Command {
public:
    ScheduleCommand(const std::string& str, Scheduler* s) :
        Command(str),
        sched(s) {}
    virtual ~ScheduleCommand() {}

    Scheduler* sched;
};

class StartNextItemCommand : public ScheduleCommand {
public:
    StartNextItemCommand(Scheduler* s) :
        ScheduleCommand("Start Next Scheduled Item", s) {
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (sched->StartNextScheduledItemNow())
            return std::make_unique<Command::Result>("Next scheduled item started");
        else
            return std::make_unique<Command::Result>("Unable to start next scheduled item");
    }
};

class ExtendScheduleCommand : public ScheduleCommand {
public:
    ExtendScheduleCommand(Scheduler* s) :
        ScheduleCommand("Extend Schedule", s) {
        args.push_back(CommandArg("Seconds", "int", "Extend for Seconds").setRange(-12 * 60 * 60, 12 * 60 * 60).setDefaultValue("300").setAdjustable());
        args.push_back(CommandArg("Limit", "int", "For schedule events occurring in the next x Seconds").setRange(-12 * 60 * 60, 12 * 60 * 60).setDefaultValue("0").setAdjustable());
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() < 1) {
            return std::make_unique<Command::ErrorResult>("Command needs at least 1 argument, found " + std::to_string(args.size()));
        }
        if (Player::INSTANCE.AdjustPlaylistStopTime(std::stoi(args[0]))) {
            if (args.size() > 1) {
                if (std::stoi(args[1]) != 0) {
                    scheduler->SetTimeDelta(std::stoi(args[0]), std::stoi(args[1]));
                }
            }
            return std::make_unique<Command::Result>("Schedule Updated");
        }
        return std::make_unique<Command::Result>("Error extending Schedule");
    }
};

void Scheduler::RegisterCommands() {
    CommandManager::INSTANCE.addCommand(new StartNextItemCommand(this));
    CommandManager::INSTANCE.addCommand(new ExtendScheduleCommand(this));
}
