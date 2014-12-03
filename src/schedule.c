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

#include <ctype.h>
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
int nowWeeklySeconds2 = 0;
int lastLoadDate = 0;

extern PlaylistDetails playlistDetails;

int DateStrToInt(char *str)
{
	if ((!str) || (str[4] != '-') || (str[7] != '-') || (str[10] != 0x0))
		return 0;

	int result = 0;
	char tmpStr[11];

	strcpy(tmpStr, str);

	result += atoi(str    ) * 10000; // Year
	result += atoi(str + 5) *   100; // Month
	result += atoi(str + 8)        ; // Day

	return result;
}

int GetCurrentDateInt(void)
{
	time_t currTime = time(NULL);
	struct tm *now = localtime(&currTime);
	int result = 0;

	result += (now->tm_year + 1900) * 10000;
	result += (now->tm_mon + 1)     *   100;
	result += (now->tm_mday)               ;

	return result;
}

int CurrentDateInRange(int startDate, int endDate)
{
	int currentDate = GetCurrentDateInt();

	if ((startDate < 10000) || (endDate < 10000))
	{
		startDate = startDate % 10000;
		endDate = endDate % 10000;
		currentDate = currentDate % 10000;
	}

	if ((startDate < 100) || (endDate < 100))
	{
		startDate = startDate % 100;
		endDate = endDate % 100;
		currentDate = currentDate % 100;
	}

	if ((startDate == 0) && (endDate == 0))
		return 1;

	if ((startDate <= currentDate) && (currentDate <= endDate))
		return 1;

	return 0;
}

void ScheduleProc()
{
  if (lastLoadDate != GetCurrentDateInt())
  {
    if (FPPstatus == FPP_STATUS_IDLE)
      CurrentScheduleHasbeenLoaded = 0;

    NextScheduleHasbeenLoaded = 0;
  }

  if (!CurrentScheduleHasbeenLoaded || !NextScheduleHasbeenLoaded)
    LoadScheduleFromFile();

  if(!CurrentScheduleHasbeenLoaded)
    LoadCurrentScheduleInfo();

  if(!NextScheduleHasbeenLoaded)
    LoadNextScheduleInfo();

  switch(FPPstatus)
  {
    case FPP_STATUS_IDLE:
      if (currentSchedulePlaylist.ScheduleEntryIndex != SCHEDULE_INDEX_INVALID)
        PlayListLoadCheck();
      break;
    case FPP_STATUS_PLAYLIST_PLAYING:
      PlayListStopCheck();
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
		if ((Schedule[i].enable) &&
			(Schedule[i].repeat) &&
			(CurrentDateInRange(Schedule[i].startDate, Schedule[i].endDate)))
		{
			for(j=0;j<Schedule[i].weeklySecondCount;j++)
			{
				if((nowWeeklySeconds>=Schedule[i].weeklyStartSeconds[j]) && (nowWeeklySeconds < Schedule[i].weeklyEndSeconds[j]))
				{
					LogWarn(VB_SCHEDULE, "Should be playing now - schedule index = %d weekly index= %d\n",i,j);
					currentSchedulePlaylist.ScheduleEntryIndex = i;
					currentSchedulePlaylist.startWeeklySeconds = Schedule[i].weeklyStartSeconds[j];
					currentSchedulePlaylist.endWeeklySeconds = Schedule[i].weeklyEndSeconds[j];

					// Make end time non-inclusive
					if (currentSchedulePlaylist.startWeeklySeconds != currentSchedulePlaylist.endWeeklySeconds)
						currentSchedulePlaylist.endWeeklySeconds--;

					CurrentScheduleHasbeenLoaded = 1;
					NextScheduleHasbeenLoaded = 0;
		      strcpy(playlistDetails.currentPlaylistFile,Schedule[currentSchedulePlaylist.ScheduleEntryIndex].playList);
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
  for(i=0;i<ScheduleEntryCount;i++)
  {
		if ((Schedule[i].enable) &&
			(CurrentDateInRange(Schedule[i].startDate, Schedule[i].endDate)))
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
  lastLoadDate = 0;
  CurrentScheduleHasbeenLoaded = 0;
}

void ReLoadNextScheduleInfo(void)
{
  lastLoadDate = 0;
  NextScheduleHasbeenLoaded = 0;
}

void LoadCurrentScheduleInfo()
{
  currentSchedulePlaylist.ScheduleEntryIndex = GetNextScheduleEntry(&currentSchedulePlaylist.weeklySecondIndex);
	currentSchedulePlaylist.startWeeklySeconds = Schedule[currentSchedulePlaylist.ScheduleEntryIndex].weeklyStartSeconds[currentSchedulePlaylist.weeklySecondIndex];
	currentSchedulePlaylist.endWeeklySeconds = Schedule[currentSchedulePlaylist.ScheduleEntryIndex].weeklyEndSeconds[currentSchedulePlaylist.weeklySecondIndex];

	// Make end time non-inclusive
	if (currentSchedulePlaylist.startWeeklySeconds != currentSchedulePlaylist.endWeeklySeconds)
		currentSchedulePlaylist.endWeeklySeconds--;

  CurrentScheduleHasbeenLoaded = 1;
}

void LoadNextScheduleInfo()
{
  char t[64];
  char p[64];

  nextSchedulePlaylist.ScheduleEntryIndex = GetNextScheduleEntry(&nextSchedulePlaylist.weeklySecondIndex);
  NextScheduleHasbeenLoaded = 1;

  if (nextSchedulePlaylist.ScheduleEntryIndex >= 0)
  {
    GetNextPlaylistText(p);
    GetNextScheduleStartText(t);
    LogDebug(VB_SCHEDULE, "Next Scheduled Playlist is '%s' for %s\n",p, t);
  }
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

    // If current schedule starttime is in the past and the item is not set
    // for repeat, then reschedule.
    if ((diff < -1) &&
        (!Schedule[currentSchedulePlaylist.ScheduleEntryIndex].repeat))
      ReLoadCurrentScheduleInfo();

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
      strcpy(playlistDetails.currentPlaylistFile,Schedule[currentSchedulePlaylist.ScheduleEntryIndex].playList);
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

    // This check for 1 second ago is a hack rather than a more invasive
    // patch to handle the race condition if we miss this check on the exact
    // second the schedule should be ending.  The odds of us missing 2 in a row
    // are much lower, so this will suffice for v1.0.
    if((nowWeeklySeconds == currentSchedulePlaylist.endWeeklySeconds) ||
       (nowWeeklySeconds == (currentSchedulePlaylist.endWeeklySeconds - 1)))
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

  lastLoadDate = GetCurrentDateInt();

  LogInfo(VB_SCHEDULE, "Loading Schedule from %s\n",getScheduleFile());
  fp = fopen((const char *)getScheduleFile(), "r");
  if (fp == NULL) 
  {
		return;
  }
  while(fgets(buf, 512, fp) != NULL)
  {
    while (isspace(buf[strlen(buf)-1]))
        buf[strlen(buf)-1] = 0x0;

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

    // Start Date
    s=strtok(NULL,",");
    if (s && strcmp(s, ""))
      Schedule[ScheduleEntryCount].startDate = DateStrToInt(s);
    else
      Schedule[ScheduleEntryCount].startDate = 20140101;

    // End Date
    s=strtok(NULL,",");
    if (s && strcmp(s, ""))
      Schedule[ScheduleEntryCount].endDate = DateStrToInt(s);
    else
      Schedule[ScheduleEntryCount].endDate = 20991231;

    // Set WeeklySecond start and end times
    SetScheduleEntrysWeeklyStartAndEndSeconds(&Schedule[ScheduleEntryCount]);
    ScheduleEntryCount++;
  }
  fclose(fp);

  SchedulePrint();

  return;
}

void SchedulePrint()
{
  int i=0;

  LogInfo(VB_SCHEDULE, "Current Schedule: (Status: '+' = Enabled, '-' = Disabled, '!' = Outside Date Range, '*' = Repeat)\n");
  LogInfo(VB_SCHEDULE, "St  Start & End Dates       Days         Start & End Times   Playlist\n");
  LogInfo(VB_SCHEDULE, "--- ----------------------- ------------ ------------------- ---------------------------------------------\n");
  for(i=0;i<ScheduleEntryCount;i++)
  {
    char dayStr[32];
    GetDayTextFromDayIndex(Schedule[i].dayIndex, dayStr);
    LogInfo(VB_SCHEDULE, "%c%c%c %04d-%02d-%02d - %04d-%02d-%02d %-12.12s %02d:%02d:%02d - %02d:%02d:%02d %s\n",
      Schedule[i].enable ? '+': '-',
      CurrentDateInRange(Schedule[i].startDate, Schedule[i].endDate) ? ' ': '!',
      Schedule[i].repeat ? '*': ' ',
      (int)(Schedule[i].startDate / 10000),
      (int)(Schedule[i].startDate % 10000 / 100),
      (int)(Schedule[i].startDate % 100),
      (int)(Schedule[i].endDate / 10000),
      (int)(Schedule[i].endDate % 10000 / 100),
      (int)(Schedule[i].endDate % 100),
      dayStr,
      Schedule[i].startHour,Schedule[i].startMinute,Schedule[i].startSecond,
      Schedule[i].endHour,Schedule[i].endMinute,Schedule[i].endSecond,
      Schedule[i].playList);
  }
}

int GetWeeklySeconds(int day, int hour, int minute, int second)
{
  int weeklySeconds = (day*SECONDS_PER_DAY) + (hour*SECONDS_PER_HOUR) + (minute*SECONDS_PER_MINUTE) + second;
  return weeklySeconds;
}

int GetWeeklySecondDifference(int weeklySeconds1, int weeklySeconds2)
{
  int seconds = 0;
  int i;
  if(weeklySeconds1<weeklySeconds2)
  {
    seconds = weeklySeconds2-weeklySeconds1;
  }
  else if (weeklySeconds1 > weeklySeconds2)
  {
    seconds = (SECONDS_PER_WEEK - weeklySeconds1) + weeklySeconds2;
  }

  return seconds;
}

int GetDayFromWeeklySeconds(int weeklySeconds)
{
  return (int)(weeklySeconds/SECONDS_PER_DAY);
}

int FindNextStartingScheduleIndex(void)
{
	int i = 0;
	int index = -1;
	int currentDate = GetCurrentDateInt();
	int schedDate = 99999999;
	int weeklySecondStart = 999999;
	for(i = 0; i < ScheduleEntryCount; i++)
	{
		if ((Schedule[i].enable) &&
			(Schedule[i].startDate >= currentDate) &&
			(Schedule[i].endDate < schedDate))
		{
			index = i;
			schedDate = Schedule[i].endDate;
		}
	}

	return index;
}

void GetNextScheduleStartText(char * txt)
{
	if (!NextScheduleHasbeenLoaded)
		return;

	if (nextSchedulePlaylist.ScheduleEntryIndex >= 0)
	{
		GetScheduleEntryStartText(nextSchedulePlaylist.ScheduleEntryIndex,nextSchedulePlaylist.weeklySecondIndex,txt);
	}
	else
	{
		char dayText[16];
		int found = FindNextStartingScheduleIndex();

		if (found >= 0)
		{
			GetDayTextFromDayIndex(Schedule[found].dayIndex,dayText);
			sprintf(txt, "%04d-%02d-%02d @ %02d:%02d:%02d - (%s)\n",
				(int)(Schedule[found].startDate / 10000),
				(int)(Schedule[found].startDate % 10000 / 100),
				(int)(Schedule[found].startDate % 100),
				Schedule[found].startHour,
				Schedule[found].startMinute,
				Schedule[found].startSecond,
				dayText);
		}
	}
}

void GetNextPlaylistText(char * txt)
{
	if (!NextScheduleHasbeenLoaded)
		return;

	if (nextSchedulePlaylist.ScheduleEntryIndex >= 0)
	{
		strcpy(txt,Schedule[nextSchedulePlaylist.ScheduleEntryIndex].playList);
	}
	else
	{
		int found = FindNextStartingScheduleIndex();

		if (found >= 0)
			strcpy(txt,Schedule[found].playList);
	}
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
