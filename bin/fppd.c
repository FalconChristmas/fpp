#include "fpp.h"
#include "fppd.h"
#include "log.h"
#include "E131.h"
#include "command.h"
#include "playList.h"
#include "mpg123.h"
#include "schedule.h"
#include "pixelnetDMX.h"
#include "e131bridge.h"

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/types.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

pid_t pid, sid;
int FPPstatus=FPP_STATUS_IDLE;
int FPPmode=PLAYER_MODE;

//Settings 
char * SettingsFile = "/home/pi/media/settings";
char fppdVolume[4];

int main()
{
  CreateDaemon();
  
  CheckExistanceOfDirectoriesAndFiles();
	ReadFPPsettings(SettingsFile);
  E131_Initialize();
	InitializePixelnetDMX();
  Command_Initialize();
	if(FPPmode == PLAYER_MODE)
	{
		LogWrite("Starting Player Process\n");
		PlayerProcess();
	}
	else
	{
		LogWrite("Starting Bridge Process\n");
		Bridge_Process();
	}
  return 0;
}

void PlayerProcess(void)
{
	oggInit();
	struct sched_param param;
	param.sched_priority = 99;
	if (sched_setscheduler(0, SCHED_FIFO, & param) != 0) 
	{
		perror("sched_setscheduler");
		exit(EXIT_FAILURE);  
	}

  LogWrite("Initialize E131 done\n");
	CheckIfShouldBePlayingNow();
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

int ReadFPPsettings(char const * file)
{
  FILE *fp;
  int listIndex=0;
  char buf[128];
  char *s;
  LogWrite("Opening Settings Now %s\n",file);
  fp = fopen(file, "r");
  if (fp == NULL) 
  {
    LogWrite("Could not open settings file %s\n",file);
  	return 0;
  }
	// Parse Settings
	fgets(buf, 128, fp);
  s=strtok(buf,",");
  FPPmode = atoi(s);
  s = strtok(NULL,",");
	if(atoi(s) > 100)
	{
		strcpy(fppdVolume,"75");
	}
	else
	{
		strcpy(fppdVolume,s);
	}
	LogWrite("Mode=%d Volume=%s\n",FPPmode,fppdVolume);
  fclose(fp);
}

void CreateSettingsFile(char * file)
{
  FILE *fp;
	char * settings = "0,100";			// Mode, Volume
	char command[32];
  fp = fopen(file, "w");
	LogWrite("Creating file: %s\n",file);
	fwrite(settings, 1, 4, fp);
	fclose(fp);
	sprintf(command,"sudo chmod 777 %s",file);
	system(command);
}

void CheckExistanceOfDirectoriesAndFiles()
{
	if(!DirectoryExists("/home/pi/media"))
	{
		mkdir("/home/pi/media", 0777);
		LogWrite("Directory FPP Does Not Exist\n");
	}
	if(!DirectoryExists("/home/pi/media/music"))
	{
		mkdir("/home/pi/media/music", 0777);
		LogWrite("Directory Music Does Not Exist\n");
	}
	if(!DirectoryExists("/home/pi/media/sequences"))
	{
		mkdir("/home/pi/media/sequences", 0777);
		LogWrite("Directory sequences Does Not Exist\n");
	}
	if(!DirectoryExists("/home/pi/media/playlists"))
	{
		mkdir("/home/pi/media/playlists", 0777);
		LogWrite("Directory playlists Does Not Exist\n");
	}
	if(!FileExists("/home/pi/media/universes"))
	{
		system("touch /home/pi/media/universes");
	}
	if(!FileExists("/home/pi/media/pixelnetDMX"))
	{
		CreatePixelnetDMXfile("/home/pi/media/pixelnetDMX");
	}
	if(!FileExists("/home/pi/media/schedule"))
	{
		system("touch /home/pi/media/schedule");
	}
	if(!FileExists("/home/pi/media/bytesReceived"))
	{
		system("touch /home/pi/media/bytesReceived");
	}
	if(!FileExists(SettingsFile))
	{
		CreateSettingsFile(SettingsFile);
	}
}
