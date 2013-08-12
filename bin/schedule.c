#include "fpp.h"
#include "schedule.h"
#include "command.h"
#include "playList.h"
#include "mpg123.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>



char * scheduleFile = "/home/pi/media/schedule";
ScheduleEntry Schedule[MAX_SCHEDULE_ENTRIES];
int ScheduleEntryCount=0;

int CurrentScheduleEntryIndex=0;
int CurrentScheduleStartSecond=0;
int CurrentScheduleEndSecond=0;

unsigned char NextScheduleInfoHasbeenLoaded=0;

extern int FPPstatus;
extern PlaylistDetails playlistDetails;

extern char logText[256];

int nowWeeklySeconds2;

void ScheduleProc()
{
  switch(FPPstatus)
  {
    case FPP_STATUS_IDLE:
      if(!NextScheduleInfoHasbeenLoaded)
      {
        LoadNextScheduleInfo();
      }
      PlayListLoadCheck();
      break;
    case FPP_STATUS_PLAYLIST_PLAYING:
      PlayListStopCheck();
      break;
    default:
      break;

  }
}

void PlayListLoadCheck()
{
  time_t currTime = time(NULL);
  struct tm *now = localtime(&currTime);
  int nowWeeklySeconds = GetWeeklySeconds(now->tm_wday, now->tm_hour, now->tm_min, now->tm_sec);
  if (nowWeeklySeconds2 != nowWeeklySeconds)
  {
    nowWeeklySeconds2 = nowWeeklySeconds;
    sprintf(logText,"NowSecs = %d CurrStartSecs=%d\n",nowWeeklySeconds,CurrentScheduleStartSecond);
    //LogWrite(logText);
  }
  if(nowWeeklySeconds == CurrentScheduleStartSecond)
  {
    NextScheduleInfoHasbeenLoaded = 0;
    strcpy(playlistDetails.currentPlaylistFile,Schedule[CurrentScheduleEntryIndex].playList);
		playlistDetails.currentPlaylistEntry=0;
		playlistDetails.playlistStarting=1;
		if (playlistDetails.currentPlaylistFile,Schedule[CurrentScheduleEntryIndex].repeat == 0)
		{
	    FPPstatus = FPP_STATUS_STOPPING_GRACEFULLY;
		}
		else
		{
	    FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
		}
  }
}

void PlayListStopCheck()
{
  time_t currTime = time(NULL);
  struct tm *now = localtime(&currTime);
  int nowWeeklySeconds = GetWeeklySeconds(now->tm_wday, now->tm_hour, now->tm_min, now->tm_sec);

  if (nowWeeklySeconds2 != nowWeeklySeconds)
  {
    nowWeeklySeconds2 = nowWeeklySeconds;
    sprintf(logText,"NowSecs = %d CurrEndSecs=%d\n",nowWeeklySeconds,CurrentScheduleEndSecond);
    //LogWrite(logText);
  }

  if(nowWeeklySeconds == CurrentScheduleEndSecond)
  {
    StopPlaylistGracefully();
  }
}

void LoadNextScheduleInfo()
{
  time_t currTime = time(NULL);
  struct tm *now = localtime(&currTime);
  LoadScheduleFromFile();
  int nowWeeklySeconds = GetWeeklySeconds(now->tm_wday, now->tm_hour, now->tm_min, now->tm_sec);

  CurrentScheduleEntryIndex = GetNextScheduleEntry();
  CurrentScheduleStartSecond = GetStartSecond(nowWeeklySeconds,Schedule[CurrentScheduleEntryIndex].startWeeklySecond, Schedule[CurrentScheduleEntryIndex].startDay);

  sprintf(logText,"CurrentScheduleEntryIndex=%d , CurrStartSecs=%d\n",CurrentScheduleEntryIndex,CurrentScheduleStartSecond);
  //LogWrite(logText);

  CurrentScheduleEndSecond = GetEndSecond(nowWeeklySeconds,Schedule[CurrentScheduleEntryIndex].endWeeklySecond, Schedule[CurrentScheduleEntryIndex].endDay);
  NextScheduleInfoHasbeenLoaded = 1;
}

int GetStartSecond(int startSecond1, int startSecond2, int day)
{
  int i,second;
  if(day==DAY_EVERYDAY)
  {
    if(startSecond1 > (startSecond2 + (SECONDS_PER_DAY * 6)))
    {
      second = startSecond2;
      sprintf(logText,"1 startSecond1=%d , CurrStartSecs=%d\n",startSecond1,second);
      //LogWrite(logText);
    }
    else
    {
      for(i=0;i<6;i++)
      {
        if(startSecond1 < (startSecond2 + (SECONDS_PER_DAY*i)))
        {
          second = startSecond2 + (SECONDS_PER_DAY * i);
          sprintf(logText,"i= %d, 2 startSecond1=%d , CurrStartSecs=%d\n",i,startSecond1,second);
          //LogWrite(logText);
          break;
        }
      }
    }
  }
  else
  {
    second = startSecond2;
    sprintf(logText,"3 startSecond1=%d , CurrStartSecs=%d\n",startSecond1,second);
    //LogWrite(logText);
  }
  return second;
}

int GetEndSecond(int endSecond1, int endSecond2, int day)
{
  int i,second;
  if(day==DAY_EVERYDAY)
  {
    if(endSecond1 > (endSecond2 + (SECONDS_PER_DAY * 6)))
    {
      second = endSecond2;
    }
    else
    {
      for(i=0;i<6;i++)
      {
        if(endSecond1 < (endSecond2 + (SECONDS_PER_DAY*i)))
        {
          second = endSecond2 + (SECONDS_PER_DAY * i);
          break;
        }
      }
    }
  }
  else
  {
    second = endSecond2;
  }
  return second;
}


void LoadScheduleFromFile()
{
  FILE *fp;
  char buf[512];
  char *s;
  ScheduleEntryCount=0;
  int day;
  NextScheduleInfoHasbeenLoaded = 0;
  sprintf(logText,"Opening File Now %s\n",scheduleFile);
 // LogWrite(logText);
  fp = fopen(scheduleFile, "r");
  if (fp == NULL) 
  {
		return;
  }
  while(fgets(buf, 512, fp) != NULL)
  {
    // Enable
    s=strtok(buf,",");
    Schedule[ScheduleEntryCount].enable = atoi(s);
    // Playlist Name
    s=strtok(NULL,",");
    strcpy(Schedule[ScheduleEntryCount].playList,s);
    // Start Day
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].startDay = atoi(s);
    // Start Hour
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].startHour = atoi(s);
    // Start Minute
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].startMinute = atoi(s);
    // Start Second
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].startSecond = atoi(s);
    // End Day
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].endDay = atoi(s);
    // End Hour
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].endHour = atoi(s);
    // End Minute
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].endMinute = atoi(s);
    // End Second
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].endSecond = atoi(s);
		// Repeat
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].repeat = atoi(s);

    if(Schedule[ScheduleEntryCount].startDay == DAY_EVERYDAY)
    {
      day =0;
    }
    else
    {
      day = Schedule[ScheduleEntryCount].startDay;
    }
    Schedule[ScheduleEntryCount].startWeeklySecond = 
        GetWeeklySeconds(day,Schedule[ScheduleEntryCount].startHour,
                         Schedule[ScheduleEntryCount].startMinute,
                         Schedule[ScheduleEntryCount].startSecond);

    Schedule[ScheduleEntryCount].endWeeklySecond = 
        GetWeeklySeconds(day,Schedule[ScheduleEntryCount].endHour,
                         Schedule[ScheduleEntryCount].endMinute,
                         Schedule[ScheduleEntryCount].endSecond);

    ScheduleEntryCount++;
  }
  fclose(fp);
  SchedulePrint();
  return;
}

void SchedulePrint()
{
  int i=0;
  int h;
  h= GetNextScheduleEntry();
  for(i=0;i<ScheduleEntryCount;i++)
  {
    sprintf(logText,"%s  Next=%d   %d-%.2d:%.2d:%.2d,%.2d-%.2d:%.2d:%.2d  sws=%d,ews=%d\n",
                                          Schedule[i].playList,h,
                                          Schedule[i].startDay,
                                          Schedule[i].startHour,
                                          Schedule[i].startMinute,
                                          Schedule[i].startSecond,
                                          Schedule[i].endDay,
                                          Schedule[i].endHour,
                                          Schedule[i].endMinute,
                                          Schedule[i].endSecond,
                                          Schedule[i].startWeeklySecond,
                                          Schedule[i].endWeeklySecond
                                          );
    //LogWrite(logText);

  }
}

int GetNextScheduleEntry()
{
  int i;
  int nextEntrySecondsFromNow=SECONDS_PER_WEEK;
  int nextEntryIndex=0;
  int secondsFromNow=0;
  int nowWeeklySeconds=0;

  // Get weekly seconds the current time
  time_t currTime = time(NULL);
  struct tm *now = localtime(&currTime);
  nowWeeklySeconds = GetWeeklySeconds(now->tm_wday, now->tm_hour, now->tm_min, now->tm_sec);

  sprintf(logText,"Nowseconds= %d\n", nowWeeklySeconds);
  //LogWrite(logText); 

  for(i=0;i<ScheduleEntryCount;i++)
  {
    secondsFromNow = GetWeeklySecondDifference(nowWeeklySeconds,Schedule[i].startWeeklySecond,Schedule[i].startDay);
    sprintf(logText,"secondsFromNow= %d\n", secondsFromNow);
    //LogWrite(logText); 
    if(secondsFromNow<nextEntrySecondsFromNow)
    {
      nextEntryIndex = i;
      nextEntrySecondsFromNow = secondsFromNow;
    }
  }
  sprintf(logText,"nextEntryIndex= %d\n", nextEntryIndex);
  //LogWrite(logText); 
  return nextEntryIndex;
}

int GetWeeklySeconds(int day, int hour, int minute, int second)
{
  int weeklySeconds = (day*SECONDS_PER_DAY) + (hour*SECONDS_PER_HOUR) + (minute*SECONDS_PER_MINUTE) + second;
  return weeklySeconds;
}

int GetWeeklySecondDifference(int weeklySeconds1, int weeklySeconds2, int day)
{
  int seconds;
  int i;
  if(day == DAY_EVERYDAY)
  {
    if(weeklySeconds1 > (6*weeklySeconds2))
    {
      seconds = (SECONDS_PER_WEEK - weeklySeconds1) + weeklySeconds2;
    }
    else
    {
      for(i=0;i<7;i++)
      {
        if(weeklySeconds1 < (weeklySeconds2 + (i*SECONDS_PER_DAY)))
        {
          seconds = (weeklySeconds2 + (i*SECONDS_PER_DAY)) - weeklySeconds1;
          break;
        }
      }
    }
  }
  // Not everyday entry
  else
  {
    if(weeklySeconds1<weeklySeconds2)
    {
      seconds = weeklySeconds2-weeklySeconds1;
    }
    else
    {
      seconds = (SECONDS_PER_WEEK - weeklySeconds1) + weeklySeconds2;
    }
  }
  return seconds;
}

void GetNextScheduleStartText(char * txt)
{
		char day[12];
		int nextScheduleEntry = GetNextScheduleEntry();
		GetDayTextFromScheduleEntry(Schedule[nextScheduleEntry].startDay,day);
    sprintf(txt,"%s @ %02d:%02d:%02d",day,
		                                  Schedule[nextScheduleEntry].startHour,
																	    Schedule[nextScheduleEntry].startMinute,
																	    Schedule[nextScheduleEntry].startSecond);
}

void GetNextPlaylistText(char * txt)
{
		char day[12];
		int nextScheduleEntry = GetNextScheduleEntry();
    sprintf(txt,"%s", Schedule[nextScheduleEntry].playList);
}

void GetDayTextFromScheduleEntry(int index,char * txt)
{
	switch(index)
	{
		case 0:
			strcpy(txt,"Sunday");
			break;		
		case 1:
			strcpy(txt,"Monday");
			break;		
		case 2:
			strcpy(txt,"Tuesday");
			break;		
		case 3:
			strcpy(txt,"Wednesday");
			break;		
		case 4:
			strcpy(txt,"Thursday");
			break;		
		case 5:
			strcpy(txt,"Friday");
			break;		
		case 6:
			strcpy(txt,"Saturday");
			break;		
		case 7:
			strcpy(txt,"Everyday");
			break;	
		default:
			strcpy(txt, "Error");
			break;	
	}
}