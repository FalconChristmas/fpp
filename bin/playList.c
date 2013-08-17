#include "fpp.h"
#include "log.h"
#include "playList.h"
#include "command.h"
#include "E131.h"
#include "schedule.h"
#include "mpg123.h"
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

char * playlistFolder = "/home/pi/media/playlists/";

PlaylistDetails playlistDetails;
extern unsigned long currentSequenceFileSize;
char currentSequenceFile[128];
char * pl = "playlist1.lst";


extern int E131status;

extern int MusicCommand;
extern int MusicResponse;
extern int MusicPlayerStatus;
extern char currentSong[128];
extern char nextSong[128];
extern int lastSecond;
extern int FPPstatus;


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
		//printf("Last=%d maxEntryIndex=%d\n", playlistDetails.last,maxEntryIndex); 
		playlistDetails.currentPlaylistEntry++;
 		if(playlistDetails.currentPlaylistEntry >= maxEntryIndex)
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
  strcpy(playlistDetails.currentPlaylist,playlistFolder);
  strcat(playlistDetails.currentPlaylist,file);

  LogWrite("Opening File Now %s\n",playlistDetails.currentPlaylist);
  fp = fopen(playlistDetails.currentPlaylist, "r");
  if (fp == NULL) 
  {
    LogWrite("Could not open playlist file %s\n",file);
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
        s = strtok(NULL,",");
        strcpy(playlistDetails.playList[listIndex].songName,s);
        playlistDetails.playList[listIndex].type = PL_TYPE_MUSIC;
        break;
      case 'p':
        s = strtok(NULL,",");
        playlistDetails.playList[listIndex].pauselength = atoi(s);
        playlistDetails.playList[listIndex].type = PL_TYPE_PAUSE;
        break;
      default:
        LogWrite("Invalid entry in sequence file %s\n",file);
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
  playlistDetails.StopPlaylist = 0;
  playlistDetails.playListCount = ReadPlaylist(playlistDetails.currentPlaylistFile);
  if(playlistDetails.currentPlaylistEntry < 0 || playlistDetails.currentPlaylistEntry >= playlistDetails.playListCount)
	{
		playlistDetails.currentPlaylistEntry = 0;
	}
  while(!playlistDetails.StopPlaylist)
  {
    usleep(10000);
    switch(playlistDetails.playList[playlistDetails.currentPlaylistEntry].type)
    {
      case PL_TYPE_BOTH:
        if(MusicPlayerStatus == IDLE_MPLAYER_STATUS)
        {
          Play_PlaylistEntry();
        }
				else
				{
		    	if(MusicPlayerStatus==PLAYING_MPLAYER_STATUS || MusicPlayerStatus==QUEUED_MPLAYER_STATUS)
					{
      			MPG_UpdateStatus();
    			}
				}
        break;
      case PL_TYPE_MUSIC:
        if(MusicPlayerStatus == IDLE_MPLAYER_STATUS)
        {
          Play_PlaylistEntry();
        }
				else
				{
			    if(MusicPlayerStatus==PLAYING_MPLAYER_STATUS || MusicPlayerStatus==QUEUED_MPLAYER_STATUS)
  			  {
      			MPG_UpdateStatus();
    			}
				}
        break;
      case PL_TYPE_SEQUENCE:
        if(E131status == E131_STATUS_IDLE)
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
      default:
        break;
    }
    Commandproc();
    ScheduleProc();
  }
  FPPstatus = FPP_STATUS_IDLE;
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
  CalculateNextPlayListEntry();
	if( playlistDetails.currentPlaylistEntry==PLAYLIST_STOP_INDEX)
	{
		if(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY)
		{ 
			printf("Changing Status to Stopping Gracefully\n"); 
			playlistDetails.StopPlaylist = 1;
			return;
		}
	}

	printf("\nplayListCount=%d  CurrentPlaylistEntry = %d\n", playlistDetails.playListCount,playlistDetails.currentPlaylistEntry); 
  switch(playlistDetails.playList[playlistDetails.currentPlaylistEntry].type)
  {
    case PL_TYPE_BOTH:
      currentSequenceFileSize=E131_OpenSequenceFile(playlistDetails.playList[playlistDetails.currentPlaylistEntry].seqName);
      PlaylistPlaySong();
      break;
    case PL_TYPE_MUSIC:
      PlaylistPlaySong();
      break;
    case PL_TYPE_SEQUENCE:
      currentSequenceFileSize=E131_OpenSequenceFile(playlistDetails.playList[playlistDetails.currentPlaylistEntry].seqName);
      LogWrite("seqFileSize=%lu\n",currentSequenceFileSize);
      break;
    case PL_TYPE_PAUSE:
      break;
  }

}


void PlaylistPlaySong(void)
{
  strcpy(currentSong,playlistDetails.playList[playlistDetails.currentPlaylistEntry].songName);
	MPG_PlaySong();
}

void PlaylistStopSong(void)
{
	MPG_StopSong();
}



void PlaylistPrint()
{
  int i=0;
  LogWrite("playListCount=%d\n",playlistDetails.playListCount);
  for(i=0;i<playlistDetails.playListCount;i++)
  {
    LogWrite("type=%d\n",playlistDetails.playList[i].type);
    LogWrite("seqName=%s\n",playlistDetails.playList[i].seqName);
    LogWrite("SongName=%s\n",playlistDetails.playList[i].songName);
    LogWrite("pauselength=%d\n",playlistDetails.playList[i].pauselength);
  }
}

void StopPlaylistGracefully(void)
{
  FPPstatus = FPP_STATUS_STOPPING_GRACEFULLY;
}

void StopPlaylistNow(void)
{
  FPPstatus = FPP_STATUS_IDLE;
  E131_CloseSequenceFile();
  PlaylistStopSong();
  playlistDetails.StopPlaylist = 1;
}

void JumpToPlaylistEntry(int entryIndex)
{

}


int DirectoryExists(char * Directory)
{
	DIR* dir = opendir(Directory);
	if (dir)
	{
			return 1;
	}
	else
	{
			return 0;
	}
}


int FileExists(char * File)
{
	struct stat sts;
	if (stat(File, &sts) == -1 && errno == ENOENT)
	{
			return 0;
	}
	else
	{
		return 1;
	}
	
}
