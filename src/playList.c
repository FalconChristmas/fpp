/*
 *   Play list handler for Falcon Pi Player (FPP)
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
#include "E131.h"
#include "command.h"
#include "events.h"
#include "log.h"
#include "mediaoutput.h"
#include "playList.h"
#include "schedule.h"
#include "settings.h"
#include "sequence.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

PlaylistDetails playlistDetails;
char * pl = "playlist1.lst";


extern int MusicCommand;
extern int MusicResponse;
extern int MusicPlayerStatus;
extern char currentSong[128];//FIXME
extern char nextSong[128];//FIXME
extern int lastSecond;


struct timeval pauseStartTime;
int numberOfSecondsPaused;
struct timeval nowTime;
int pauseStatus = PAUSE_STATUS_IDLE;

extern char logText[256];

void CalculateNextPlayListEntry()
{
	int maxEntryIndex;
	int firstEntryIndex;
	int lastEntry;
	if(playlistDetails.playlistStarting)
	{
		// Do not change "playlistDetails.currentPlaylistEntry"
		playlistDetails.playlistStarting=0;
		LogInfo(VB_PLAYLIST, "Playlist Starting\n");
		return;
	}
	else if(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY)
	{
		lastEntry = playlistDetails.last?playlistDetails.playListCount-1:PLAYLIST_STOP_INDEX;
		playlistDetails.currentPlaylistEntry = playlistDetails.currentPlaylistEntry == playlistDetails.playListCount-1 ? PLAYLIST_STOP_INDEX:lastEntry;
	}
	else
	{	
		maxEntryIndex = playlistDetails.last?playlistDetails.playListCount-1:playlistDetails.playListCount; 
		playlistDetails.currentPlaylistEntry++;
		LogDebug(VB_PLAYLIST, "currentPlaylistEntry = %d Last=%d maxEntryIndex=%d repeat=%d \n",playlistDetails.currentPlaylistEntry, playlistDetails.last,maxEntryIndex,playlistDetails.repeat); 
		if((playlistDetails.currentPlaylistEntry == maxEntryIndex) && !playlistDetails.repeat)
		{
			LogInfo(VB_PLAYLIST, "Stopping Gracefully\n");
			playlistDetails.currentPlaylistEntry = PLAYLIST_STOP_INDEX;
		}
 		else if(playlistDetails.currentPlaylistEntry >= maxEntryIndex)
		{
			// Calculate where start index is.
			firstEntryIndex = playlistDetails.first?1:0; 
			playlistDetails.currentPlaylistEntry = firstEntryIndex;
		}
	}
}


int ReadPlaylist(char const * file)
{
  FILE *fp;
  int listIndex=0;
  char buf[512];
  char *s;
  // Put together playlist file with default folder
  strcpy((char*)playlistDetails.currentPlaylist,(const char *)getPlaylistDirectory());
  strcat((char*)playlistDetails.currentPlaylist,"/");
  strcat((char*)playlistDetails.currentPlaylist,file);

  LogInfo(VB_PLAYLIST, "Opening File Now %s\n",playlistDetails.currentPlaylist);
  fp = fopen((const char*)playlistDetails.currentPlaylist, "r");
  if (fp == NULL) 
  {
    LogErr(VB_PLAYLIST, "Could not open playlist file %s\n",file);
  return 0;
  }
	// Parse Playlist settings (First, Last)
	fgets(buf, 512, fp);
  s=strtok(buf,",");
	playlistDetails.first = atoi(s);
  s = strtok(NULL,",");
	playlistDetails.last = atoi(s);

	// Parse Playlists 
  while(fgets(buf, 512, fp) != NULL)
  {
    s=strtok(buf,",");
    playlistDetails.playList[listIndex].cType = s[0];
    switch(s[0])
    {
      case 'b':
        s = strtok(NULL,",");
        strcpy(playlistDetails.playList[listIndex].seqName,s);
        s = strtok(NULL,",");
        strcpy(playlistDetails.playList[listIndex].songName,s);
        playlistDetails.playList[listIndex].type = PL_TYPE_BOTH;
        break;
      case 's':
        s = strtok(NULL,",");
        strcpy(playlistDetails.playList[listIndex].seqName,s);
        playlistDetails.playList[listIndex].type = PL_TYPE_SEQUENCE;
        break;
      case 'm':
      case 'v':
        s = strtok(NULL,",");
        strcpy(playlistDetails.playList[listIndex].songName,s);
        playlistDetails.playList[listIndex].type = PL_TYPE_MEDIA;
        break;
      case 'p':
        s = strtok(NULL,",");
        playlistDetails.playList[listIndex].pauselength = atoi(s);
        playlistDetails.playList[listIndex].type = PL_TYPE_PAUSE;
        break;
      case 'e':
        s = strtok(NULL,",");
        strcpy(playlistDetails.playList[listIndex].eventID,s);
        s = strtok(NULL,",");
        playlistDetails.playList[listIndex].pauselength = atoi(s);
        playlistDetails.playList[listIndex].type = PL_TYPE_EVENT;
        break;
      default:
        LogErr(VB_PLAYLIST, "Invalid entry in sequence file %s\n",file);
        return 0;
        break;
    }
    listIndex++;
  }
  fclose(fp);
  return listIndex;
}

void PlayListPlayingLoop(void)
{
	LogInfo(VB_PLAYLIST, "Starting PlaylistPlaying loop\n");
  playlistDetails.StopPlaylist = 0;
	playlistDetails.ForceStop = 0;
  playlistDetails.playListCount = ReadPlaylist((char*)playlistDetails.currentPlaylistFile);
	if(playlistDetails.playListCount == 0)
	{
		LogInfo(VB_PLAYLIST, "PlaylistCount = 0. Exiting PlayListPlayingLoop\n");
		FPPstatus = FPP_STATUS_IDLE;
		return;
	}
	
  if(playlistDetails.currentPlaylistEntry < 0 || playlistDetails.currentPlaylistEntry >= playlistDetails.playListCount)
	{
		LogErr(VB_PLAYLIST, "currentPlaylistEntry is not valid\n");
		FPPstatus = FPP_STATUS_IDLE;
		return;
	}
  while(!playlistDetails.StopPlaylist)
  {
    usleep(10000);
    switch(playlistDetails.playList[playlistDetails.currentPlaylistEntry].type)
    {
      case PL_TYPE_BOTH:
        if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_IDLE)
        {
					LogInfo(VB_PLAYLIST, "Play File Now \n");
          Play_PlaylistEntry();
        }
				else
				{
					if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING)
					{
						pthread_mutex_lock(&mediaOutputLock);
						if (mediaOutput)
							mediaOutput->processData();
						pthread_mutex_unlock(&mediaOutputLock);
    			}
				}
        break;
      case PL_TYPE_MEDIA:
        if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_IDLE)
        {
          Play_PlaylistEntry();
        }
				else
				{
					if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING)
  			  {
						pthread_mutex_lock(&mediaOutputLock);
						if (mediaOutput)
							mediaOutput->processData();
						pthread_mutex_unlock(&mediaOutputLock);
    			}
				}
        break;
      case PL_TYPE_SEQUENCE:
        if(!IsSequenceRunning())
        {
          Play_PlaylistEntry();
        }
        break;
      case PL_TYPE_PAUSE:
        PauseProcess();
        if(pauseStatus==PAUSE_STATUS_ENDED)
        {
          Play_PlaylistEntry();
          pauseStatus = PAUSE_STATUS_IDLE;
        }
        break;
      case PL_TYPE_VIDEO:
        Play_PlaylistEntry();
        break;
      case PL_TYPE_EVENT:
        Play_PlaylistEntry();
        break;
      default:
        break;
    }
    Commandproc();
    ScheduleProc();
  }
  FPPstatus = FPP_STATUS_IDLE;
  SendBlankingData();
  ReLoadCurrentScheduleInfo();

	if(!playlistDetails.ForceStop)
	{
		CheckIfShouldBePlayingNow();
	}
}


void PauseProcess(void)
{
  switch(pauseStatus)
  {
    case PAUSE_STATUS_IDLE:
      pauseStatus = PAUSE_STATUS_STARTED;
      gettimeofday(&pauseStartTime,NULL);
      break;
    case PAUSE_STATUS_STARTED:
      gettimeofday(&nowTime,NULL);
			numberOfSecondsPaused = nowTime.tv_sec - pauseStartTime.tv_sec;
      if(numberOfSecondsPaused >  (int)playlistDetails.playList[playlistDetails.currentPlaylistEntry].pauselength)
      {
        pauseStatus = PAUSE_STATUS_ENDED;
      }
      break;
    default:
      break;
  }
}


void Play_PlaylistEntry(void)
{
  PlaylistEntry *plEntry = NULL;

  CalculateNextPlayListEntry();
	if( playlistDetails.currentPlaylistEntry==PLAYLIST_STOP_INDEX)
	{
		LogInfo(VB_PLAYLIST, "Stopping Playlist\n"); 
		playlistDetails.StopPlaylist = 1;
		return;
	}

	LogDebug(VB_PLAYLIST, "playListCount=%d  CurrentPlaylistEntry = %d\n", playlistDetails.playListCount,playlistDetails.currentPlaylistEntry);

  plEntry = &playlistDetails.playList[playlistDetails.currentPlaylistEntry];
  switch(plEntry->type)
  {
    case PL_TYPE_BOTH:
      if (OpenSequenceFile(plEntry->seqName) > 0)
			{
	      OpenMediaOutput(playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName);
      }
			else
			{
				CalculateNextPlayListEntry();
			}
			break;
    case PL_TYPE_MEDIA:
      OpenMediaOutput(playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName);
      break;
    case PL_TYPE_SEQUENCE:
      OpenSequenceFile(plEntry->seqName);
      break;
    case PL_TYPE_PAUSE:
      break;
    case PL_TYPE_EVENT:
      TriggerEventByID(plEntry->eventID);
      break;
  }
}

void PlaylistPrint()
{
  int i=0;
  LogDebug(VB_PLAYLIST, "playListCount=%d\n",playlistDetails.playListCount);
  for(i=0;i<playlistDetails.playListCount;i++)
  {
    LogDebug(VB_PLAYLIST, "type=%d\n",playlistDetails.playList[i].type);
    LogDebug(VB_PLAYLIST, "seqName=%s\n",playlistDetails.playList[i].seqName);
    LogDebug(VB_PLAYLIST, "SongName=%s\n",playlistDetails.playList[i].songName);
    LogDebug(VB_PLAYLIST, "pauselength=%d\n",playlistDetails.playList[i].pauselength);
  }
}

void StopPlaylistGracefully(void)
{
  FPPstatus = FPP_STATUS_STOPPING_GRACEFULLY;
}

void StopPlaylistNow(void)
{
  LogInfo(VB_PLAYLIST, "StopPlaylistNow()\n");
  FPPstatus = FPP_STATUS_IDLE;
  CloseSequenceFile();
	CloseMediaOutput();
  playlistDetails.StopPlaylist = 1;
}

void JumpToPlaylistEntry(int entryIndex)
{

}

