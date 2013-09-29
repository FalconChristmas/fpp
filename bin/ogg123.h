#ifndef _OGG123_H
#define _OFF123_H

#ifndef TYPES_H
	#include <sys/types.h>
#endif

#define PIPE_READ    	0
#define PIPE_WRITE   	1
#define MAX_BYTES_OGG 1000
#define TIME_STR_MAX  8

void sigchld_handler(int signal);
int oggInit();
int oggPlaySong(char * musicFile);
int oggRunning();
int MusicProc();
void OGGstopSong();
void PollMusicInfo();
void ProcessOGGData(int bytesRead);
void ParseTimes();

typedef struct{
  int MusicPlayerStatus; 
  int secondsElasped;
  int subSecondsElasped;
  int secondsRemaining;
  int subSecondsRemaining;
  int minutesTotal;
  int secondsTotal;
}MusicStatus; 

extern int MusicPlayerStatus;
extern MusicStatus musicStatus;

#define IDLE_MPLAYER_STATUS     0
#define QUEUED_MPLAYER_STATUS   1
#define PLAYING_MPLAYER_STATUS  2

#endif
