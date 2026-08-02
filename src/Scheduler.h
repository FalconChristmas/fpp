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

#include <map>
#include "fpp-json.h"
#include <mutex>
#include <vector>

#include <pthread.h>
#include <time.h>

#include "ScheduleEntry.h"

#define SCHEDULE_INDEX_INVALID -1

// The INX_* day index and day mask defines now live in ScheduleEntry.h, which
// is included above, since they describe a ScheduleEntry's dayIndex field.
// They remain visible to anything including this header.

class ScheduledItem {
public:
    ScheduleEntry* const entry;
    int const priority;
    int const entryIndex;
    bool ran;
    time_t const startTime;
    time_t endTime;
    std::string command;
    Json::Value args;
};

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    void ScheduleProc(void);
    void CheckIfShouldBePlayingNow(int ignoreRepeat = 0, int forceStopped = -1);
    void ReloadScheduleFile(void);
    void SetTimeDelta(int delta, int timeLimit);
    bool StartNextScheduledItemNow();

    std::string GetNextPlaylistName();
    std::string GetNextPlaylistStartStr();
    bool IsEnabled() const { return !m_schedulerDisabled; }

    std::string GetPlaylistThatShouldBePlaying(int& repeat);
    Json::Value GetInfo(void);
    Json::Value GetSchedule(void);

    // One expanded occurrence of a schedule entry, used while building the
    // calendar range response.  Kept lightweight so a range holding tens of
    // thousands of occurrences can be collected and aggregated before any of
    // the much more expensive JSON is built.
    struct Occurrence {
        int entryIndex;
        int dayKey; // YYYYMMDD of the local day the occurrence starts on
        time_t start;
        time_t end;
        bool overridden;
    };

    Json::Value GetScheduleRange(time_t rangeStart, time_t rangeEnd,
                                 bool includeDisabled, bool daySummary);

private:
    Json::Value BuildScheduleRangeItem(const ScheduleEntry& entry,
                                       const Occurrence& occurrence,
                                       const std::string& timeFmt);

    void AddScheduledItems(ScheduleEntry* entry, int index);
    void DumpScheduledItem(const std::time_t itemTime, const ScheduledItem& item);
    void DumpScheduledItems();
    void CheckScheduledItems(bool restarted = false);
    void ClearScheduledItems();
    void SetItemRan(ScheduledItem& item, bool ran);

    ScheduledItem* GetNextScheduledPlaylist();
    void LoadScheduleFromFile(void);
    void SchedulePrint(void);
    std::string GetDayTextFromDayIndex(const int index);

    void RegisterCommands();

    void doCountdown(const std::time_t now, const std::time_t itemTime, const std::vector<ScheduledItem>& items);
    void doScheduledCommand(const std::time_t itemTime, const ScheduledItem& item);
    bool doScheduledPlaylist(const std::time_t now, const std::time_t itemTime, ScheduledItem& item, bool restarted);

    bool m_schedulerDisabled;
    bool m_loadSchedule;
    int m_lastLoadDate;
    int m_timeDelta;
    time_t m_timeDeltaThreshold;

    time_t m_lastProcTime;

    std::recursive_mutex m_scheduleLock;
    std::vector<ScheduleEntry> m_Schedule;
    std::map<std::time_t, std::vector<ScheduledItem>> m_scheduledItems;
    std::map<std::time_t, std::vector<ScheduledItem>> m_oldItems;
    std::map<std::time_t, std::vector<ScheduledItem>> m_ranItems;

    int m_forcedNextPlaylist;

    std::string m_lastBlockedPlaylist;
    std::time_t m_lastBlockedTime;

    bool m_schedulesExtendBeyondDistance;
};

extern Scheduler* scheduler;
