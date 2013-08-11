#ifndef _SCHEDULE_H
#define _SCHEDULE_H

#define MAX_SCHEDULE_ENTRIES  64 
#define DAY_EVERYDAY          7 
#define SECONDS_PER_MINUTE    60
#define SECONDS_PER_HOUR      3600
#define SECONDS_PER_DAY       86400
#define SECONDS_PER_WEEK      604800

typedef struct{
		char enable;
		char playList[128];
    char startDay;
    char startHour;
		char startMinute;
    char endDay;
    char endHour;
		char endMinute;
		char repeat;
		int startSecond;
    int startWeeklySecond;
		int endSecond;
    int endWeeklySecond;
}ScheduleEntry;

void ScheduleProc();
void PlayListLoadCheck();
void PlayListStopCheck();
void LoadNextScheduleInfo();
int GetStartSecond(int startSecond1, int startSecond2, int day);
int GetEndSecond(int endSecond1, int endSecond2, int day);
void LoadScheduleFromFile();
void SchedulePrint();
int GetNextScheduleEntry();
int GetWeeklySeconds(int day, int hour, int minute, int second);
int GetWeeklySecondDifference(int weeklySeconds1, int weeklySeconds2, int day);
void GetNextScheduleStartText(char * txt);
void GetNextPlaylistText(char * txt);
void GetDayTextFromScheduleEntry(int index,char * txt);



#endif
