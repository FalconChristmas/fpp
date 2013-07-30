#ifndef _MPG123_H
#define _MPG123_H

#ifndef TYPES_H
	#include <sys/types.h>
#endif


struct mpg123_type {
   int topipe;
   int frompipe;
   int outlength;
   char out[255];
   float seconds;
   float secondsleft;
   int frames;
   int framesleft;
   pid_t mpg123_pid;
   int playstat;
   float bar;
};

struct mpg123_type mpg123_init();

void mpg123_cmd( struct mpg123_type, int, char *);

struct mpg123_type mpg123_proc( struct mpg123_type );
void* MusicThread(void *arg);
void MusicInitialize(void);
void StopMusic(int k);
void  MPG_PlaySong();
void MPG_StopSong(void);
void MPG_SetVolume(char * volume);
void MPG_UpdateStatus();

#ifdef CMD_PLAY
#error CMD_PLAY defined
#endif

#define CMD_NOP  0
#define CMD_PLAY 1
#define CMD_STOP 2
#define CMD_QUIT 3
#define CMD_JUMP 4
#define CMD_PAUSE 5
#define CMD_VOLUME 6

#define PLAY_STOP 0
#define PLAY_PLAY 2
#define PLAY_PAUSE 1

#define CMD_IDLE_RESPONSE       0
#define CMD_EXECUTED_RESPONSE   1


#define IDLE_MPLAYER_STATUS     0
#define QUEUED_MPLAYER_STATUS   1
#define PLAYING_MPLAYER_STATUS  2

#endif