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
#include <mutex>
#include <vector>

#include <pthread.h>
#include <time.h>

#include "ScheduleEntry.h"

#define SCHEDULE_INDEX_INVALID -1

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

class ScheduledItem {
public:
    ScheduledItem();
    ScheduledItem(ScheduledItem* item);
    ~ScheduledItem() {}

    int priority;
    ScheduleEntry* entry;
    int entryIndex;
    bool ran;
    time_t startTime;
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

    std::string GetPlaylistThatShouldBePlaying(int& repeat);
    Json::Value GetInfo(void);
    Json::Value GetSchedule(void);

private:
    void AddScheduledItems(ScheduleEntry* entry, int index);
    void DumpScheduledItem(std::time_t itemTime, ScheduledItem* item);
    void DumpScheduledItems();
    void CheckScheduledItems(bool restarted = false);
    void ClearScheduledItems();
    void SetItemRan(ScheduledItem* item, bool ran);

    ScheduledItem* GetNextScheduledPlaylist();
    void LoadScheduleFromFile(void);
    void SchedulePrint(void);
    std::string GetDayTextFromDayIndex(const int index);

    void RegisterCommands();

    bool m_schedulerDisabled;
    bool m_loadSchedule;
    int m_lastLoadDate;
    int m_timeDelta;
    time_t m_timeDeltaThreshold;

    time_t m_lastProcTime;

    std::recursive_mutex m_scheduleLock;
    std::vector<ScheduleEntry> m_Schedule;
    std::map<std::time_t, std::vector<ScheduledItem*>*> m_scheduledItems;
    std::map<std::time_t, std::vector<ScheduledItem*>*> m_oldItems;
    std::map<std::time_t, std::vector<ScheduledItem*>*> m_ranItems;

    int m_forcedNextPlaylist;
};

extern Scheduler* scheduler;
