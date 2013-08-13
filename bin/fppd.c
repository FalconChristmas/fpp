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
#include <string.h>

FILE *logFile;
char logText[1024];

pid_t pid, sid;
int FPPstatus=FPP_STATUS_IDLE;

//Settings 
char * SettingsFile = "/home/pi/media/settings";
int FPPDmode=0;
extern char MPG123volume[4];

int main()
{
  //CreateDaemon();
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

	CheckExistanceOfDirectoriesAndFiles();
	ReadFPPsettings(SettingsFile);
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

int ReadFPPsettings(char const * file)
{
  FILE *fp;
  int listIndex=0;
  char buf[128];
  char *s;
  sprintf(logText,"Opening Settings Now %s\n",file);
  LogWrite(logText);
  fp = fopen(file, "r");
  if (fp == NULL) 
  {
    sprintf(logText,"Could not open settings file %s\n",file);
    LogWrite(logText);
  	return 0;
  }
	// Parse Settings
	fgets(buf, 128, fp);
  s=strtok(buf,",");
  FPPDmode = atoi(s);
  s = strtok(NULL,",");
	if(atoi(s) > 100)
	{
		strcpy(MPG123volume,"75");
	}
	else
	{
		strcpy(MPG123volume,s);
	}
	printf("Mode=%d Volume=%s\n",FPPDmode,MPG123volume);
  fclose(fp);
}

void CreateSettingsFile(char * file)
{
  FILE *fp;
	char * settings = "0,75";			// Mode, Volume
	char command[32];
  fp = fopen(file, "w");
	printf("Creating file: %s\n",file);
	fwrite(settings, 1, 4, fp);
	fclose(fp);
	sprintf(command,"sudo chmod 775 %s",file);
	system(command);
}

void CheckExistanceOfDirectoriesAndFiles()
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
		mkdir("/home/pi/media/playlists", 0755);
		sprintf(logText,"Directory playlists Does Not Exist\n");
		LogWrite(logText);
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
	if(!FileExists(SettingsFile))
	{
		CreateSettingsFile(SettingsFile);
	}
	

}



void LogWrite(const char* text)
{
    //logFile = fopen("fppdLog.txt", "a");
		//fwrite(text,1,strlen(text),logFile);
    //fclose(logFile);
    printf(text);

}




