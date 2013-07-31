#include "mpg123.h"
#include "fpp.h"
#include "E131.h"
#include "playList.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

char * musicFolder = "/home/pi/media/music/";
char * silenceMusic = "/home/pi/media/music/silence.mp3";
char MPG123volume[4];

char currentSong[128];
char currentSongPath[128];
int lastSecond=0;

int MusicPlayerStatus = IDLE_MPLAYER_STATUS;
char mCommand[150];
extern char logText[256];

struct mpg123_type mpg123;

struct mpg123_type mpg123_init( ) {
   int toprog[2];
   int fromprog[2];
   pid_t childpid;
   char buf[256];
   struct mpg123_type prog;

   pipe(toprog);
   pipe(fromprog);

   childpid = fork();
   
   if ( childpid == 0 ) {
      close(toprog[1]);
      close(fromprog[0]);
      if ( dup2(toprog[0],0) == -1 ) {
         perror("dup2");
      }
      if ( dup2(fromprog[1],1) == -1 ) {
         perror("dup2");
      }
      execlp("/usr/bin/mpg123", "mpg123", "-R", ".", NULL);
      perror("execl");
      exit(0);
   } else {
      close(toprog[0]);
      close(fromprog[1]);
      prog.topipe = toprog[1];
      prog.frompipe = fromprog[0];
      prog.outlength = 0;
      prog.out[0] = '\0';
      prog.seconds = 0;
      prog.mpg123_pid = childpid;
      if ( fcntl(prog.frompipe, F_SETFL, O_NONBLOCK) == -1 ) {
         perror("fcntl");
      }
      read(prog.frompipe, buf, 10);
   }
   return prog;
}

void mpg123_cmd( struct mpg123_type prog, int cmd, char *arg)
{
   
   switch (cmd) {
      case CMD_PLAY:
         if ( mpg123.playstat == PLAY_PAUSE ) {
            strcpy(mCommand, "PAUSE\n");
         } else {
            sprintf(mCommand, "LOAD %s\n", arg);
         }
         mpg123.playstat = PLAY_PLAY;
         break;
      case CMD_STOP:
         strcpy(mCommand, "STOP\n");
         break;
      case CMD_QUIT:
         strcpy(mCommand, "QUIT\n");
         break;
      case CMD_JUMP:
         sprintf(mCommand, "JUMP %s\n", arg);
         break;
      case CMD_PAUSE:
         strcpy(mCommand, "PAUSE\n");
         break;
      case CMD_VOLUME:
         sprintf(mCommand, "VOLUME %s\n", arg);
				 LogWrite(logText);
         break;
      default:
         sprintf(logText,"mpg123_cmd(): uknown command: %d\n", cmd);
         LogWrite(logText);
         return;
      
   }
   write(prog.topipe, mCommand, strlen(mCommand));
}

struct mpg123_type mpg123_proc( struct mpg123_type prog ) {
   char buf[2], tmp[2];
   int i, field, len;
   char seconds[10], secondsleft[10], frames[10], framesleft[10];

   while ( read(prog.frompipe, buf, 1) == 1 ) {
      if ( buf[0] != '\n' ) {
         sprintf(prog.out, "%s%c", prog.out, buf[0]);
         prog.outlength++;
      } else {
         if ( prog.out[0] != '@' ) {
         } else {
            switch (prog.out[1]) {
               case 'R':
                  break;
               case 'I':
                  break;
               case 'S':
                  break;
               case 'F':
                  field = 0;
                  seconds[0] = '\0';
                  secondsleft[0] = '\0';
                  frames[0] = '\0';
                  len = strlen(prog.out);
                  for ( i = 3; i < len; i++ ) {
                     if ( prog.out[i] == ' ' ) {
                        field++;
                     } else {
                        if ( field == 0 ) {
                          sprintf(frames, "%s%c", frames, prog.out[i]);
                        }
                        if ( field == 1 ) {
                          sprintf(framesleft, "%s%c", framesleft, prog.out[i]);
                        }
                        if ( field == 2 ) {
                          sprintf(seconds, "%s%c", seconds,prog.out[i]);
                        }
                        if ( field == 3 ) {
                          sprintf(secondsleft, "%s%c", secondsleft,prog.out[i]);
                        }
                     }
                  }
                  prog.seconds = atof(seconds);
                  prog.secondsleft = atof(secondsleft);
                  prog.frames = atoi(frames);
                  prog.framesleft = atoi(framesleft);
                  break;
               case 'P':
                  tmp[0] = prog.out[3];
                  tmp[1] = '\0';
                  prog.playstat = atoi(tmp);
                  break;
               case 'E':
                  sprintf(logText,"Error: %s\n", prog.out);
                  LogWrite(logText);
                  break;
               default:
                  sprintf(logText,"unknown response: %s\n", prog.out);
                  LogWrite(logText);
                  break;
            }
         }
         prog.outlength = 0;
         prog.out[0] = '\0';
      }
   }
   return prog;
}

void  MPG_PlaySong()
{
    MusicPlayerStatus = QUEUED_MPLAYER_STATUS;
    sprintf(logText,"Changing Status to Queued\n");
    LogWrite(logText);
    mpg123.playstat = PLAY_PLAY;
    // Create for path of song 
    strcpy(currentSongPath,musicFolder);
    strcat(currentSongPath,currentSong);
    sprintf(logText,"Starting Song = %s",currentSongPath);
    LogWrite(logText);
    // Send command to play song    
    mpg123_cmd(mpg123,CMD_PLAY,currentSongPath);
    lastSecond = (int)mpg123.seconds;
}

void MPG_StopSong(void)
{
  sprintf(logText,"Changing Status to stop\n");
  LogWrite(logText);
  mpg123_cmd(mpg123,CMD_STOP,NULL);
	MusicPlayerStatus = IDLE_MPLAYER_STATUS;
}

void MPG_SetVolume(char * volume)
{
  sprintf(logText,"Setting Volume\n");
  LogWrite(logText);
  mpg123_cmd(mpg123,CMD_VOLUME,volume);
}


void MPG_UpdateStatus()
{
    mpg123 = mpg123_proc(mpg123);
    switch(MusicPlayerStatus)
    {
      case QUEUED_MPLAYER_STATUS:
        if(lastSecond !=(int)mpg123.seconds)
        {
          lastSecond = (int)mpg123.seconds;
          sprintf(logText,"Seconds= %g Remaining=%g\n\r",mpg123.seconds,mpg123.secondsleft);
          LogWrite(logText);
          MusicPlayerStatus = PLAYING_MPLAYER_STATUS;
          sprintf(logText,"Changing Statusssss to play\n");
          LogWrite(logText);
        }
        break;
      case PLAYING_MPLAYER_STATUS:
        if(mpg123.secondsleft < .1)
        {
          MusicPlayerStatus=IDLE_MPLAYER_STATUS;
					E131_CloseSequenceFile();
          sprintf(logText,"Changing Status to idle\n");
          LogWrite(logText);
        }
        break;
      default:
        break;
    }
}


void MusicInitialize(void)
{
  int err;
  mpg123 = mpg123_init();
}

