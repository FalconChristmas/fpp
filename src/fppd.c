#include "channeloutput/channeloutput.h"
#include "command.h"
#include "e131bridge.h"
#include "effects.h"
#include "fpp.h"
#include "fppd.h"
#include "log.h"
#include "mediaoutput/mediaoutput.h"
//#include "memorymap.h"
#include "playList.h"
#include "schedule.h"
#include "sequence.h"
#include "settings.h"

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
int runMainFPPDLoop = 1;

int main(int argc, char *argv[])
{
	initSettings();

	loadSettings("/home/pi/media/settings");

	// Parse our arguments first, override any defaults
	parseArguments(argc, argv);

	printSettings();

	// Start functioning
	if (getDaemonize())
	    CreateDaemon();

	CheckExistanceOfDirectoriesAndFiles();

	InitializeChannelOutputs();

	Command_Initialize();

	InitEffects();

//	InitializeSequenceDataMemoryMap();

	if (getFPPmode() == PLAYER_MODE)
	{
		SendBlankingData();

		LogInfo(VB_GENERIC, "Starting Player Process\n");
		PlayerProcess();
	}
	else if (getFPPmode() == BRIDGE_MODE)
	{
		LogInfo(VB_GENERIC, "Starting Bridge Process\n");
		Bridge_Process();
	}
	else
	{
		LogErr(VB_GENERIC, "Invalid mode, quitting\n");
	}

//	CloseSequenceDataMemoryMap();

	CloseEffects();

	CloseChannelOutputs();

	return 0;
}

void ShutdownFPPD(void)
{
	runMainFPPDLoop = 0;
}

void PlayerProcess(void)
{
	InitMediaOutput();
#ifndef NOROOT
	struct sched_param param;
	param.sched_priority = 99;
	if (sched_setscheduler(0, SCHED_FIFO, & param) != 0) 
	{
		perror("sched_setscheduler");
		exit(EXIT_FAILURE);  
	}
#endif

	CheckIfShouldBePlayingNow();
  while (runMainFPPDLoop)
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

  LogInfo(VB_GENERIC, "Main Player Process Loop complete, shutting down.\n");

  CleanupMediaOutput();
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
