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

    // A player start/stop the scheduler decided on while holding
    // m_scheduleLock, carried by value so it can be run after the lock is
    // released.  See the m_scheduleLock declaration below.
    struct PlayerAction {
        enum class Type {
            StopUntilIdle,
            StopNow,
            StopGracefully,
            StopGracefullyAfterLoop,
            StartPlaylist
        };

        Type type;
        int forceStop = 0;
        std::string playlist;
        int position = 0;
        int repeat = 0;
        int entryIndex = 0;
        int priority = 0;
        time_t startTime = 0;
        time_t endTime = 0;
        int stopType = 0;
    };

    // A countdown preset to trigger once the lock is released.  Presets run
    // their commands synchronously, so they are subject to the same rule as
    // PlayerAction.
    struct CountdownPreset {
        std::string name;
        std::string playlist;
    };

    void RunPlayerActions(const std::vector<PlayerAction>& actions);
    void RunCountdownPresets(const std::vector<CountdownPreset>& presets);

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

    void doCountdown(const std::time_t now, const std::time_t itemTime, const std::vector<ScheduledItem>& items,
                     std::vector<CountdownPreset>& presets);
    void doScheduledCommand(const std::time_t itemTime, const ScheduledItem& item);
    bool doScheduledPlaylist(const std::time_t now, const std::time_t itemTime, ScheduledItem& item, bool restarted,
                             std::vector<PlayerAction>& actions);

    bool m_schedulerDisabled;
    bool m_loadSchedule;
    int m_lastLoadDate;
    int m_timeDelta;
    time_t m_timeDeltaThreshold;

    time_t m_lastProcTime;

    // Lock ordering: m_scheduleLock may be taken while holding no other lock,
    // or from inside Playlist's m_playlistMutex (a playlist entry can run an
    // FPP Command inline, and several of those call back into the scheduler).
    // That makes m_playlistMutex -> m_scheduleLock the established order, so
    // scheduler code must never start or stop the Player/Playlist while
    // holding m_scheduleLock - doing so takes the two locks in the opposite
    // order and deadlocks against a playlist thread.  Both mutexes are
    // recursive, which does not help across threads.  Decide under the lock,
    // copy out what the call needs, release, then call the Player (see
    // CheckScheduledItems).  The read-only Player/Playlist getters do not take
    // m_playlistMutex and are safe to call under this lock.
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
