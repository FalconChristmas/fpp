/*
 *   Scheduler class for the Falcon Player (FPP) 
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

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include <mutex>
#include <vector>

#include <pthread.h>

#include <jsoncpp/json/json.h>

#include "ScheduleEntry.h"

#define SCHEDULE_INDEX_INVALID  -1
#define SECONDS_PER_MINUTE    60
#define SECONDS_PER_HOUR      3600
#define SECONDS_PER_DAY       86400
#define SECONDS_PER_WEEK      604800
#define DAYS_PER_WEEK         7

#define INX_SUN                     0
#define INX_MON                     1
#define INX_TUE                     2
#define INX_WED                     3
#define INX_THU                     4
#define INX_FRI                     5
#define INX_SAT                     6
#define INX_EVERYDAY                7
#define INX_WKDAYS                  8
#define INX_WKEND                   9
#define INX_M_W_F                   10
#define INX_T_TH                    11
#define INX_SUN_TO_THURS            12
#define INX_FRI_SAT                 13
#define INX_ODD_DAY                 14
#define INX_EVEN_DAY                15
#define INX_DAY_MASK                0x10000
#define INX_DAY_MASK_SUNDAY         0x04000
#define INX_DAY_MASK_MONDAY         0x02000
#define INX_DAY_MASK_TUESDAY        0x01000
#define INX_DAY_MASK_WEDNESDAY      0x00800
#define INX_DAY_MASK_THURSDAY       0x00400
#define INX_DAY_MASK_FRIDAY         0x00200
#define INX_DAY_MASK_SATURDAY       0x00100


class SchedulePlaylistDetails {
public:
    SchedulePlaylistDetails() {}
    ~SchedulePlaylistDetails() {}

	void SetTimes(time_t currTime, int nowWeeklySeconds);

	ScheduleEntry entry;

	int ScheduleEntryIndex = 0;
	int weeklySecondIndex = 0;
	int startWeeklySeconds = 0;
	int endWeeklySeconds = 0;

	int    actualStartWeeklySeconds = 0;
	time_t actualStartTime = 0;
	time_t actualEndTime = 0;
	time_t scheduledStartTime = 0;
	time_t scheduledEndTime = 0;
};

class Scheduler {
  public:
	Scheduler();
	~Scheduler();

	void ScheduleProc(void);
	void CheckIfShouldBePlayingNow(int ignoreRepeat = 0);
	void ReloadScheduleFile(void);
	void ReLoadCurrentScheduleInfo(void);
	void ReLoadNextScheduleInfo(void);
	void GetNextScheduleStartText(char * txt);
	void GetNextPlaylistText(char * txt);

	std::string GetPlaylistThatShouldBePlaying(int &repeat);
	Json::Value GetInfo(void);

	int  ExtendRunningSchedule(int seconds = 300);

  private:
	int  GetNextScheduleEntry(int *weeklySecondIndex, bool future);
	void LoadCurrentScheduleInfo(bool future = false);
	void LoadNextScheduleInfo(void);
	void GetSunInfo(int set, int &hour, int &minute, int &second);
	void SetScheduleEntrysWeeklyStartAndEndSeconds(ScheduleEntry *entry);
	void PlayListLoadCheck(void);
	void PlayListStopCheck(void);
	void ConvertScheduleFile(void);
	void LoadScheduleFromFile(void);
	void SchedulePrint(void);
	std::string GetWeekDayStrFromSeconds(int weeklySeconds);
	int  GetWeeklySeconds(int day, int hour, int minute, int second);
	int  GetWeeklySecondDifference(int weeklySeconds1, int weeklySeconds2);
	int  GetDayFromWeeklySeconds(int weeklySeconds);
	int  FindNextStartingScheduleIndex(void);
	void GetScheduleEntryStartText(int index,int weeklySecondIndex, char * txt);
	void GetDayTextFromDayIndex(int index,char * txt);

	void RegisterCommands();

	bool          m_loadSchedule;
	unsigned char m_CurrentScheduleHasbeenLoaded;
	unsigned char m_NextScheduleHasbeenLoaded;
	int           m_nowWeeklySeconds2;
	int           m_lastLoadDate;

	time_t        m_lastProcTime;
	time_t        m_lastLoadTime;
	time_t        m_lastCalculateTime;

	pthread_t     m_threadID;
	int           m_runThread;
	int           m_threadIsRunning;

	std::mutex                  m_scheduleLock;
	std::vector<ScheduleEntry>  m_Schedule;

	SchedulePlaylistDetails m_currentSchedulePlaylist;
	SchedulePlaylistDetails m_nextSchedulePlaylist;
};

extern Scheduler *scheduler;

#endif /* _SCHEDULER_H */

