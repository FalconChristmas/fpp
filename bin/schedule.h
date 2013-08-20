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
    char dayIndex;
    char startHour;
		char startMinute;
		int startSecond;
    char endHour;
		char endMinute;
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


#endif
