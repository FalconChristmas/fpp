/*
 *   Setting manager for Falcon Pi Player (FPP)
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

#include "channeloutput/FPD.h"
#include "falcon.h"
#include "settings.h"
#include "fppd.h"
#include "fppversion.h"
#include "log.h"
#include "mediaoutput.h"

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include "common.h"


char *fpp_bool_to_string[] = { "false", "true", "default" };
static struct config settings = { 0 };

/* Prototypes for functions below */
int findSettingIndex(char *setting);

/*
 *
 */
void initSettings(int argc, char **argv)
{
	settings.binDirectory = strdup(dirname(argv[0]));

	settings.fppMode = PLAYER_MODE;

	char *tmp = getcwd(NULL, 0);
	if ( tmp == NULL )
		settings.fppDirectory = strdup("/opt/fpp");
	else
	{
		// trim off src/ or bin/
		char *offset = NULL;
		int size = strlen(tmp);

		if ((size > 4) && (!strcmp(&tmp[size - 4], "/src")))
			offset = &tmp[size - 4];
		else if ((size > 4) && (!strcmp(&tmp[size - 4], "/bin")))
			offset = &tmp[size - 4];
		else if ((size > 8) && (!strcmp(&tmp[size - 8], "/scripts")))
			offset = &tmp[size - 8];

		if (offset != NULL)
			*offset = 0;

		settings.fppDirectory = strdup(tmp);

		free(tmp);
	}

	char tmpDir[64];
	char mediaDir[64];
	if (DirectoryExists("/home/fpp"))
		strcpy(mediaDir, "/home/fpp");
	else
		strcpy(mediaDir, "/home/pi");

	strcat(mediaDir, "/media");
	settings.mediaDirectory = strdup(mediaDir);

	strcpy(tmpDir, mediaDir);
	settings.musicDirectory = strdup(strcat(tmpDir, "/music"));
	strcpy(tmpDir, mediaDir);
	settings.sequenceDirectory = strdup(strcat(tmpDir, "/sequences"));
	strcpy(tmpDir, mediaDir);
	settings.playlistDirectory = strdup(strcat(tmpDir, "/playlists"));
	strcpy(tmpDir, mediaDir);
	settings.eventDirectory = strdup(strcat(tmpDir, "/events"));
	strcpy(tmpDir, mediaDir);
	settings.videoDirectory = strdup(strcat(tmpDir, "/videos"));
	strcpy(tmpDir, mediaDir);
	settings.effectDirectory = strdup(strcat(tmpDir, "/effects"));
	strcpy(tmpDir, mediaDir);
	settings.scriptDirectory = strdup(strcat(tmpDir, "/scripts"));
	strcpy(tmpDir, mediaDir);
	settings.pluginDirectory = strdup(strcat(tmpDir, "/plugins"));
	strcpy(tmpDir, mediaDir);
	settings.universeFile = strdup(strcat(tmpDir, "/universes"));
	strcpy(tmpDir, mediaDir);
	settings.pixelnetFile = strdup(strcat(tmpDir, "/config/Falcon.FPDV1"));
	strcpy(tmpDir, mediaDir);
	settings.scheduleFile = strdup(strcat(tmpDir, "/schedule"));
	strcpy(tmpDir, mediaDir);
	settings.logFile = strdup(strcat(tmpDir, "/logs/fppd.log"));
	strcpy(tmpDir, mediaDir);
	settings.silenceMusic = strdup(strcat(tmpDir, "/silence.ogg"));
	strcpy(tmpDir, mediaDir);
	settings.bytesFile = strdup(strcat(tmpDir, "/bytesReceived"));
	strcpy(tmpDir, mediaDir);
	settings.settingsFile = strdup(strcat(tmpDir, "/settings"));
	settings.daemonize = 1;
	settings.E131interface = strdup("eth0");
	settings.controlMajor = 0;
	settings.controlMinor = 0;

	SetLogLevel("info");
	SetLogMask("most");

	// FIXME, include defaults from above in here
	bzero(settings.keys, sizeof(settings.keys));
	bzero(settings.values, sizeof(settings.values));
}

// Returns a string that's the white-space trimmed version
// of the input string.  Also trim double quotes now that the
// settings file will have double quotes in it.
char *trimwhitespace(const char *str)
{
	const char *end;
	size_t out_size;
	char *out;

	// Trim leading space
	while((isspace(*str)) || (*str == '"')) str++;

	if(*str == 0)  // All spaces?
		return strdup("");

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && ((isspace(*end)) || (*end == '"'))) end--;
	end++;

	// Set output size to minimum of trimmed string length
	out_size = end - str;

	// All whitespace
	if ( out_size == 0 )
		return strdup("");

	out = (char *)malloc(out_size+1);
	// If we didn't allocate anything, die!
	if ( ! out )
	{
		fprintf(stderr, "Failed to allocate memory for string trimming\n");
		exit(EXIT_FAILURE);
	}

	// Copy trimmed string and add null terminator
	memcpy(out, str, out_size);
	out[out_size] = 0;

	return out;
}

// Caller must free the char array we give them
char *modeToString(int mode)
{
	if ( mode == PLAYER_MODE )
		return strdup("fppMode");
	else if ( mode == BRIDGE_MODE )
		return strdup("bridge");
	else if ( mode == MASTER_MODE )
		return strdup("master");
	else if ( mode == REMOTE_MODE )
		return strdup("remote");
	
	return NULL;
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
#ifdef USEHTTPAPI
"                                    http        - HTTP API requests\n"
#endif
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
				settings.daemonize = false;
				break;
			case 'd': //daemonize
				settings.daemonize = true;
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

int parseSetting(char *key, char *value)
{
	if ( strcmp(key, "daemonize") == 0 )
	{
		if ( strcmp(value, "false") == 0 )
			settings.daemonize = false;
		else if ( strcmp(value, "true") == 0 )
			settings.daemonize = true;
		else
		{
			fprintf(stderr, "Failed to apply daemonize setting\n");
			exit(EXIT_FAILURE);
		}
	}
	else if ( strcmp(key, "fppMode") == 0 )
	{
		if ( strcmp(value, "player") == 0 )
			settings.fppMode = PLAYER_MODE;
		else if ( strcmp(value, "bridge") == 0 )
			settings.fppMode = BRIDGE_MODE;
		else if ( strcmp(value, "master") == 0 )
			settings.fppMode = MASTER_MODE;
		else if ( strcmp(value, "remote") == 0 )
			settings.fppMode = REMOTE_MODE;
		else
		{
			fprintf(stderr, "Error parsing mode\n");
			exit(EXIT_FAILURE);
		}
	}
	else if ( strcmp(key, "volume") == 0 )
	{
		if ( strlen(value) )
			setVolume(atoi(value));
		else
			fprintf(stderr, "Failed to apply volume setting\n");
	}
	else if ( strcmp(key, "alwaysTransmit") == 0 )
	{
		if ( strlen(value) )
			settings.alwaysTransmit = atoi(value);
		else
			fprintf(stderr, "Failed to apply alwaysTransmit setting\n");
	}
	else if ( strcmp(key, "mediaDirectory") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.mediaDirectory);
			settings.mediaDirectory = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply mediaDirectory\n");
	}
	else if ( strcmp(key, "musicDirectory") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.musicDirectory);
			settings.musicDirectory = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply musicDirectory\n");
	}
	else if ( strcmp(key, "sequenceDirectory") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.sequenceDirectory);
			settings.sequenceDirectory = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply sequenceDirectory\n");
	}
	else if ( strcmp(key, "eventDirectory") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.eventDirectory);
			settings.eventDirectory = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply eventDirectory\n");
	}
	else if ( strcmp(key, "videoDirectory") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.videoDirectory);
			settings.videoDirectory = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply videoDirectory\n");
	}
	else if ( strcmp(key, "effectDirectory") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.effectDirectory);
			settings.effectDirectory = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply effectDirectory\n");
	}
	else if ( strcmp(key, "scriptDirectory") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.scriptDirectory);
			settings.scriptDirectory = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply scriptDirectory\n");
	}
	else if ( strcmp(key, "pluginDirectory") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.pluginDirectory);
			settings.pluginDirectory = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply pluginDirectory\n");
	}
	else if ( strcmp(key, "playlistDirectory") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.playlistDirectory);
			settings.playlistDirectory = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply playlistDirectory\n");
	}
	else if ( strcmp(key, "universeFile") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.universeFile);
			settings.universeFile = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply universeFile\n");
	}
	else if ( strcmp(key, "pixelnetFile") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.pixelnetFile);
			settings.pixelnetFile = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply pixelnetFile\n");
	}
	else if ( strcmp(key, "scheduleFile") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.scheduleFile);
			settings.scheduleFile = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply scheduleFile\n");
	}
	else if ( strcmp(key, "LogLevel") == 0 )
	{
		if (strlen(value))
			SetLogLevel(value);
		else
			SetLogLevel("warn");
	}
	else if ( strcmp(key, "LogMask") == 0 )
	{
		if (strlen(value))
			SetLogMask(value);
		else
			SetLogMask("");
	}
	else if ( strcmp(key, "logFile") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.logFile);
			settings.logFile = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply logFile\n");
	}
	else if ( strcmp(key, "silenceMusic") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.silenceMusic);
			settings.silenceMusic = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply silenceMusic\n");
	}
	else if ( strcmp(key, "bytesFile") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.bytesFile);
			settings.bytesFile = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply bytesFile\n");
	}
	else if ( strcmp(key, "E131interface") == 0 )
	{
		if ( strlen(value) )
		{
			free(settings.E131interface);
			settings.E131interface = strdup(value);
		}
		else
			fprintf(stderr, "Failed to apply E131interface\n");
	}
	else if ( strcmp(key, "controlMajor") == 0 )
	{
		if ( strlen(value) )
		{
			int ivalue = atoi(value);
			if (ivalue >= 0)
				settings.controlMajor = (unsigned int)ivalue;
			else
				fprintf(stderr, "Error, controlMajor value negative\n");
		}
		else
			fprintf(stderr, "Failed to apply controlMajor setting\n");
	}
	else if ( strcmp(key, "controlMinor") == 0 )
	{
		if ( strlen(value) )
		{
			int ivalue = atoi(value);
			if (ivalue >= 0)
				settings.controlMinor = (unsigned int)ivalue;
			else
				fprintf(stderr, "Error, controlMinor value negative\n");
		}
		else
			fprintf(stderr, "Failed to apply controlMinor setting\n");
	}

	return 1;
}

int loadSettings(const char *filename)
{
	if (!FileExists(filename)) {
		LogWarn(VB_SETTING,
			"Attempted to load settings file %s which does not exist!", filename);
		return -1;
	}

	FILE *file = fopen(filename, "r");

	if (file != NULL)
	{
		char * line = NULL;
		size_t len = 0;
		ssize_t read;
		int count = 0;
		int sIndex = 0;

		while ((read = getline(&line, &len, file)) != -1)
		{
			if (( ! line ) || ( ! read ) || ( read == 1 ))
				continue;

			char *key = NULL, *value = NULL;	// These are values we're looking for and will
												// run through trimwhitespace which means they
												// must be freed before we are done.

			char *token = strtok(line, "=");
			if ( ! token )
				continue;

			key = trimwhitespace(token);
			if ( !strlen(key) )
			{
				free(key);
				continue;
			}

			token = strtok(NULL, "=");
			if ( !token )
			{
				fprintf(stderr, "Error tokenizing value for %s setting\n", key);
				free(key);
				continue;
			}
			value = trimwhitespace(token);

			parseSetting(key, value);

			sIndex = findSettingIndex(key);
			if (sIndex >= 0) {
				free(settings.values[sIndex]);
				settings.values[sIndex] = strdup(value);
			} else {
				settings.keys[count] = strdup(key);
				settings.values[count] = strdup(value);
				count++;
			}

			if ( key )
			{
				free(key);
				key = NULL;
			}

			if ( value )
			{
				free(value);
				value = NULL;
			}
		}

		if (line)
			free(line);
	
		fclose(file);
	}
	else
	{
		LogWarn(VB_SETTING, "Warning: couldn't open settings file: '%s'!\n", filename);
		return -1;
	}

	if (getDaemonize())
		SetLogFile(getLogFile());
	else
		SetLogFile("");

	return 0;
}

int findSettingIndex(char *setting)
{
	int count = 0;

	if (!setting) {
		return -1;
	}

	while (settings.keys[count]) {
		if (!strcmp(settings.keys[count], setting)) {
			return count;
		}
		count++;
	}

	return -1;
}

char *getSetting(char *setting)
{
	int count = 0;

	if (!setting) {
		LogErr(VB_SETTING, "getSetting() called with NULL value\n");
		return "";
	}

	while (settings.keys[count]) {
		if (!strcmp(settings.keys[count], setting)) {
			LogExcess(VB_SETTING, "getSetting(%s) found '%s'\n", setting, settings.values[count]);
			return settings.values[count];
		}
		count++;
	}

	LogExcess(VB_SETTING, "getSetting(%s) returned setting not found\n", setting);
	return "";
}

int getSettingInt(char *setting)
{
	char *valueStr = getSetting(setting);
	int   value = strtol(valueStr, NULL, 10);

	LogExcess(VB_SETTING, "getSettingInt(%s) returning %d\n", setting, value);

	return value;
}

int getDaemonize(void)
{
	return settings.daemonize;
}

#ifndef __GNUG__
inline
#endif
int getFPPmode(void)
{
	return settings.fppMode;
}

#ifndef __GNUG__
inline
#endif
char *getFPPmodeStr(void)
{
	if (settings.fppMode == BRIDGE_MODE)
		return "bridge";
	else if (settings.fppMode == PLAYER_MODE)
		return "player";
	else if (settings.fppMode == MASTER_MODE)
		return "master";
	else if (settings.fppMode == REMOTE_MODE)
		return "remote";

	return "UNKNOWN";
}

int getVolume(void)
{
	// Default of 0 is also a settable value, just return our data
	return settings.volume;
}

#ifndef __GNUG__
inline
#endif
int getAlwaysTransmit(void)
{
	return settings.alwaysTransmit;
}

char *getBinDirectory(void)
{
	return settings.binDirectory;
}

char *getFPPDirectory(void)
{
	return settings.fppDirectory;
}
char *getMediaDirectory(void)
{
	return settings.mediaDirectory;
}
char *getMusicDirectory(void)
{
	return settings.musicDirectory;
}
char *getSequenceDirectory(void)
{
	return settings.sequenceDirectory;
}
char *getEventDirectory(void)
{
	return settings.eventDirectory;
}
char *getVideoDirectory(void)
{
	return settings.videoDirectory;
}
char *getEffectDirectory(void)
{
	return settings.effectDirectory;
}
char *getScriptDirectory(void)
{
	return settings.scriptDirectory;
}
char *getPluginDirectory(void)
{
	return settings.pluginDirectory;
}
char *getPlaylistDirectory(void)
{
	return settings.playlistDirectory;
}
char *getUniverseFile(void)
{
	return settings.universeFile;
}
char *getPixelnetFile(void)
{
	return settings.pixelnetFile;
}
char *getScheduleFile(void)
{
	return settings.scheduleFile;
}
char *getLogFile(void)
{
	return settings.logFile;
}
char *getSilenceMusic(void)
{
	return settings.silenceMusic;
}
char *getBytesFile(void)
{
	return settings.bytesFile;
}

char *getSettingsFile(void)
{
	return settings.settingsFile;
}

char *getE131interface(void)
{
	return settings.E131interface;
}

unsigned int getControlMajor(void)
{
	return settings.controlMajor;
}

unsigned int getControlMinor(void)
{
	return settings.controlMinor;
}

void setVolume(int volume)
{
	char buffer [60];
	
	if ( volume < 0 )
		settings.volume = 0;
	else if ( volume > 100 )
		settings.volume = 100;
	else
		settings.volume = volume;

	char *mixerDevice = getSetting("AudioMixerDevice");
	int   audioOutput = getSettingInt("AudioOutput");

	// audioOutput is 0 on Pi where we need to apply volume adjustment formula.
	// This may break non-Pi, non-BBB platforms, but there aren't any yet.
	// The same assumption is made in fppxml.php SetVolume()
	if (audioOutput == 0)
	{
		if (mixerDevice)
			snprintf(buffer, 60, "amixer set %s %.2f%% >/dev/null 2>&1",
				 mixerDevice, (50 + (settings.volume / 2.0)));
		else
			snprintf(buffer, 60, "amixer set PCM %.2f%% >/dev/null 2>&1",
				 (50 + (settings.volume / 2.0)));
	}
	else
	{
		if (mixerDevice)
			snprintf(buffer, 60, "amixer set %s %d%% >/dev/null 2>&1",
				 mixerDevice, settings.volume);
		else
			snprintf(buffer, 60, "amixer set PCM %d%% >/dev/null 2>&1",
				 settings.volume);
	}

	LogDebug(VB_SETTING,"Volume change: %d \n", settings.volume);	
	system(buffer);

	pthread_mutex_lock(&mediaOutputLock);
	if (mediaOutput)
		mediaOutput->SetVolume(settings.volume);

	pthread_mutex_unlock(&mediaOutputLock);
}

/*
int saveSettingsFile(void)
{
	char buffer[1024]; //TODO: Fix this!!
	int bytes;

	FILE *fd = fopen(getSettingsFile(),"w");
	if ( ! fd )
	{
		fprintf(stderr, "Failed to create config file: %s\n", getSettingsFile());
		exit(EXIT_FAILURE);
	}

	bytes  = fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "daemonize", fpp_bool_to_string[getDaemonize()]);
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	if ( getFPPmode() == PLAYER_MODE )
		snprintf(buffer, 1024, "%s = %s\n", "fppMode", "player");
	else if ( getFPPmode() == BRIDGE_MODE )
		snprintf(buffer, 1024, "%s = %s\n", "fppMode", "bridge");
	else if ( getFPPmode() == MASTER_MODE )
		snprintf(buffer, 1024, "%s = %s\n", "fppMode", "master");
	else if ( getFPPmode() == REMOTE_MODE )
		snprintf(buffer, 1024, "%s = %s\n", "fppMode", "remote");
	else
		exit(EXIT_FAILURE);
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %d\n", "volume", getVolume());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "mediaDirectory", getMediaDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "musicDirectory", getMusicDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "sequenceDirectory", getSequenceDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "eventDirectory", getEventDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "videoDirectory", getVideoDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "effectDirectory", getEffectDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "scriptDirectory", getScriptDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "pluginDirectory", getPluginDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "playlistDirectory", getPlaylistDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "universeFile", getUniverseFile());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "pixelnetFile", getPixelnetFile());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "scheduleFile", getScheduleFile());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "logFile", getLogFile());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "silenceMusic", getSilenceMusic());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "mpg123Path", getMPG123Path());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "bytesFile", getBytesFile());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %d\n", "controlMajor", getControlMajor());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %d\n", "controlMinor", getControlMinor());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);

	fclose(fd);

	LogInfo(VB_SETTING, "Wrote config file of size %d\n", bytes);

	return 0;
}
*/

void CheckExistanceOfDirectoriesAndFiles(void)
{
	if(!DirectoryExists(getMediaDirectory()))
	{
		LogWarn(VB_SETTING, "FPP directory does not exist, creating it.\n");

		if ( mkdir(getMediaDirectory(), 0777) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create media directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getMusicDirectory()))
	{
		LogWarn(VB_SETTING, "Music directory does not exist, creating it.\n");

		if ( mkdir(getMusicDirectory(), 0777) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create music directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getSequenceDirectory()))
	{
		LogWarn(VB_SETTING, "Sequence directory does not exist, creating it.\n");

		if ( mkdir(getSequenceDirectory(), 0777) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create sequence directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getEventDirectory()))
	{
		LogWarn(VB_SETTING, "Event directory does not exist, creating it.\n");

		if ( mkdir(getEventDirectory(), 0777) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create event directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getVideoDirectory()))
	{
		LogWarn(VB_SETTING, "Video directory does not exist, creating it.\n");

		if ( mkdir(getVideoDirectory(), 0777) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create video directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getEffectDirectory()))
	{
		LogWarn(VB_SETTING, "Effect directory does not exist, creating it.\n");

		if ( mkdir(getEffectDirectory(), 0777) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create effect directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getScriptDirectory()))
	{
		LogWarn(VB_SETTING, "Script directory does not exist, creating it.\n");

		if ( mkdir(getScriptDirectory(), 0777) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create script directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getPlaylistDirectory()))
	{
		LogWarn(VB_SETTING, "Playlist directory does not exist, creating it.\n");

		if ( mkdir(getPlaylistDirectory(), 0777) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create playlist directory.\n");
			exit(EXIT_FAILURE);
		}
	}

	if(!FileExists(getUniverseFile()))
	{
		LogWarn(VB_SETTING, "Universe file does not exist, creating it.\n");

		char *cmd, *file = getUniverseFile();
		cmd = (char *)malloc(strlen(file)+7);
		snprintf(cmd, strlen(file)+7, "touch %s", file);
		if ( system(cmd) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create universe file.\n");
			exit(EXIT_FAILURE);
		}
		free(cmd);
	}
	if(!FileExists(getPixelnetFile()))
	{
		LogWarn(VB_SETTING, "Pixelnet file does not exist, creating it.\n");
		CreatePixelnetDMXfile(getPixelnetFile());
	}

	if(!FileExists(getScheduleFile()))
	{
		LogWarn(VB_SETTING, "Schedule file does not exist, creating it.\n");

		char *cmd, *file = getScheduleFile();
		cmd = (char *)malloc(strlen(file)+7);
		snprintf(cmd, strlen(file)+7, "touch %s", file);
		if ( system(cmd) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create schedule file.\n");
			exit(EXIT_FAILURE);
		}
		free(cmd);
	}
	if(!FileExists(getBytesFile()))
	{
		LogWarn(VB_SETTING, "Bytes file does not exist, creating it.\n");

		char *cmd, *file = getBytesFile();
		cmd = (char *)malloc(strlen(file)+7);
		snprintf(cmd, strlen(file)+7, "touch %s", file);
		if ( system(cmd) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create bytes file.\n");
			exit(EXIT_FAILURE);
		}
		free(cmd);
	}

	if(!FileExists(getSettingsFile()))
	{
		LogWarn(VB_SETTING, "Settings file does not exist, creating it.\n");

		char *cmd, *file = getSettingsFile();
		cmd = (char *)malloc(strlen(file)+7);
		snprintf(cmd, strlen(file)+7, "touch %s", file);
		if ( system(cmd) != 0 )
		{
			LogErr(VB_SETTING, "Error: Unable to create settings file.\n");
			exit(EXIT_FAILURE);
		}
		free(cmd);
	}
  


  
}
