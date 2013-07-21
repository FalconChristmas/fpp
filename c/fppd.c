#include "fpp.h"
#include "fppd.h"
#include "E131.h"
#include "command.h"
#include "playList.h"
#include "mpg123.h"
#include "schedule.h"
#include "pixelnetDMX.h"
#include "e131bridge.h"

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>

FILE *logFile;
char logText[1024];

pid_t pid, sid;
int FPPstatus=FPP_STATUS_IDLE;

int main()
{
  CreateDaemon();
  //while(1)
  //{
  //  sprintf(logText,"Daemon Done Created");
  //  LogWrite(logText);
  //  sleep(1);
  //}
  MainProc();
  return 0;
}

void CreateDaemon(void)
{
  /* Fork off the parent process */
  pid = fork();
  if (pid < 0) {
          exit(EXIT_FAILURE);
  }
  /* If we got a good PID, then
      we can exit the parent process. */
  if (pid > 0) {
          exit(EXIT_SUCCESS);
  }

  /* Change the file mode mask */
  umask(0);

  /* Open any logs here */

  /* Create a new SID for the child process */
  sid = setsid();
  if (sid < 0) {
          /* Log any failures here */
          exit(EXIT_FAILURE);
  }

  /* Close out the standard file descriptors */
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}

void MainProc(void)
{
		struct sched_param param;
		param.sched_priority = 99;
		if (sched_setscheduler(0, SCHED_FIFO, & param) != 0) {
				perror("sched_setscheduler");
		exit(EXIT_FAILURE);  
		}

   logFile = fopen("fppdLog.txt", "w");
   logText[0] = '\0';
   fwrite(logText,1,1,logFile);
   fclose(logFile);

   sprintf(logText,"Falcon PI Player\n\r");
   LogWrite(logText);
  
	//Bridge_Initialize();
	//while(1)
	//{
	
	//}
	CheckExistanceOfDirectories();
  MusicInitialize();
  E131_Initialize();
  Command_Initialize();
	InitializePixelnetDMX();
  sprintf(logText,"Initialize E131 done\n");
  LogWrite(logText);
  while(1)
  {
    usleep(100000);
    switch(FPPstatus)
    {
      case FPP_STATUS_IDLE:
        Commandproc();
        ScheduleProc();
        break;
      case FPP_STATUS_PLAYLIST_PLAYING:
        PlayListPlayingLoop();
        break;
			case FPP_STATUS_STOPPING_GRACEFULLY:
        PlayListPlayingLoop();
        break;
      default:
        break;
    }
  }
}


void LogWrite(const char* text)
{
    //logFile = fopen("fppdLog.txt", "a");
		//fwrite(text,1,strlen(text),logFile);
    //fclose(logFile);
    //printf(text);

}




