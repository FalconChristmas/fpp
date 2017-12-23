/*
 *   Scheduler class for the Falcon Player (FPP) 
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

#include <vector>

#include <pthread.h>

#include "ScheduleEntry.h"

#define SCHEDULE_INDEX_INVALID  -1
#define MAX_SCHEDULE_ENTRIES  64 
#define SECONDS_PER_MINUTE    60
#define SECONDS_PER_HOUR      3600
#define SECONDS_PER_DAY       86400
#define SECONDS_PER_WEEK      604800
#define DAYS_PER_WEEK         7

#define INX_SUN                         0
#define INX_MON                         1
#define INX_TUE                         2
#define INX_WED                         3
#define INX_THU                         4
#define INX_FRI                         5
#define INX_SAT                         6
#define INX_EVERYDAY                7
#define INX_WKDAYS                  8
#define INX_WKEND                       9
#define INX_M_W_F                       10
#define INX_T_TH                        11
#define INX_SUN_TO_THURS        12
#define INX_FRI_SAT                 13
#define INX_DAY_MASK                0x10000
#define INX_DAY_MASK_SUNDAY         0x04000
#define INX_DAY_MASK_MONDAY         0x02000
#define INX_DAY_MASK_TUESDAY        0x01000
#define INX_DAY_MASK_WEDNESDAY      0x00800
#define INX_DAY_MASK_THURSDAY       0x00400
#define INX_DAY_MASK_FRIDAY         0x00200
#define INX_DAY_MASK_SATURDAY       0x00100

typedef struct{
	char enable;
	char playList[128];
	int  dayIndex;
	int  startHour;
	int  startMinute;
	int  startSecond;
	int  endHour;
	int  endMinute;
	int  endSecond;
	char repeat;
	int  weeklySecondCount;
	int  weeklyStartSeconds[DAYS_PER_WEEK];
	int  weeklyEndSeconds[DAYS_PER_WEEK];
	int  startDate; // YYYYMMDD format as an integer
	int  endDate;   // YYYYMMDD format as an integer
} ScheduleEntryStruct;

typedef struct {
	int ScheduleEntryIndex;
	int weeklySecondIndex;
	int startWeeklySeconds;
	int endWeeklySeconds;
} SchedulePlaylistDetails;


class Scheduler {
  public:
	Scheduler();
	~Scheduler();

	void ScheduleProc(void);
	void CheckIfShouldBePlayingNow(void);
	void ReLoadCurrentScheduleInfo(void);
	void ReLoadNextScheduleInfo(void);
	void GetNextScheduleStartText(char * txt);
	void GetNextPlaylistText(char * txt);

  private:
	int  GetNextScheduleEntry(int *weeklySecondIndex);
	void LoadCurrentScheduleInfo(void);
	void LoadNextScheduleInfo(void);
	void SetScheduleEntrysWeeklyStartAndEndSeconds(ScheduleEntryStruct * entry);
	void PlayListLoadCheck(void);
	void PlayListStopCheck(void);
	void LoadScheduleFromFile(void);
	void SchedulePrint(void);
	int  GetWeeklySeconds(int day, int hour, int minute, int second);
	int  GetWeeklySecondDifference(int weeklySeconds1, int weeklySeconds2);
	int  GetDayFromWeeklySeconds(int weeklySeconds);
	int  FindNextStartingScheduleIndex(void);
	void GetScheduleEntryStartText(int index,int weeklySecondIndex, char * txt);
	void GetDayTextFromDayIndex(int index,char * txt);


	int           m_ScheduleEntryCount;
	unsigned char m_CurrentScheduleHasbeenLoaded;
	unsigned char m_NextScheduleHasbeenLoaded;
	int           m_nowWeeklySeconds2;
	int           m_lastLoadDate;

	time_t        m_lastLoadTime;
	time_t        m_lastCalculateTime;

	pthread_t     m_threadID;
	int           m_runThread;
	int           m_threadIsRunning;

	pthread_mutex_t             m_scheduleLock;
	std::vector<ScheduleEntry>  m_schedule;

	ScheduleEntryStruct     m_Schedule[MAX_SCHEDULE_ENTRIES];
	SchedulePlaylistDetails m_currentSchedulePlaylist;
	SchedulePlaylistDetails m_nextSchedulePlaylist;
};

extern Scheduler *scheduler;

#endif /* _SCHEDULER_H */

