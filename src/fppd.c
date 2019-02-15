/*
 *   Falcon Player Daemon
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
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
#include "e131bridge.h"
#include "effects.h"
#include "fppd.h"
#include "fppversion.h"
#include "fpp.h"
#include "gpio.h"
#include "httpAPI.h"
#include "log.h"
#include "MultiSync.h"
#include "mediadetails.h"
#include "mediaoutput.h"
#include "mqtt.h"
#include "PixelOverlay.h"
#include "Playlist.h"
#include "playlist/Playlist.h"
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
#include <fcntl.h>
#include <signal.h>
#include <execinfo.h>

#include "channeloutput/FPD.h"
#include "falcon.h"
#include "mediaoutput.h"
#include "fppd.h"
#include <getopt.h>


#include <curl/curl.h>

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
volatile int runMainFPPDLoop = 1;
extern PluginCallbackManager pluginCallbackManager;

ChannelTester *channelTester = NULL;

/* Prototypes for functions below */
void MainLoop(void);


static void handleCrash(int s) {
    static volatile bool inCrashHandler = false;
    if (inCrashHandler) {
        //need to ignore any crashes in the crash handler
        return;
    }
    inCrashHandler = true;
    LogErr(VB_ALL, "Crash handler called:  %d\n", s);

    void* callstack[128];
    int i, frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);
    for (i = 0; i < frames; i++) {
        LogErr(VB_ALL, "  %s\n", strs[i]);
    }
    for (i = 0; i < frames; i++) {
        printf("  %s\n", strs[i]);
    }
    free(strs);
    inCrashHandler = false;
    if (s != SIGQUIT && s != SIGUSR1) {
        exit(-1);
    }
}
bool setupExceptionHandlers()
{
    // old sig handlers
    static bool s_savedHandlers = false;
    static struct sigaction s_handlerFPE,
    s_handlerILL,
    s_handlerBUS,
    s_handlerSEGV;
    
    bool ok = true;
    if ( !s_savedHandlers ) {
        // install the signal handler
        struct sigaction act;
        
        // some systems extend it with non std fields, so zero everything
        memset(&act, 0, sizeof(act));
        
        act.sa_handler = handleCrash;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        
        ok &= sigaction(SIGFPE, &act, &s_handlerFPE) == 0;
        ok &= sigaction(SIGILL, &act, &s_handlerILL) == 0;
        ok &= sigaction(SIGBUS, &act, &s_handlerBUS) == 0;
        ok &= sigaction(SIGSEGV, &act, &s_handlerSEGV) == 0;
        ok &= sigaction(SIGQUIT, &act, nullptr) == 0;
        ok &= sigaction(SIGUSR1, &act, nullptr) == 0;
        if (!ok) {
            LogWarn(VB_ALL, "Failed to install our signal handler.");
        }
        
        s_savedHandlers = true;
    } else if (s_savedHandlers) {
        // uninstall the signal handler
        ok &= sigaction(SIGFPE, &s_handlerFPE, NULL) == 0;
        ok &= sigaction(SIGILL, &s_handlerILL, NULL) == 0;
        ok &= sigaction(SIGBUS, &s_handlerBUS, NULL) == 0;
        ok &= sigaction(SIGSEGV, &s_handlerSEGV, NULL) == 0;
        if (!ok) {
            LogWarn(VB_ALL, "Failed to install default signal handlers.");
        }
        s_savedHandlers = false;
    }
    return ok;
}

inline void WriteRuntimeInfoFile(Json::Value v) {
    
    Json::Value systems = v["systems"];
    std::string addresses = "";
    for (int x = 0; x < systems.size(); x++) {
        if (addresses != "") {
            addresses += ",";
        }
        addresses += systems[x]["address"].asString();
    }
    Json::Value local = systems[0];
    local.removeMember("address");
    local["addresses"] = addresses;
    
    Json::FastWriter fastWriter;
    std::string resultStr = fastWriter.write(local);
    FILE *file = fopen("/home/fpp/media/fpp-info.json", "w");
    if (file)
    {
        fprintf(file, "%s\n", resultStr.c_str());
        fclose(file);
    }
}


void usage(char *appname)
{
printf("Usage: %s [OPTION...]\n"
"\n"
"fppd is the Falcon Player daemon.  It runs and handles playback of sequences,\n"
"audio, etc.  Normally it is kicked off by a startup task and daemonized,\n"
"however you can optionally kill the automatically started daemon and invoke it\n"
"manually via the command line or via the web interface.  Configuration is\n"
"supported for developers by specifying command line options, or editing a\n"
"config file that controls most settings.  For more information on that, read\n"
"the source code, it will not likely be documented any time soon.\n"
"\n"
"Options:\n"
"  -c, --config-file FILENAME    - Location of alternate configuration file\n"
"  -f, --foreground              - Don't daemonize the application.  In the\n"
"                                  foreground, all logging will be on the\n"
"                                  console instead of the log file\n"
"  -d, --daemonize               - Daemonize even if the config file says not to.\n"
"  -v, --volume VOLUME           - Set a volume (over-written by config file)\n"
"  -m, --mode MODE               - Set the mode: \"player\", \"bridge\",\n"
"                                  \"master\", or \"remote\"\n"
"  -B, --media-directory DIR     - Set the media directory\n"
"  -M, --music-directory DIR     - Set the music directory\n"
"  -S, --sequence-directory DIR  - Set the sequence directory\n"
"  -P, --playlist-directory DIR  - Set the playlist directory\n"
"  -u, --universe-file FILENAME  - Set the universe file\n"
"  -p, --pixelnet-file FILENAME  - Set the pixelnet file\n"
"  -s, --schedule-file FILENAME  - Set the schedule-file\n"
"  -l, --log-file FILENAME       - Set the log file\n"
"  -b, --bytes-file FILENAME     - Set the bytes received file\n"
"  -H  --detect-hardware         - Detect Falcon hardware on SPI port\n"
"  -C  --configure-hardware      - Configured detected Falcon hardware on SPI\n"
"  -h, --help                    - This menu.\n"
"      --log-level LEVEL         - Set the log output level:\n"
"                                  \"info\", \"warn\", \"debug\", \"excess\")\n"
"      --log-mask LIST           - Set the log output mask, where LIST is a\n"
"                                  comma-separated list made up of one or more\n"
"                                  of the following items:\n"
"                                    channeldata - channel data itself\n"
"                                    channelout  - channel output code\n"
"                                    command     - command processing\n"
"                                    control     - Control socket debugging\n"
"                                    e131bridge  - E1.31 bridge\n"
"                                    effect      - Effects sequences\n"
"                                    event       - Event handling\n"
"                                    general     - general messages\n"
"                                    gpio        - GPIO Input handling\n"
"                                    http        - HTTP API requests\n"
"                                    mediaout    - Media file handling\n"
"                                    playlist    - Playlist handling\n"
"                                    plugin      - Plugin handling\n"
"                                    schedule    - Playlist scheduling\n"
"                                    sequence    - Sequence parsing\n"
"                                    setting     - Settings parsing\n"
"                                    sync        - Master/Remote Synchronization\n"
"                                    all         - ALL log messages\n"
"                                    most        - Most excluding \"channeldata\"\n"
"                                  The default logging is:\n"
"                                    '--log-level info --log-mask most'\n"
	, appname);
}
extern struct config settings;

int parseArguments(int argc, char **argv)
{
	char *s = NULL;
	int c;
	while (1)
	{
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] =
		{
			{"displayvers",			no_argument,		0, 'V'},
			{"config-file",			required_argument,	0, 'c'},
			{"foreground",			no_argument,		0, 'f'},
			{"daemonize",			no_argument,		0, 'd'},
			{"volume",				required_argument,	0, 'v'},
			{"mode",				required_argument,	0, 'm'},
			{"media-directory",		required_argument,	0, 'B'},
			{"music-directory",		required_argument,	0, 'M'},
			{"sequence-directory",	required_argument,	0, 'S'},
			{"playlist-directory",	required_argument,	0, 'P'},
			{"event-directory",		required_argument,	0, 'E'},
			{"video-directory",		required_argument,	0, 'F'},
			{"universe-file",		required_argument,	0, 'u'},
			{"pixelnet-file",		required_argument,	0, 'p'},
			{"schedule-file",		required_argument,	0, 's'},
			{"log-file",			required_argument,	0, 'l'},
			{"bytes-file",			required_argument,	0, 'b'},
			{"detect-hardware",		no_argument,		0, 'H'},
			{"configure-hardware",		no_argument,		0, 'C'},
			{"help",				no_argument,		0, 'h'},
			{"silence-music",		required_argument,	0,	1 },
			{"log-level",			required_argument,	0,  2 },
			{"log-mask",			required_argument,	0,  3 },
			{0,						0,					0,	0}
		};

		c = getopt_long(argc, argv, "c:fdVv:m:B:M:S:P:u:p:s:l:b:HChV",
		long_options, &option_index);
		if (c == -1)
			break;

		switch (c)
		{
			case 'V':
				printVersionInfo();
				exit(0);
			case 1: //silence-music
				free(settings.silenceMusic);
				settings.silenceMusic = strdup(optarg);
				break;
			case 2: // log-level
				if (SetLogLevel(optarg)) {
					LogInfo(VB_SETTING, "Log Level set to %d (%s)\n", logLevel, optarg);
				}
				break;
			case 3: // log-mask
				if (SetLogMask(optarg)) {
					LogInfo(VB_SETTING, "Log Mask set to %d (%s)\n", logMask, optarg);
				}
				break;
			case 'c': //config-file
				if (FileExists(optarg))
				{
					if (loadSettings(optarg) != 0 )
					{
						LogErr(VB_SETTING, "Failed to load settings file given as argument: '%s'\n", optarg);
					}
					else
					{
						free(settings.settingsFile);
						settings.settingsFile = strdup(optarg);
					}
				} else {
					fprintf(stderr, "Settings file specified does not exist: '%s'\n", optarg);
				}
				break;
			case 'f': //foreground
				settings.daemonize = 0;
				break;
			case 'd': //daemonize
				settings.daemonize = 1;
				break;
			case 'v': //volume
				setVolume (atoi(optarg));
				break;
			case 'm': //mode
				if ( strcmp(optarg, "player") == 0 )
					settings.fppMode = PLAYER_MODE;
				else if ( strcmp(optarg, "bridge") == 0 )
					settings.fppMode = BRIDGE_MODE;
				else if ( strcmp(optarg, "master") == 0 )
					settings.fppMode = MASTER_MODE;
				else if ( strcmp(optarg, "remote") == 0 )
					settings.fppMode = REMOTE_MODE;
				else
				{
					fprintf(stderr, "Error parsing mode\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 'B': //media-directory
				free(settings.mediaDirectory);
				settings.mediaDirectory = strdup(optarg);
				break;
			case 'M': //music-directory
				free(settings.musicDirectory);
				settings.musicDirectory = strdup(optarg);
				break;
			case 'S': //sequence-directory
				free(settings.sequenceDirectory);
				settings.sequenceDirectory = strdup(optarg);
				break;
			case 'E': //event-directory
				free(settings.eventDirectory);
				settings.eventDirectory = strdup(optarg);
				break;
			case 'F': //video-directory
				free(settings.videoDirectory);
				settings.videoDirectory = strdup(optarg);
				break;
			case 'P': //playlist-directory
				free(settings.playlistDirectory);
				settings.playlistDirectory = strdup(optarg);
				break;
			case 'u': //universe-file
				free(settings.universeFile);
				settings.universeFile = strdup(optarg);
				break;
			case 'p': //pixelnet-file
				free(settings.pixelnetFile);
				settings.pixelnetFile = strdup(optarg);
				break;
			case 's': //schedule-file
				free(settings.scheduleFile);
				settings.scheduleFile = strdup(optarg);
				break;
			case 'l': //log-file
				free(settings.logFile);
				settings.logFile = strdup(optarg);
				break;
			case 'b': //bytes-file
				free(settings.bytesFile);
				settings.bytesFile = strdup(optarg);
				break;
			case 'H': //Detect Falcon hardware
			case 'C': //Configure Falcon hardware
				SetLogFile("");
				SetLogLevel("debug");
				SetLogMask("setting");
				if (DetectFalconHardware((c == 'C') ? 1 : 0))
					exit(1);
				else
					exit(0);
				break;
			case 'h': //help
				usage(argv[0]);
				exit(EXIT_SUCCESS);
				break;
			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	if (getDaemonize())
		SetLogFile(getLogFile());
	else
		SetLogFile("");

	return 0;
}

int main(int argc, char *argv[])
{
    setupExceptionHandlers();
	initSettings(argc, argv);
	initMediaDetails();

	if (DirectoryExists("/home/fpp"))
		loadSettings("/home/fpp/media/settings");
	else
		loadSettings("/home/pi/media/settings");

	curl_global_init(CURL_GLOBAL_ALL);

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

	if (strcmp(getSetting("MQTTHost"),""))
	{
		mqtt = new MosquittoClient(getSetting("MQTTHost"), getSettingInt("MQTTPort"), getSetting("MQTTPrefix"));

		if (!mqtt || !mqtt->Init(getSetting("MQTTUsername"), getSetting("MQTTPassword")))
			exit(EXIT_FAILURE);

		mqtt->Publish("version", getFPPVersion());
		mqtt->Publish("branch", getFPPBranch());
	}

	scheduler = new Scheduler();
	playlist = new Playlist();
	sequence  = new Sequence();
	channelTester = new ChannelTester();
	multiSync = new MultiSync();

	if (!multiSync->Init())
		exit(EXIT_FAILURE);

    int fd = -1;
    if ((fd = open ("/dev/spidev0.0", O_RDWR)) < 0) {
        LogWarn(VB_GENERAL, "Could not open SPI device.  Skipping piFace setup.\n");
    } else {
        close(fd);
        piFaceSetup(200); // PiFace inputs 1-8 == wiringPi 200-207
    }

	SetupGPIOInput();

	pluginCallbackManager.init();

	CheckExistanceOfDirectoriesAndFiles();
    if(!FileExists(getPixelnetFile())) {
        LogWarn(VB_SETTING, "Pixelnet file does not exist, creating it.\n");
        CreatePixelnetDMXfile(getPixelnetFile());
    }


	if (getFPPmode() != BRIDGE_MODE)
	{
		InitMediaOutput();
	}

	InitializeChannelOutputs();
	sequence->SendBlankingData();

	InitEffects();
	InitializeChannelDataMemoryMap();
    
    WriteRuntimeInfoFile(multiSync->GetSystems(true, false));

	MainLoop();

	if (getFPPmode() != BRIDGE_MODE)
	{
		CleanupMediaOutput();
	}

	if (getFPPmode() & PLAYER_MODE)
	{
		CloseChannelDataMemoryMap();
		CloseEffects();
	}

	CloseChannelOutputs();

	delete multiSync;
	delete channelTester;
	delete scheduler;
	delete playlist;
	delete sequence;
    runMainFPPDLoop = -1;

	if (mqtt)
		delete mqtt;

	curl_global_cleanup();

	return 0;
}

void ShutdownFPPD(void)
{
    LogInfo(VB_GENERAL, "Shutting down main loop.\n");
	runMainFPPDLoop = 0;
}

void MainLoop(void)
{
	int            commandSock = 0;
	int            controlSock = 0;
	int            bridgeSock = 0;
    int            ddpSock = 0;
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
		Bridge_Initialize(bridgeSock, ddpSock);
		if (bridgeSock)
			FD_SET (bridgeSock, &active_fd_set);
                if (ddpSock)
                    FD_SET (ddpSock, &active_fd_set);
	}

	controlSock = multiSync->GetControlSocket();
	FD_SET (controlSock, &active_fd_set);

	APIServer apiServer;
	apiServer.Init();

	multiSync->Discover();

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

        bool pushBridgeData = false;
		if (commandSock && FD_ISSET(commandSock, &read_fd_set))
			CommandProc();

		if (bridgeSock && FD_ISSET(bridgeSock, &read_fd_set))
 			pushBridgeData |= Bridge_ReceiveE131Data();
        if (ddpSock && FD_ISSET(ddpSock, &read_fd_set))
            pushBridgeData |= Bridge_ReceiveDDPData();

		if (FD_ISSET(controlSock, &read_fd_set))
			multiSync->ProcessControlPacket();

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
					playlist->Start();
					sleepms = 10000;
				}

				// Check again here in case PlayListPlayingInit
				// didn't find anything and put us back to IDLE
				if ((FPPstatus == FPP_STATUS_PLAYLIST_PLAYING) ||
					(FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY))
				{
					playlist->Process();
				}
			}

			int reactivated = 0;
			if (FPPstatus == FPP_STATUS_IDLE)
			{
				if ((prevFPPstatus == FPP_STATUS_PLAYLIST_PLAYING) ||
					(prevFPPstatus == FPP_STATUS_STOPPING_GRACEFULLY))
				{
					playlist->Cleanup();

					scheduler->ReLoadCurrentScheduleInfo();

					if (!playlist->GetForceStop())
						scheduler->CheckIfShouldBePlayingNow();

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
				playlist->ProcessMedia();
			}
        }
        else if (getFPPmode() == BRIDGE_MODE && pushBridgeData)
        {
            ForceChannelOutputNow();
        }

		CheckGPIOInputs();
	}

    LogInfo(VB_GENERAL, "Stopping channel output thread.\n");
	StopChannelOutputThread();

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
