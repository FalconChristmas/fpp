/*
 *   Falcon Pi Player Daemon 
 *   Falcon Pi Player project (FPP) 
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

#include "channeloutput.h"
#include "channeloutputthread.h"
#include "command.h"
#include "e131bridge.h"
#include "effects.h"
#include "fpp.h"
#include "fppd.h"
#include "log.h"
#include "mediaoutput.h"
#include "memorymap.h"
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
#include <sys/stat.h>

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

	InitializeChannelDataMemoryMap();

	if (getFPPmode() == PLAYER_MODE)
	{
		SendBlankingData();

		LogInfo(VB_GENERAL, "Starting Player Process\n");
		PlayerProcess();
	}
	else if (getFPPmode() == BRIDGE_MODE)
	{
		LogInfo(VB_GENERAL, "Starting Bridge Process\n");
		Bridge_Process();
	}
	else
	{
		LogErr(VB_GENERAL, "Invalid mode, quitting\n");
	}

	CloseChannelDataMemoryMap();

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

	// Check to see if we need to start up the output thread.
	// FIXME, possibly trigger this via a fpp command to fppd
	if (UsingMemoryMapInput() && !ChannelOutputThreadIsRunning())
		StartChannelOutputThread();
  }

  LogInfo(VB_GENERAL, "Main Player Process Loop complete, shutting down.\n");

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
