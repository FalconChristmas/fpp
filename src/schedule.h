/*
 *   Schedule handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#define SCHEDULE_INDEX_INVALID  -1
#define MAX_SCHEDULE_ENTRIES  64 
#define SECONDS_PER_MINUTE    60
#define SECONDS_PER_HOUR      3600
#define SECONDS_PER_DAY       86400
#define SECONDS_PER_WEEK      604800
#define DAYS_PER_WEEK         7

#define	INX_SUN							0
#define	INX_MON							1
#define	INX_TUE							2
#define	INX_WED							3
#define	INX_THU							4
#define	INX_FRI							5
#define	INX_SAT							6
#define	INX_EVERYDAY				7
#define	INX_WKDAYS					8
#define	INX_WKEND						9
#define	INX_M_W_F						10
#define	INX_T_TH						11
#define	INX_SUN_TO_THURS		12
#define	INX_FRI_SAT					13

typedef struct{
		char enable;
		char playList[128];
    int dayIndex;
    int startHour;
		int startMinute;
		int startSecond;
    int endHour;
		int endMinute;
		int endSecond;
		char repeat;
    int weeklySecondCount;
    int weeklyStartSeconds[DAYS_PER_WEEK];
    int weeklyEndSeconds[DAYS_PER_WEEK];
}ScheduleEntry;

typedef struct{
  int ScheduleEntryIndex;
  int weeklySecondIndex;
	int startWeeklySeconds;
	int endWeeklySeconds;
}SchedulePlaylistDetails;

void ReLoadCurrentScheduleInfo();
void LoadCurrentScheduleInfo();
void LoadNextScheduleInfo();
void PlayListLoadCheck();
void PlayListStopCheck();
void LoadScheduleFromFile();
void SchedulePrint();
void GetNextScheduleStartText(char * txt);
void GetNextPlaylistText(char * txt);
void GetScheduleEntryStartText(int index,int weeklySecondIndex, char * txt);
void GetDayTextFromDayIndex(int index,char * txt);
void CheckIfShouldBePlayingNow();
int GetNextScheduleEntry(int *weeklySecondIndex);
int GetWeeklySeconds(int day, int hour, int minute, int second);
int GetWeeklySecondDifference(int weeklySeconds1, int weeklySeconds2);
void ScheduleProc();


#endif
