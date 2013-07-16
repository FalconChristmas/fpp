#include "fpp.h"
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

char currentPlaylist[128];
PlaylistEntry playList[32];
int playListCount;
int currentPlaylistEntry=1;
int nextPlaylistEntry=1;
char firstTimeThrough = 0;

extern unsigned long currentSequenceFileSize;

int StopPlaylist=0;

char currentPlaylist[128];
char currentPlaylistFile[128];
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

void CheckExistanceOfDirectories()
{
	if(!DirectoryExists("/home/pi/media"))
	{
		mkdir("/home/pi/media", 0755);
		sprintf(logText,"Directory FPP Does Not Exist\n");
		LogWrite(logText);
	}
	if(!DirectoryExists("/home/pi/media/music"))
	{
		mkdir("/home/pi/media/music", 0755);
		sprintf(logText,"Directory Music Does Not Exist\n");
		LogWrite(logText);
	}
	if(!DirectoryExists("/home/pi/media/sequences"))
	{
		mkdir("/home/pi/media/sequences", 0755);
		sprintf(logText,"Directory sequences Does Not Exist\n");
		LogWrite(logText);
	}
	if(!DirectoryExists("/home/pi/media/playlists"))
	{
		mkdir("/mnt/media/playlists", 0755);
		sprintf(logText,"Directory playlists Does Not Exist\n");
		LogWrite(logText);
	}
	if(!FileExists("/mnt/media/universes"))
	{
		system("touch /mnt/media/universes");
	}
	if(!FileExists("/mnt/media/pixelnetDMX"))
	{
		system("touch /mnt/media/pixelnetDMX");
	}
	if(!FileExists("/mnt/media/schedule"))
	{
		system("touch /mnt/media/schedule");
	}

}

int ReadPlaylist(char const * file)
{
  FILE *fp;
  int listIndex=0;
  char buf[512];
  char *s;
	CheckExistanceOfDirectories();
  // Put together playlist file with default folder
  strcpy(currentPlaylist,playlistFolder);
  strcat(currentPlaylist,file);

  sprintf(logText,"Opening File Now %s\n",currentPlaylist);
  LogWrite(logText);
  fp = fopen(currentPlaylist, "r");
  if (fp == NULL) 
  {
    sprintf(logText,"Could not open playlist file %s\n",file);
    LogWrite(logText);
  return 0;
  }
  while(fgets(buf, 512, fp) != NULL)
  {
    s=strtok(buf,",");
    playList[listIndex].cType = s[0];
    switch(s[0])
    {
      case 'b':
        s = strtok(NULL,",");
        strcpy(playList[listIndex].seqName,s);
        s = strtok(NULL,",");
        strcpy(playList[listIndex].songName,s);
        playList[listIndex].type = PL_TYPE_BOTH;
        break;
      case 's':
        s = strtok(NULL,",");
        strcpy(playList[listIndex].seqName,s);
        playList[listIndex].type = PL_TYPE_SEQUENCE;
        break;
      case 'm':
        s = strtok(NULL,",");
        strcpy(playList[listIndex].songName,s);
        playList[listIndex].type = PL_TYPE_MUSIC;
        break;
      case 'p':
        s = strtok(NULL,",");
        playList[listIndex].pauselength = atoi(s);
        playList[listIndex].type = PL_TYPE_PAUSE;
        break;
      default:
        sprintf(logText,"Invalid entry in sequence file %s\n",file);
        LogWrite(logText);
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
  StopPlaylist = 0;
  playListCount = ReadPlaylist(currentPlaylistFile);
  currentPlaylistEntry=0;
  nextPlaylistEntry=0;
	firstTimeThrough = 1;
  while(!StopPlaylist)
  {
    usleep(100000);
    switch(playList[currentPlaylistEntry].type)
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
          sprintf(logText,"Play_PlaylistEntry_Music=%d\n",currentPlaylistEntry);
          LogWrite(logText);
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
          sprintf(logText,"Play_PlaylistEntry_seq=%d\n",currentPlaylistEntry);
          LogWrite(logText);
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
      sprintf(logText,"%d Pause Started\n",playList[currentPlaylistEntry].pauselength);
      LogWrite(logText);
      gettimeofday(&pauseStartTime,NULL);
      break;
    case PAUSE_STATUS_STARTED:
      gettimeofday(&nowTime,NULL);
			numberOfSecondsPaused = nowTime.tv_sec - pauseStartTime.tv_sec;
      if(numberOfSecondsPaused >  (int)playList[currentPlaylistEntry].pauselength)
      {
          pauseStatus = PAUSE_STATUS_ENDED;
          sprintf(logText,"%d Pause ended\n",playList[currentPlaylistEntry].pauselength);
          LogWrite(logText);
      }
      break;
    default:
      break;
  }
}


void Play_PlaylistEntry(void)
{
  currentPlaylistEntry = nextPlaylistEntry;
  switch(playList[currentPlaylistEntry].type)
  {
    case PL_TYPE_BOTH:
      currentSequenceFileSize=E131_OpenSequenceFile(playList[currentPlaylistEntry].seqName);
      PlaylistPlaySong();
      break;
    case PL_TYPE_MUSIC:
      PlaylistPlaySong();
      break;
    case PL_TYPE_SEQUENCE:
      currentSequenceFileSize=E131_OpenSequenceFile(playList[currentPlaylistEntry].seqName);
      sprintf(logText,"seqFileSize=%lu\n",currentSequenceFileSize);
      LogWrite(logText);
      break;
    case PL_TYPE_PAUSE:
      break;
  }
  nextPlaylistEntry++;
	//if(nextPlaylistEntry==playListCount)
	//{
		//printf("NextPlaylistEntry = %d, firstTimeThrough = %d\n", nextPlaylistEntry,firstTimeThrough); 
		//if(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY && nextPlaylistEntry==playListCount && firstTimeThrough==0)
		//{ 
			//printf("StopPlaylist - NextPlaylistEntry = %d, firstTimeThrough = %d\n", nextPlaylistEntry,firstTimeThrough); 
		//	StopPlaylist = 1;
		//	return;     
		//}
		//firstTimeThrough=0;
	//}
  nextPlaylistEntry = nextPlaylistEntry%playListCount;

}


void PlaylistPlaySong(void)
{
  strcpy(currentSong,playList[currentPlaylistEntry].songName);
	MPG_PlaySong();
}

void PlaylistStopSong(void)
{
	MPG_StopSong();
}



void PlaylistPrint()
{
  int i=0;
  sprintf(logText,"playListCount=%d\n",playListCount);
  LogWrite(logText);
  for(i=0;i<playListCount;i++)
  {
    sprintf(logText,"type=%d\n",playList[i].type);
    LogWrite(logText);
    sprintf(logText,"seqName=%s\n",playList[i].seqName);
    LogWrite(logText);
    sprintf(logText,"SongName=%s\n",playList[i].songName);
    LogWrite(logText);
    sprintf(logText,"pauselength=%d\n",playList[i].pauselength);
    LogWrite(logText);
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
  StopPlaylist = 1;
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