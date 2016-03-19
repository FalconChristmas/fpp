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
#include "command.h"
#include "common.h"
#include "controlrecv.h"
#include "controlsend.h"
#include "e131bridge.h"
#include "effects.h"
#include "fppd.h"
#include "fppversion.h"
#include "fpp.h"
#include "gpio.h"
#include "log.h"
#include "mediadetails.h"
#include "mediaoutput.h"
#include "PixelOverlay.h"
#include "Player.h"
#include "playlist/NewPlaylist.h"
#include "Plugins.h"
#include "Scheduler.h"
#include "Sequence.h"
#include "settings.h"

#include <errno.h>
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

ChannelTester *channelTester = NULL;

/*
 *
 */
int main(int argc, char *argv[])
{
	initSettings(argc, argv);
	initMediaDetails();

	if (DirectoryExists("/home/fpp"))
		loadSettings("/home/fpp/media/settings");
	else
		loadSettings("/home/pi/media/settings");

	// Parse our arguments first, override any defaults
	parseArguments(argc, argv);

	if (loggingToFile())
		logVersionInfo();

	printVersionInfo();

	// Start functioning
	if (getDaemonize())
		CreateDaemon();

	player = new Player();
	channelTester = new ChannelTester();

#ifndef NOROOT
	struct sched_param param;
	param.sched_priority = 99;
	if (sched_setscheduler(0, SCHED_FIFO, &param) != 0)
	{
		perror("sched_setscheduler");
		exit(EXIT_FAILURE);
	}
#endif

	player->MainLoop();

	if (getFPPmode() != BRIDGE_MODE)
	{
		CleanupMediaOutput();
	}

	if (getFPPmode() & PLAYER_MODE)
	{
		if (getFPPmode() == MASTER_MODE)
			ShutdownSync();

		CloseChannelDataMemoryMap();
		CloseEffects();
	}

	CloseChannelOutputs();

	delete channelTester;
	delete player;

	player = NULL;

	return 0;
}

void ShutdownFPPD(void)
{
	player->Shutdown();
}

void CreateDaemon(void)
{
  /* Fork and terminate parent so we can run in the background */
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

  /* Fork a second time to get rid of session leader */
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

  /* Close out the standard file descriptors */
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);
}
