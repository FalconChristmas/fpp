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
#include "Playlist.h"
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

#ifdef USEWIRINGPI
#   include <wiringPi.h>
#   include <piFace.h>
#else
#   define wiringPiSetupSys()       0
#   define wiringPiSetupGpio()      0
#   define piFaceSetup(x)
#endif

pid_t pid, sid;
int FPPstatus=FPP_STATUS_IDLE;
int runMainFPPDLoop = 1;
extern PluginCallbackManager pluginCallbackManager;

ChannelTester *channelTester = NULL;

/* Prototypes for functions below */
void MainLoop(void);

int main(int argc, char *argv[])
{
	initSettings(argc, argv);
	initMediaDetails();

	if (DirectoryExists("/home/fpp"))
		loadSettings("/home/fpp/media/settings");
	else
		loadSettings("/home/pi/media/settings");

	wiringPiSetupGpio(); // would prefer wiringPiSetupSys();
	// NOTE: wiringPISetupSys() is not fast enough for SoftPWM on GPIO output

	// Parse our arguments first, override any defaults
	parseArguments(argc, argv);

	if (loggingToFile())
		logVersionInfo();

	printVersionInfo();

	// Start functioning
	if (getDaemonize())
		CreateDaemon();

	scheduler = new Scheduler();
	playlist  = new Playlist();
	sequence  = new Sequence();
	channelTester = new ChannelTester();

	piFaceSetup(200); // PiFace inputs 1-8 == wiringPi 200-207

	SetupGPIOInput();

	pluginCallbackManager.init();

	CheckExistanceOfDirectoriesAndFiles();

	if (getFPPmode() != BRIDGE_MODE)
	{
		InitMediaOutput();
	}

	if (getFPPmode() & PLAYER_MODE)
	{
		if (getFPPmode() == MASTER_MODE)
			InitSyncMaster();
	}

	InitializeChannelOutputs();
	sequence->SendBlankingData();

	InitEffects();
	InitializeChannelDataMemoryMap();

#ifndef NOROOT
	struct sched_param param;
	param.sched_priority = 99;
	if (sched_setscheduler(0, SCHED_FIFO, &param) != 0)
	{
		perror("sched_setscheduler");
		exit(EXIT_FAILURE);
	}
#endif

	MainLoop();

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
	delete scheduler;
	delete playlist;
	delete sequence;

	return 0;
}

void ShutdownFPPD(void)
{
	runMainFPPDLoop = 0;
}

void MainLoop(void)
{
	int            commandSock = 0;
	int            controlSock = 0;
	int            bridgeSock = 0;
	int            prevFPPstatus = FPPstatus;
	int            sleepms = 50000;
	fd_set         active_fd_set;
	fd_set         read_fd_set;
	struct timeval timeout;
	int            selectResult;

	LogDebug(VB_GENERAL, "MainLoop()\n");

	FD_ZERO (&active_fd_set);

	commandSock = Command_Initialize();
	if (commandSock)
		FD_SET (commandSock, &active_fd_set);

	if (getFPPmode() & PLAYER_MODE)
	{
		scheduler->CheckIfShouldBePlayingNow();
		if (getAlwaysTransmit())
			StartChannelOutputThread();
	}
	else if (getFPPmode() == BRIDGE_MODE)
	{
		bridgeSock = Bridge_Initialize();
		if (bridgeSock)
			FD_SET (bridgeSock, &active_fd_set);
	}

	controlSock = InitControlSocket();
	FD_SET (controlSock, &active_fd_set);

	LogInfo(VB_GENERAL, "Starting main processing loop\n");

	while (runMainFPPDLoop)
	{
		timeout.tv_sec  = 0;
		timeout.tv_usec = sleepms;

		read_fd_set = active_fd_set;


		selectResult = select(FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout);
		if (selectResult < 0)
		{
			if (errno == EINTR)
			{
				// We get interrupted when media players finish
				continue;
			}
			else
			{
				LogErr(VB_GENERAL, "Main select() failed: %s\n",
					strerror(errno));
				runMainFPPDLoop = 0;
				continue;
			}
		}

		if (commandSock && FD_ISSET(commandSock, &read_fd_set))
			CommandProc();

		if (bridgeSock && FD_ISSET(bridgeSock, &read_fd_set))
			Bridge_ReceiveData();

		if (controlSock && FD_ISSET(controlSock, &read_fd_set))
			ProcessControlPacket();

		// Check to see if we need to start up the output thread.
		// FIXME, possibly trigger this via a fpp command to fppd
		if ((!ChannelOutputThreadIsRunning()) &&
			(getFPPmode() != BRIDGE_MODE) &&
			((UsingMemoryMapInput()) ||
			 (channelTester->Testing()) ||
			 (getAlwaysTransmit()))) {
			int E131BridgingInterval = getSettingInt("E131BridgingInterval");
			if (!E131BridgingInterval)
				E131BridgingInterval = 50;
			SetChannelOutputRefreshRate(1000 / E131BridgingInterval);
			StartChannelOutputThread();
		}

		if (getFPPmode() & PLAYER_MODE)
		{
			if ((FPPstatus == FPP_STATUS_PLAYLIST_PLAYING) ||
				(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY))
			{
				if (prevFPPstatus == FPP_STATUS_IDLE)
				{
					playlist->PlayListPlayingInit();
					sleepms = 10000;
				}

				// Check again here in case PlayListPlayingInit
				// didn't find anything and put us back to IDLE
				if ((FPPstatus == FPP_STATUS_PLAYLIST_PLAYING) ||
					(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY))
				{
					playlist->PlayListPlayingProcess();
				}
			}

			int reactivated = 0;
			if (FPPstatus == FPP_STATUS_IDLE)
			{
				if ((prevFPPstatus == FPP_STATUS_PLAYLIST_PLAYING) ||
					(prevFPPstatus == FPP_STATUS_STOPPING_GRACEFULLY))
				{
					playlist->PlayListPlayingCleanup();

					if (FPPstatus != FPP_STATUS_IDLE)
						reactivated = 1;
					else
						sleepms = 50000;
				}
			}

			if (reactivated)
				prevFPPstatus = FPP_STATUS_IDLE;
			else
				prevFPPstatus = FPPstatus;

			scheduler->ScheduleProc();
		}
		else if (getFPPmode() == REMOTE_MODE)
		{
			if(mediaOutputStatus.status == MEDIAOUTPUTSTATUS_PLAYING)
			{
				playlist->PlaylistProcessMediaData();
			}
		}

		CheckGPIOInputs();
	}

	StopChannelOutputThread();
	ShutdownControlSocket();

	if (getFPPmode() == BRIDGE_MODE)
		Bridge_Shutdown();

	LogInfo(VB_GENERAL, "Main Loop complete, shutting down.\n");
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
