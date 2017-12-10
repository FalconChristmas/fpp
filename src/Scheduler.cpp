/*
 *   Scheduler Class for Falcon Pi Player (FPP)
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

#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#include "command.h"
#include "common.h"
#include "fpp.h"
#include "log.h"
#include "Playlist.h"
#include "Scheduler.h"
#include "settings.h"

Scheduler *scheduler = NULL;

/////////////////////////////////////////////////////////////////////////////

Scheduler::Scheduler()
  : m_ScheduleEntryCount(0),
	m_CurrentScheduleHasbeenLoaded(0),
	m_NextScheduleHasbeenLoaded(0),
	m_nowWeeklySeconds2(0),
	m_lastLoadDate(0),
	m_lastLoadTime(0),
	m_lastCalculateTime(0),
	m_runThread(0),
	m_threadIsRunning(0)
{
	bzero(&m_currentSchedulePlaylist, sizeof(m_currentSchedulePlaylist));
	pthread_mutex_init(&m_scheduleLock, NULL);
}

Scheduler::~Scheduler()
{
	pthread_mutex_destroy(&m_scheduleLock);
}

void Scheduler::ScheduleProc(void)
{
  if (m_lastLoadDate != GetCurrentDateInt())
  {
    if (FPPstatus == FPP_STATUS_IDLE)
      m_CurrentScheduleHasbeenLoaded = 0;

    m_NextScheduleHasbeenLoaded = 0;
  }

  if (!m_CurrentScheduleHasbeenLoaded || !m_NextScheduleHasbeenLoaded)
    LoadScheduleFromFile();

  if(!m_CurrentScheduleHasbeenLoaded)
    LoadCurrentScheduleInfo();

  if(!m_NextScheduleHasbeenLoaded)
    LoadNextScheduleInfo();

  switch(FPPstatus)
  {
    case FPP_STATUS_IDLE:
      if (m_currentSchedulePlaylist.ScheduleEntryIndex != SCHEDULE_INDEX_INVALID)
        PlayListLoadCheck();
      break;
    case FPP_STATUS_PLAYLIST_PLAYING:
      PlayListStopCheck();
      break;
    default:
      break;

  }
}

void Scheduler::CheckIfShouldBePlayingNow(void)
{
  int i,j,dayCount;
  time_t currTime = time(NULL);
  struct tm now;

  localtime_r(&currTime, &now);

  int nowWeeklySeconds = GetWeeklySeconds(now.tm_wday, now.tm_hour, now.tm_min, now.tm_sec);
  LoadScheduleFromFile();
  for(i=0;i<m_ScheduleEntryCount;i++)
  {
		// only check schedule entries that are enabled and set to repeat.
		// Do not start non repeatable entries
		if ((m_Schedule[i].enable) &&
			(m_Schedule[i].repeat) &&
			(CurrentDateInRange(m_Schedule[i].startDate, m_Schedule[i].endDate)))
		{
			for(j=0;j<m_Schedule[i].weeklySecondCount;j++)
			{
				if((nowWeeklySeconds>=m_Schedule[i].weeklyStartSeconds[j]) && (nowWeeklySeconds < m_Schedule[i].weeklyEndSeconds[j]))
				{
					LogWarn(VB_SCHEDULE, "Should be playing now - schedule index = %d weekly index= %d\n",i,j);
					m_currentSchedulePlaylist.ScheduleEntryIndex = i;
					m_currentSchedulePlaylist.startWeeklySeconds = m_Schedule[i].weeklyStartSeconds[j];
					m_currentSchedulePlaylist.endWeeklySeconds = m_Schedule[i].weeklyEndSeconds[j];

					// Make end time non-inclusive
					if (m_currentSchedulePlaylist.startWeeklySeconds != m_currentSchedulePlaylist.endWeeklySeconds)
						m_currentSchedulePlaylist.endWeeklySeconds--;

					m_CurrentScheduleHasbeenLoaded = 1;
					m_NextScheduleHasbeenLoaded = 0;
		      strcpy(playlist->m_playlistDetails.currentPlaylistFile,m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].playList);
				  playlist->m_playlistDetails.currentPlaylistEntry=0;
					playlist->m_playlistDetails.repeat = m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].repeat;
		  		playlist->m_playlistDetails.playlistStarting=1;
      		FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
				}				
			}
		}
  }
}


int Scheduler::GetNextScheduleEntry(int *weeklySecondIndex)
{
  int i,j,dayCount;
  int leastWeeklySecondDifferenceFromNow=SECONDS_PER_WEEK;
  int difference;
  int nextEntryIndex = SCHEDULE_INDEX_INVALID;
  time_t currTime = time(NULL);
  struct tm now;
  
  localtime_r(&currTime, &now);

  int nowWeeklySeconds = GetWeeklySeconds(now.tm_wday, now.tm_hour, now.tm_min, now.tm_sec);
  for(i=0;i<m_ScheduleEntryCount;i++)
  {
		if ((m_Schedule[i].enable) &&
			(CurrentDateInRange(m_Schedule[i].startDate, m_Schedule[i].endDate)))
		{
			for(j=0;j<m_Schedule[i].weeklySecondCount;j++)
			{
				difference = GetWeeklySecondDifference(nowWeeklySeconds,m_Schedule[i].weeklyStartSeconds[j]);
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

void Scheduler::ReLoadCurrentScheduleInfo(void)
{
  m_lastLoadDate = 0;
  m_CurrentScheduleHasbeenLoaded = 0;
}

void Scheduler::ReLoadNextScheduleInfo(void)
{
  m_lastLoadDate = 0;
  m_NextScheduleHasbeenLoaded = 0;
}

void Scheduler::LoadCurrentScheduleInfo(void)
{
  m_currentSchedulePlaylist.ScheduleEntryIndex = GetNextScheduleEntry(&m_currentSchedulePlaylist.weeklySecondIndex);
	m_currentSchedulePlaylist.startWeeklySeconds = m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].weeklyStartSeconds[m_currentSchedulePlaylist.weeklySecondIndex];
	m_currentSchedulePlaylist.endWeeklySeconds = m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].weeklyEndSeconds[m_currentSchedulePlaylist.weeklySecondIndex];

	// Make end time non-inclusive
	if (m_currentSchedulePlaylist.startWeeklySeconds != m_currentSchedulePlaylist.endWeeklySeconds)
		m_currentSchedulePlaylist.endWeeklySeconds--;

  m_CurrentScheduleHasbeenLoaded = 1;
}

void Scheduler::LoadNextScheduleInfo(void)
{
  char t[64];
  char p[64];

  m_nextSchedulePlaylist.ScheduleEntryIndex = GetNextScheduleEntry(&m_nextSchedulePlaylist.weeklySecondIndex);
  m_NextScheduleHasbeenLoaded = 1;

  if (m_nextSchedulePlaylist.ScheduleEntryIndex >= 0)
  {
    GetNextPlaylistText(p);
    GetNextScheduleStartText(t);
    LogDebug(VB_SCHEDULE, "Next Scheduled Playlist is '%s' for %s\n",p, t);
  }
}

void Scheduler::SetScheduleEntrysWeeklyStartAndEndSeconds(ScheduleEntryStruct * entry)
{
	if (entry->dayIndex & INX_DAY_MASK)
	{
		int count = 0;
		if (entry->dayIndex & INX_DAY_MASK_SUNDAY)
		{
			entry->weeklyStartSeconds[count] = GetWeeklySeconds(INX_SUN,entry->startHour,entry->startMinute,entry->startSecond);
			entry->weeklyEndSeconds[count]   = GetWeeklySeconds(INX_SUN,entry->endHour,entry->endMinute,entry->endSecond);
			count++;
		}

		if (entry->dayIndex & INX_DAY_MASK_MONDAY)
		{
			entry->weeklyStartSeconds[count] = GetWeeklySeconds(INX_MON,entry->startHour,entry->startMinute,entry->startSecond);
			entry->weeklyEndSeconds[count]   = GetWeeklySeconds(INX_MON,entry->endHour,entry->endMinute,entry->endSecond);
			count++;
		}

		if (entry->dayIndex & INX_DAY_MASK_TUESDAY)
		{
			entry->weeklyStartSeconds[count] = GetWeeklySeconds(INX_TUE,entry->startHour,entry->startMinute,entry->startSecond);
			entry->weeklyEndSeconds[count]   = GetWeeklySeconds(INX_TUE,entry->endHour,entry->endMinute,entry->endSecond);
			count++;
		}

		if (entry->dayIndex & INX_DAY_MASK_WEDNESDAY)
		{
			entry->weeklyStartSeconds[count] = GetWeeklySeconds(INX_WED,entry->startHour,entry->startMinute,entry->startSecond);
			entry->weeklyEndSeconds[count]   = GetWeeklySeconds(INX_WED,entry->endHour,entry->endMinute,entry->endSecond);
			count++;
		}

		if (entry->dayIndex & INX_DAY_MASK_THURSDAY)
		{
			entry->weeklyStartSeconds[count] = GetWeeklySeconds(INX_THU,entry->startHour,entry->startMinute,entry->startSecond);
			entry->weeklyEndSeconds[count]   = GetWeeklySeconds(INX_THU,entry->endHour,entry->endMinute,entry->endSecond);
			count++;
		}

		if (entry->dayIndex & INX_DAY_MASK_FRIDAY)
		{
			entry->weeklyStartSeconds[count] = GetWeeklySeconds(INX_FRI,entry->startHour,entry->startMinute,entry->startSecond);
			entry->weeklyEndSeconds[count]   = GetWeeklySeconds(INX_FRI,entry->endHour,entry->endMinute,entry->endSecond);
			count++;
		}

		if (entry->dayIndex & INX_DAY_MASK_SATURDAY)
		{
			entry->weeklyStartSeconds[count] = GetWeeklySeconds(INX_SAT,entry->startHour,entry->startMinute,entry->startSecond);
			entry->weeklyEndSeconds[count]   = GetWeeklySeconds(INX_SAT,entry->endHour,entry->endMinute,entry->endSecond);
			count++;
		}

		entry->weeklySecondCount = count;
		return;
	}

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



void Scheduler::PlayListLoadCheck(void)
{
  time_t currTime = time(NULL);
  struct tm now;
  
  localtime_r(&currTime, &now);

  int nowWeeklySeconds = GetWeeklySeconds(now.tm_wday, now.tm_hour, now.tm_min, now.tm_sec);
  if (m_nowWeeklySeconds2 != nowWeeklySeconds)
  {
    m_nowWeeklySeconds2 = nowWeeklySeconds;

    int diff = m_currentSchedulePlaylist.startWeeklySeconds - nowWeeklySeconds;
    int displayDiff = 0;

    if (diff < -600) // assume the schedule is actually next week for display
      diff += (24 * 3600 * 7);

    // If current schedule starttime is in the past and the item is not set
    // for repeat, then reschedule.
    if ((diff < -1) &&
        (!m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].repeat))
      ReLoadCurrentScheduleInfo();

    // Convoluted code to print the countdown more frequently as we get closer
    if (((diff > 300) &&                  ((diff % 300) == 0)) ||
        ((diff >  60) && (diff <= 300) && ((diff %  60) == 0)) ||
        ((diff >  10) && (diff <=  60) && ((diff %  10) == 0)) ||
        (                (diff <=  10)))
    {
      displayDiff = diff;
    }

    if (m_currentSchedulePlaylist.startWeeklySeconds && displayDiff)
      LogInfo(VB_SCHEDULE, "NowSecs = %d, CurrStartSecs = %d (%d seconds away)\n",
        nowWeeklySeconds,m_currentSchedulePlaylist.startWeeklySeconds, displayDiff);

    if(nowWeeklySeconds == m_currentSchedulePlaylist.startWeeklySeconds)
    {
      m_NextScheduleHasbeenLoaded = 0;
      strcpy(playlist->m_playlistDetails.currentPlaylistFile,m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].playList);
		  playlist->m_playlistDetails.currentPlaylistEntry=0;
			playlist->m_playlistDetails.repeat = m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].repeat;
		  playlist->m_playlistDetails.playlistStarting=1;
      LogInfo(VB_SCHEDULE, "Schedule Entry: %02d:%02d:%02d - %02d:%02d:%02d - Starting Playlist %s for %d seconds\n",
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].startHour,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].startMinute,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].startSecond,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].endHour,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].endMinute,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].endSecond,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].playList,
        m_currentSchedulePlaylist.endWeeklySeconds - m_currentSchedulePlaylist.startWeeklySeconds);
      LogInfo(VB_SCHEDULE, "NowSecs = %d, CurrStartSecs = %d, CurrEndSecs = %d (%d seconds away)\n",
        nowWeeklySeconds, m_currentSchedulePlaylist.startWeeklySeconds, m_currentSchedulePlaylist.endWeeklySeconds, displayDiff);
      FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;
    }
  }
}

void Scheduler::PlayListStopCheck(void)
{
  time_t currTime = time(NULL);
  struct tm now;
  
  localtime_r(&currTime, &now);

  int nowWeeklySeconds = GetWeeklySeconds(now.tm_wday, now.tm_hour, now.tm_min, now.tm_sec);

  if (m_nowWeeklySeconds2 != nowWeeklySeconds)
  {
    m_nowWeeklySeconds2 = nowWeeklySeconds;

    int diff = m_currentSchedulePlaylist.endWeeklySeconds - nowWeeklySeconds;
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
        nowWeeklySeconds, m_currentSchedulePlaylist.endWeeklySeconds, displayDiff);

    // This check for 1 second ago is a hack rather than a more invasive
    // patch to handle the race condition if we miss this check on the exact
    // second the schedule should be ending.  The odds of us missing 2 in a row
    // are much lower, so this will suffice for v1.0.
    if((nowWeeklySeconds == m_currentSchedulePlaylist.endWeeklySeconds) ||
       (nowWeeklySeconds == (m_currentSchedulePlaylist.endWeeklySeconds + 1)))
    {
      LogInfo(VB_SCHEDULE, "Schedule Entry: %02d:%02d:%02d - %02d:%02d:%02d - Stopping Playlist Gracefully\n",
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].startHour,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].startMinute,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].startSecond,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].endHour,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].endMinute,
        m_Schedule[m_currentSchedulePlaylist.ScheduleEntryIndex].endSecond);
      m_CurrentScheduleHasbeenLoaded = 0;
      playlist->StopPlaylistGracefully();
    }
  }

}

void Scheduler::LoadScheduleFromFile(void)
{
  FILE *fp;
  char buf[512];
  char *s;
  m_ScheduleEntryCount=0;
  int day;

  m_lastLoadDate = GetCurrentDateInt();

  pthread_mutex_lock(&m_scheduleLock);
  m_schedule.clear();

  LogInfo(VB_SCHEDULE, "Loading Schedule from %s\n",getScheduleFile());
  fp = fopen((const char *)getScheduleFile(), "r");
  if (fp == NULL) 
  {
		return;
  }
  while(fgets(buf, 512, fp) != NULL)
  {
	ScheduleEntry scheduleEntry;
	scheduleEntry.LoadFromString(buf);
	m_schedule.push_back(scheduleEntry);

    while (isspace(buf[strlen(buf)-1]))
        buf[strlen(buf)-1] = 0x0;

    // Enable
    s=strtok(buf,",");
    m_Schedule[m_ScheduleEntryCount].enable = atoi(s);
    // Playlist Name
    s=strtok(NULL,",");
    strcpy(m_Schedule[m_ScheduleEntryCount].playList,s);
    // Start Day index
    s=strtok(NULL,",");
    m_Schedule[m_ScheduleEntryCount].dayIndex = atoi(s);
    // Start Hour
    s=strtok(NULL,",");
    m_Schedule[m_ScheduleEntryCount].startHour = atoi(s);
    // Start Minute
    s=strtok(NULL,",");
    m_Schedule[m_ScheduleEntryCount].startMinute = atoi(s);
    // Start Second
    s=strtok(NULL,",");
    m_Schedule[m_ScheduleEntryCount].startSecond = atoi(s);
    // End Hour
    s=strtok(NULL,",");
    m_Schedule[m_ScheduleEntryCount].endHour = atoi(s);
    // End Minute
    s=strtok(NULL,",");
    m_Schedule[m_ScheduleEntryCount].endMinute = atoi(s);
    // End Second
    s=strtok(NULL,",");
    m_Schedule[m_ScheduleEntryCount].endSecond = atoi(s);
		// Repeat
    s=strtok(NULL,",");
    m_Schedule[m_ScheduleEntryCount].repeat = atoi(s);

    // Start Date
    s=strtok(NULL,",");
    if (s && strcmp(s, ""))
      m_Schedule[m_ScheduleEntryCount].startDate = DateStrToInt(s);
    else
      m_Schedule[m_ScheduleEntryCount].startDate = 20140101;

    // End Date
    s=strtok(NULL,",");
    if (s && strcmp(s, ""))
      m_Schedule[m_ScheduleEntryCount].endDate = DateStrToInt(s);
    else
      m_Schedule[m_ScheduleEntryCount].endDate = 20991231;

    // Set WeeklySecond start and end times
    SetScheduleEntrysWeeklyStartAndEndSeconds(&m_Schedule[m_ScheduleEntryCount]);
    m_ScheduleEntryCount++;
  }
  fclose(fp);

  pthread_mutex_unlock(&m_scheduleLock);

  SchedulePrint();

  return;
}

void Scheduler::SchedulePrint(void)
{
  int i=0;

  LogInfo(VB_SCHEDULE, "Current Schedule: (Status: '+' = Enabled, '-' = Disabled, '!' = Outside Date Range, '*' = Repeat)\n");
  LogInfo(VB_SCHEDULE, "St  Start & End Dates       Days         Start & End Times   Playlist\n");
  LogInfo(VB_SCHEDULE, "--- ----------------------- ------------ ------------------- ---------------------------------------------\n");
  for(i=0;i<m_ScheduleEntryCount;i++)
  {
    char dayStr[32];
    GetDayTextFromDayIndex(m_Schedule[i].dayIndex, dayStr);
    LogInfo(VB_SCHEDULE, "%c%c%c %04d-%02d-%02d - %04d-%02d-%02d %-12.12s %02d:%02d:%02d - %02d:%02d:%02d %s\n",
      m_Schedule[i].enable ? '+': '-',
      CurrentDateInRange(m_Schedule[i].startDate, m_Schedule[i].endDate) ? ' ': '!',
      m_Schedule[i].repeat ? '*': ' ',
      (int)(m_Schedule[i].startDate / 10000),
      (int)(m_Schedule[i].startDate % 10000 / 100),
      (int)(m_Schedule[i].startDate % 100),
      (int)(m_Schedule[i].endDate / 10000),
      (int)(m_Schedule[i].endDate % 10000 / 100),
      (int)(m_Schedule[i].endDate % 100),
      dayStr,
      m_Schedule[i].startHour,m_Schedule[i].startMinute,m_Schedule[i].startSecond,
      m_Schedule[i].endHour,m_Schedule[i].endMinute,m_Schedule[i].endSecond,
      m_Schedule[i].playList);
  }

  //////////////////////////////////////////////////////////////////////////
#if 0
  LogDebug(VB_SCHEDULE, "//////////////////////////////////////////////////\n");
  LogDebug(VB_SCHEDULE, "Now, the C++ version for comparison\n");
  pthread_mutex_lock(&m_scheduleLock);
  for (i = 0; i < m_schedule.size(); i++)
  {
    char dayStr[32];
    GetDayTextFromDayIndex(m_schedule[i].m_dayIndex, dayStr);
    LogInfo(VB_SCHEDULE, "%c%c%c %04d-%02d-%02d - %04d-%02d-%02d %-12.12s %02d:%02d:%02d - %02d:%02d:%02d %s\n",
      m_schedule[i].m_enabled ? '+': '-',
      CurrentDateInRange(m_schedule[i].m_startDate, m_schedule[i].m_endDate) ? ' ': '!',
      m_schedule[i].m_repeating ? '*': ' ',
      (int)(m_schedule[i].m_startDate / 10000),
      (int)(m_schedule[i].m_startDate % 10000 / 100),
      (int)(m_schedule[i].m_startDate % 100),
      (int)(m_schedule[i].m_endDate / 10000),
      (int)(m_schedule[i].m_endDate % 10000 / 100),
      (int)(m_schedule[i].m_endDate % 100),
      dayStr,
      m_schedule[i].m_startHour,m_schedule[i].m_startMinute,m_schedule[i].m_startSecond,
      m_schedule[i].m_endHour,m_schedule[i].m_endMinute,m_schedule[i].m_endSecond,
      m_schedule[i].m_playlistName.c_str());
  }
  pthread_mutex_unlock(&m_scheduleLock);
#endif
  LogDebug(VB_SCHEDULE, "//////////////////////////////////////////////////\n");
}

int Scheduler::GetWeeklySeconds(int day, int hour, int minute, int second)
{
  int weeklySeconds = (day*SECONDS_PER_DAY) + (hour*SECONDS_PER_HOUR) + (minute*SECONDS_PER_MINUTE) + second;
  return weeklySeconds;
}

int Scheduler::GetWeeklySecondDifference(int weeklySeconds1, int weeklySeconds2)
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

int Scheduler::GetDayFromWeeklySeconds(int weeklySeconds)
{
  return (int)(weeklySeconds/SECONDS_PER_DAY);
}

int Scheduler::FindNextStartingScheduleIndex(void)
{
	int i = 0;
	int index = -1;
	int currentDate = GetCurrentDateInt();
	int schedDate = 99999999;
	int weeklySecondStart = 999999;
	for(i = 0; i < m_ScheduleEntryCount; i++)
	{
		if ((m_Schedule[i].enable) &&
			(m_Schedule[i].startDate >= currentDate) &&
			(m_Schedule[i].endDate < schedDate))
		{
			index = i;
			schedDate = m_Schedule[i].endDate;
		}
	}

	return index;
}

void Scheduler::GetNextScheduleStartText(char * txt)
{
	if (!m_NextScheduleHasbeenLoaded)
		return;

	if (m_nextSchedulePlaylist.ScheduleEntryIndex >= 0)
	{
		GetScheduleEntryStartText(m_nextSchedulePlaylist.ScheduleEntryIndex,m_nextSchedulePlaylist.weeklySecondIndex,txt);
	}
	else
	{
		char dayText[16];
		int found = FindNextStartingScheduleIndex();

		if (found >= 0)
		{
			GetDayTextFromDayIndex(m_Schedule[found].dayIndex,dayText);
			sprintf(txt, "%04d-%02d-%02d @ %02d:%02d:%02d - (%s)\n",
				(int)(m_Schedule[found].startDate / 10000),
				(int)(m_Schedule[found].startDate % 10000 / 100),
				(int)(m_Schedule[found].startDate % 100),
				m_Schedule[found].startHour,
				m_Schedule[found].startMinute,
				m_Schedule[found].startSecond,
				dayText);
		}
	}
}

void Scheduler::GetNextPlaylistText(char * txt)
{
	if (!m_NextScheduleHasbeenLoaded)
		return;

	if (m_nextSchedulePlaylist.ScheduleEntryIndex >= 0)
	{
		strcpy(txt,m_Schedule[m_nextSchedulePlaylist.ScheduleEntryIndex].playList);
	}
	else
	{
		int found = FindNextStartingScheduleIndex();

		if (found >= 0)
			strcpy(txt,m_Schedule[found].playList);
	}
}

void Scheduler::GetScheduleEntryStartText(int index,int weeklySecondIndex, char * txt)
{
		char text[64];
		char dayText[16];
    int dayIndex = GetDayFromWeeklySeconds(m_Schedule[index].weeklyStartSeconds[weeklySecondIndex]);
    GetDayTextFromDayIndex(dayIndex,text);
		if(m_Schedule[index].dayIndex > INX_SAT)
		{
			GetDayTextFromDayIndex(m_Schedule[index].dayIndex,dayText);
			sprintf(txt,"%s @ %02d:%02d:%02d - (%s)",
							text,m_Schedule[index].startHour,m_Schedule[index].startMinute,m_Schedule[index].startSecond,dayText);
		}
		else
		{
			sprintf(txt,"%s @ %02d:%02d:%02d",
							text,m_Schedule[index].startHour,m_Schedule[index].startMinute,m_Schedule[index].startSecond);
		}		
}

void Scheduler::GetDayTextFromDayIndex(int index,char * txt)
{
	if (index & INX_DAY_MASK)
	{
		strcpy(txt,"Day Mask");
		return;
	}

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
