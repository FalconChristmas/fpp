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

#include "fpp.h"
#include "log.h"
#include "schedule.h"
#include "command.h"
#include "playList.h"
#include "settings.h"

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>



ScheduleEntry Schedule[MAX_SCHEDULE_ENTRIES];
SchedulePlaylistDetails currentSchedulePlaylist,nextSchedulePlaylist;
int ScheduleEntryCount=0;

unsigned char CurrentScheduleHasbeenLoaded=0;
unsigned char NextScheduleHasbeenLoaded=0;
int nowWeeklySeconds2;

extern PlaylistDetails playlistDetails;


void ScheduleProc()
{
  switch(FPPstatus)
  {
    case FPP_STATUS_IDLE:
      if(!CurrentScheduleHasbeenLoaded)
      {
        LoadCurrentScheduleInfo();
      }
      if(!NextScheduleHasbeenLoaded)
      {
        LoadNextScheduleInfo();
			}
			
      PlayListLoadCheck();
      break;
    case FPP_STATUS_PLAYLIST_PLAYING:
      if(!NextScheduleHasbeenLoaded)
      {
        LoadNextScheduleInfo();
      }
// FIXME:
// Why do we only check to see if we should stop if we were repeating?
// Commenting out for now to fix several reported issues, but these lines
// will probably go away after a discussion.
//			if(Schedule[currentSchedulePlaylist.ScheduleEntryIndex].repeat)
//			{
	      PlayListStopCheck();
//			}
      break;
    default:
      break;

  }
}

void CheckIfShouldBePlayingNow()
{
  int i,j,dayCount;
  time_t currTime = time(NULL);
  struct tm *now = localtime(&currTime);
  int nowWeeklySeconds = GetWeeklySeconds(now->tm_wday, now->tm_hour, now->tm_min, now->tm_sec);
  LoadScheduleFromFile();
  for(i=0;i<ScheduleEntryCount;i++)
  {
		// only check schedule entries that are enabled and set to repeat.
		// Do not start non repeatable entries
		if(Schedule[i].enable && Schedule[i].repeat)
		{
			for(j=0;j<Schedule[i].weeklySecondCount;j++)
			{
				if((nowWeeklySeconds>=Schedule[i].weeklyStartSeconds[j]) && (nowWeeklySeconds < Schedule[i].weeklyEndSeconds[j]))
				{
					LogWarn(VB_SCHEDULE, "Should be playing now - schedule index = %d weekly index= %d\n",i,j);
					currentSchedulePlaylist.ScheduleEntryIndex = i;
					currentSchedulePlaylist.startWeeklySeconds = Schedule[i].weeklyStartSeconds[j];
					currentSchedulePlaylist.endWeeklySeconds = Schedule[i].weeklyEndSeconds[j];
					CurrentScheduleHasbeenLoaded = 1;
					NextScheduleHasbeenLoaded = 0;
		      strcpy((void*)playlistDetails.currentPlaylistFile,Schedule[currentSchedulePlaylist.ScheduleEntryIndex].playList);
				  playlistDetails.currentPlaylistEntry=0;
					playlistDetails.repeat = Schedule[currentSchedulePlaylist.ScheduleEntryIndex].repeat;
		  		playlistDetails.playlistStarting=1;
      		FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
				}				
			}
		}
  }
}


int GetNextScheduleEntry(int *weeklySecondIndex)
{
  int i,j,dayCount;
  int leastWeeklySecondDifferenceFromNow=SECONDS_PER_WEEK;
  int difference;
  int nextEntryIndex = SCHEDULE_INDEX_INVALID;
  time_t currTime = time(NULL);
  struct tm *now = localtime(&currTime);
  int nowWeeklySeconds = GetWeeklySeconds(now->tm_wday, now->tm_hour, now->tm_min, now->tm_sec);
  LoadScheduleFromFile();
  for(i=0;i<ScheduleEntryCount;i++)
  {
		if(Schedule[i].enable)
		{
			for(j=0;j<Schedule[i].weeklySecondCount;j++)
			{
				difference = GetWeeklySecondDifference(nowWeeklySeconds,Schedule[i].weeklyStartSeconds[j]);
				if(difference<leastWeeklySecondDifferenceFromNow)
				{
					leastWeeklySecondDifferenceFromNow = difference; 
					nextEntryIndex = i;
					*weeklySecondIndex = j;
				}
			}
		}
  }
  LogDebug(VB_SCHEDULE, "nextEntryIndex = %d, least diff = %d, weekly index = %d\n",nextEntryIndex,leastWeeklySecondDifferenceFromNow,*weeklySecondIndex);
  return nextEntryIndex;
}

void ReLoadCurrentScheduleInfo()
{
  CurrentScheduleHasbeenLoaded = 0;
}

void LoadCurrentScheduleInfo()
{
  currentSchedulePlaylist.ScheduleEntryIndex = GetNextScheduleEntry(&currentSchedulePlaylist.weeklySecondIndex);
	currentSchedulePlaylist.startWeeklySeconds = Schedule[currentSchedulePlaylist.ScheduleEntryIndex].weeklyStartSeconds[currentSchedulePlaylist.weeklySecondIndex];
	currentSchedulePlaylist.endWeeklySeconds = Schedule[currentSchedulePlaylist.ScheduleEntryIndex].weeklyEndSeconds[currentSchedulePlaylist.weeklySecondIndex];
  CurrentScheduleHasbeenLoaded = 1;
}

void LoadNextScheduleInfo()
{
  char t[64];

  nextSchedulePlaylist.ScheduleEntryIndex = GetNextScheduleEntry(&nextSchedulePlaylist.weeklySecondIndex);
  NextScheduleHasbeenLoaded = 1;

  GetNextPlaylistText(t);
  LogDebug(VB_SCHEDULE, "Next Playlist = %s\n",t);
  GetNextScheduleStartText(t);
  LogDebug(VB_SCHEDULE, "Next Schedule Start text = %s\n",t);
}

void SetScheduleEntrysWeeklyStartAndEndSeconds(ScheduleEntry * entry)
{
	switch(entry->dayIndex)
  {
		case INX_SUN:
		case INX_MON:
		case INX_TUE:
		case INX_WED:
		case INX_THU:
		case INX_FRI:
    case INX_SAT:
      entry->weeklyStartSeconds[0] = GetWeeklySeconds(entry->dayIndex,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyEndSeconds[0] = GetWeeklySeconds(entry->dayIndex,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklySecondCount = 1;
      break;
    case INX_EVERYDAY:
      entry->weeklyStartSeconds[0] = GetWeeklySeconds(INX_SUN,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[1] = GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[2] = GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[3] = GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[4] = GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[5] = GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[6] = GetWeeklySeconds(INX_SAT,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyEndSeconds[0] = GetWeeklySeconds(INX_SUN,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[1] = GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[2] = GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[3] = GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[4] = GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[5] = GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[6] = GetWeeklySeconds(INX_SAT,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklySecondCount = 7;
      break;
    case INX_WKDAYS:
      entry->weeklyStartSeconds[0] = GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[1] = GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[2] = GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[3] = GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[4] = GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyEndSeconds[0] = GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[1] = GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[2] = GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[3] = GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[4] = GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklySecondCount = 5;

      break;
    case INX_WKEND:
      entry->weeklyStartSeconds[0] = GetWeeklySeconds(INX_SAT,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[1] = GetWeeklySeconds(INX_SUN,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyEndSeconds[0] = GetWeeklySeconds(INX_SAT,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[1] = GetWeeklySeconds(INX_SUN,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklySecondCount = 2;
      break;
    case INX_M_W_F:
      entry->weeklyStartSeconds[0] = GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[1] = GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[2] = GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyEndSeconds[0] = GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[1] = GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[2] = GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklySecondCount = 3;
      break;
    case INX_T_TH:
      entry->weeklyStartSeconds[0] = GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[1] = GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyEndSeconds[0] = GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[1] = GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklySecondCount = 2;
      break;
		case INX_SUN_TO_THURS:
      entry->weeklyStartSeconds[0] = GetWeeklySeconds(INX_SUN,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[1] = GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[2] = GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[3] = GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[4] = GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyEndSeconds[0] = GetWeeklySeconds(INX_SUN,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[1] = GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[2] = GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[3] = GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[4] = GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklySecondCount = 5;
			break;
		case INX_FRI_SAT:
      entry->weeklyStartSeconds[0] = GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyStartSeconds[1] = GetWeeklySeconds(INX_SAT,entry->startHour,entry->startMinute,entry->startSecond);
      entry->weeklyEndSeconds[0] = GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklyEndSeconds[1] = GetWeeklySeconds(INX_SAT,entry->endHour,entry->endMinute,entry->endSecond);
      entry->weeklySecondCount = 2;
      break;
		
    default:
      entry->weeklySecondCount = 0;
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

    int diff = currentSchedulePlaylist.startWeeklySeconds - nowWeeklySeconds;
    int displayDiff = 0;

    if (diff < -600) // assume the schedule is actually next week for display
      diff += (24 * 3600 * 7);

    // Convoluted code to print the countdown more frequently as we get closer
    if (((diff > 300) &&                  ((diff % 300) == 0)) ||
        ((diff >  60) && (diff <= 300) && ((diff %  60) == 0)) ||
        ((diff >  10) && (diff <=  60) && ((diff %  10) == 0)) ||
        (                (diff <=  10)))
    {
      displayDiff = diff;
    }

    if (currentSchedulePlaylist.startWeeklySeconds && displayDiff)
      LogInfo(VB_SCHEDULE, "NowSecs = %d, CurrStartSecs = %d (%d seconds away)\n",
        nowWeeklySeconds,currentSchedulePlaylist.startWeeklySeconds, displayDiff);

    if(nowWeeklySeconds == currentSchedulePlaylist.startWeeklySeconds)
    {
      NextScheduleHasbeenLoaded = 0;
      strcpy((void*)playlistDetails.currentPlaylistFile,Schedule[currentSchedulePlaylist.ScheduleEntryIndex].playList);
		  playlistDetails.currentPlaylistEntry=0;
			playlistDetails.repeat = Schedule[currentSchedulePlaylist.ScheduleEntryIndex].repeat;
		  playlistDetails.playlistStarting=1;
      LogInfo(VB_SCHEDULE, "Schedule Entry: %02d:%02d:%02d - %02d:%02d:%02d - Starting Playlist %s for %d seconds\n",
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].startHour,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].startMinute,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].startSecond,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].endHour,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].endMinute,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].endSecond,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].playList,
        currentSchedulePlaylist.endWeeklySeconds - currentSchedulePlaylist.startWeeklySeconds);
      LogInfo(VB_SCHEDULE, "NowSecs = %d, CurrStartSecs = %d, CurrEndSecs = %d (%d seconds away)\n",
        nowWeeklySeconds, currentSchedulePlaylist.startWeeklySeconds, currentSchedulePlaylist.endWeeklySeconds, displayDiff);
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

    int diff = currentSchedulePlaylist.endWeeklySeconds - nowWeeklySeconds;
    int displayDiff = 0;

    // Convoluted code to print the countdown more frequently as we get closer
    if (((diff > 300) &&                  ((diff % 300) == 0)) ||
        ((diff >  60) && (diff <= 300) && ((diff %  60) == 0)) ||
        ((diff >  10) && (diff <=  60) && ((diff %  10) == 0)) ||
        (                (diff <=  10)))
    {
      displayDiff = diff;
    }

    if (displayDiff)
      LogInfo(VB_SCHEDULE, "NowSecs = %d, CurrEndSecs = %d (%d seconds away)\n",
        nowWeeklySeconds, currentSchedulePlaylist.endWeeklySeconds, displayDiff);

    if(nowWeeklySeconds == currentSchedulePlaylist.endWeeklySeconds)
    {
      LogInfo(VB_SCHEDULE, "Schedule Entry: %02d:%02d:%02d - %02d:%02d:%02d - Stopping Playlist Gracefully\n",
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].startHour,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].startMinute,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].startSecond,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].endHour,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].endMinute,
        Schedule[currentSchedulePlaylist.ScheduleEntryIndex].endSecond);
      CurrentScheduleHasbeenLoaded = 0;
      StopPlaylistGracefully();
    }
  }

}

int GetStartSecond(int startSecond1, int startSecond2, int day)
{
}

int GetEndSecond(int endSecond1, int endSecond2, int day)
{
}


void LoadScheduleFromFile()
{
  FILE *fp;
  char buf[512];
  char *s;
  ScheduleEntryCount=0;
  int day;
  LogInfo(VB_SCHEDULE, "Opening File Now %s\n",getScheduleFile());
  fp = fopen((const char *)getScheduleFile(), "r");
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
    // Start Day index
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].dayIndex = atoi(s);
    // Start Hour
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].startHour = atoi(s);
    // Start Minute
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].startMinute = atoi(s);
    // Start Second
    s=strtok(NULL,",");
    Schedule[ScheduleEntryCount].startSecond = atoi(s);
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

    char dayStr[32];
    GetDayTextFromDayIndex(Schedule[ScheduleEntryCount].dayIndex, dayStr);
    LogInfo(VB_SCHEDULE, "Schedule Entry: %-12.12s %02d:%02d:%02d - %02d:%02d:%02d (%s)\n", dayStr,
      Schedule[ScheduleEntryCount].startHour,Schedule[ScheduleEntryCount].startMinute,Schedule[ScheduleEntryCount].startSecond,
      Schedule[ScheduleEntryCount].endHour,Schedule[ScheduleEntryCount].endMinute,Schedule[ScheduleEntryCount].endSecond,
      Schedule[ScheduleEntryCount].playList);

    // Set WeeklySecond start and end times
    SetScheduleEntrysWeeklyStartAndEndSeconds(&Schedule[ScheduleEntryCount]);
    ScheduleEntryCount++;
  }
  fclose(fp);
//  SchedulePrint();
  return;
}

void SchedulePrint()
{
  int i=0;
  int h;
  // this causes a loop
  //h= GetNextScheduleEntry();
  for(i=0;i<ScheduleEntryCount;i++)
  {
//    LogDebug(VB_SCHEDULE, "%s  Next=%d   %d-%.2d:%.2d:%.2d,%.2d-%.2d:%.2d:%.2d  sws=%d,ews=%d\n", \
                                          Schedule[i].playList,h, \
                                          Schedule[i].startDay, \
                                          Schedule[i].startHour, \
                                          Schedule[i].startMinute, \
                                          Schedule[i].startSecond, \
                                          Schedule[i].endDay, \
                                          Schedule[i].endHour, \
                                          Schedule[i].endMinute, \
                                          Schedule[i].endSecond, \
                                          Schedule[i].startWeeklySecond, \
                                          Schedule[i].endWeeklySecond \
                                          );

  }
}

int GetWeeklySeconds(int day, int hour, int minute, int second)
{
  int weeklySeconds = (day*SECONDS_PER_DAY) + (hour*SECONDS_PER_HOUR) + (minute*SECONDS_PER_MINUTE) + second;
  return weeklySeconds;
}

int GetWeeklySecondDifference(int weeklySeconds1, int weeklySeconds2)
{
  int seconds;
  int i;
  if(weeklySeconds1<weeklySeconds2)
  {
    seconds = weeklySeconds2-weeklySeconds1;
  }
  else
  {
    seconds = (SECONDS_PER_WEEK - weeklySeconds1) + weeklySeconds2;
  }
  return seconds;
}

int GetDayFromWeeklySeconds(int weeklySeconds)
{
  return (int)(weeklySeconds/SECONDS_PER_DAY);
}

void GetNextScheduleStartText(char * txt)
{
	if (!NextScheduleHasbeenLoaded)
		return;

	GetScheduleEntryStartText(nextSchedulePlaylist.ScheduleEntryIndex,nextSchedulePlaylist.weeklySecondIndex,txt);
}

void GetNextPlaylistText(char * txt)
{
	if (!NextScheduleHasbeenLoaded)
		return;

	strcpy(txt,Schedule[nextSchedulePlaylist.ScheduleEntryIndex].playList);
}

void GetScheduleEntryStartText(int index,int weeklySecondIndex, char * txt)
{
		char text[64];
		char dayText[16];
    int dayIndex = GetDayFromWeeklySeconds(Schedule[index].weeklyStartSeconds[weeklySecondIndex]);
    GetDayTextFromDayIndex(dayIndex,text);
		if(Schedule[index].dayIndex > INX_SAT)
		{
			GetDayTextFromDayIndex(Schedule[index].dayIndex,dayText);
			sprintf(txt,"%s @ %02d:%02d:%02d - (%s)",
							text,Schedule[index].startHour,Schedule[index].startMinute,Schedule[index].startSecond,dayText);
		}
		else
		{
			sprintf(txt,"%s @ %02d:%02d:%02d",
							text,Schedule[index].startHour,Schedule[index].startMinute,Schedule[index].startSecond);
		}		
}

void GetDayTextFromDayIndex(int index,char * txt)
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
		case 8:
			strcpy(txt,"Weekdays");
			break;	
		case 9:
			strcpy(txt,"Weekends");
			break;	
		case 10:
			strcpy(txt,"Mon/Wed/Fri");
			break;	
		case 11:
			strcpy(txt,"Tues-Thurs");
			break;	
		case 12:
			strcpy(txt,"Sun-Thurs");
			break;	
		case 13:
			strcpy(txt,"Fri/Sat");
			break;	
		default:
			strcpy(txt, "Error\0");
			break;	
	}
}
