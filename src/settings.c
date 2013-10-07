#include "settings.h"
#include "fppd.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>


char *fpp_bool_to_string[] = { "false", "true", "default" };
static struct config settings = { 0 };

void initSettings(void)
{
	settings.mediaDirectory = strdup("/home/pi/media");
	settings.musicDirectory = strdup("/home/pi/media/music");
	settings.sequenceDirectory = strdup("/home/pi/media/sequences");
	settings.playlistDirectory = strdup("/home/pi/media/playlists");
	settings.eventDirectory = strdup("/home/pi/media/events");
	settings.universeFile = strdup("/home/pi/media/universes");
	settings.pixelnetFile = strdup("/home/pi/media/pixelnetDMX");
	settings.scheduleFile = strdup("/home/pi/media/schedule");
	settings.logFile = strdup("/home/pi/media/fppdLog.txt");
	settings.silenceMusic = strdup("/home/pi/media/silence.ogg");
	settings.bytesFile = strdup("/home/pi/media/bytesReceived");
  settings.daemonize = 1;
}

// Returns a string that's the white-space trimmed version
// of the input string
char *trimwhitespace(const char *str)
{
	const char *end;
	size_t out_size;
	char *out;

	// Trim leading space
	while(isspace(*str)) str++;

	if(*str == 0)  // All spaces?
		return strdup("");

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && (isspace(*end))) end--;
	end++;

	// Set output size to minimum of trimmed string length
	out_size = end - str;

	// All whitespace
	if ( out_size == 0 )
		return strdup("");

	out = malloc(out_size+1);
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

void printSettings(void)
{
	FILE *fd = stdout; // change to stderr to log there instead

	fprintf(fd, "verbose: %s\n",
		settings.verbose ? "true" : "false");
	fprintf(fd, "daemonize: %s\n",
		settings.daemonize ? "true" : "false");
	
	if ( settings.fppMode == PLAYER_MODE )
		fprintf(fd, "fppMode: %s\n", "player");
	else if ( settings.fppMode == BRIDGE_MODE )
		fprintf(fd, "fppMode: %s\n", "bridge");

	fprintf(fd, "volume: %u\n", settings.volume);

	if ( settings.mediaDirectory )
		fprintf(fd, "mediaDirectory(%u): %s\n",
				strlen(settings.mediaDirectory),
				settings.mediaDirectory);
	if ( settings.musicDirectory )
		fprintf(fd, "musicDirectory(%u): %s\n",
				strlen(settings.musicDirectory),
				settings.musicDirectory);
	if ( settings.sequenceDirectory )
		fprintf(fd, "sequenceDirectory(%u): %s\n",
				strlen(settings.sequenceDirectory),
				settings.sequenceDirectory);
	if ( settings.eventDirectory )
		fprintf(fd, "eventDirectory(%u): %s\n",
				strlen(settings.eventDirectory),
				settings.eventDirectory);
	if ( settings.playlistDirectory )
		fprintf(fd, "playlistDirectory(%u): %s\n",
				strlen(settings.playlistDirectory),
				settings.playlistDirectory);
	if ( settings.universeFile )
		fprintf(fd, "universeFile(%u): %s\n",
				strlen(settings.universeFile),
				settings.universeFile);
	if ( settings.pixelnetFile )
		fprintf(fd, "pixelnetFile(%u): %s\n",
				strlen(settings.pixelnetFile),
				settings.pixelnetFile);
	if ( settings.scheduleFile )
		fprintf(fd, "scheduleFile(%u): %s\n",
				strlen(settings.scheduleFile),
				settings.scheduleFile);
	if ( settings.logFile )
		fprintf(fd, "logFile(%u): %s\n",
				strlen(settings.logFile),
				settings.logFile);
	if ( settings.silenceMusic )
		fprintf(fd, "silenceMusic(%u): %s\n",
				strlen(settings.silenceMusic),
				settings.silenceMusic);
	if ( settings.bytesFile )
		fprintf(fd, "bytesFile(%u): %s\n",
				strlen(settings.bytesFile),
				settings.bytesFile);
	if ( settings.controlMajor != 0 )
		fprintf(fd, "controlMajor: %u\n", settings.controlMajor);
	if ( settings.controlMinor != 0 )
		fprintf(fd, "controlMinor: %u\n", settings.controlMinor);
}

void usage(char *appname)
{
printf("Usage: %s [OPTION...]\n"
"\n"
"fppd is the FalconPiPlayer daemon.  It runs and handles playback of sequences,\n"
"audio, etc.  Normally it is kicked off by a startup task and daemonized,\n"
"however you can optionally kill the automatically started daemon and invoke it\n"
"manually via the command line or via the web interface.  Configuration is\n"
"supported for developers by specifying command line options, or editing a\n"
"config file that controls most settings.  For more information on that, read\n"
"the source code, it will not likely be documented any time soon.\n"
"\n"
"Options:\n"
"\t-c, --config-file\tConfiguration file for things like config file paths\n"
"\t-f, --foreground\tDon't daemonize the application.  In the foreground, all\n"
"\t\t\t\tlogging will be on the console instead of the log file\n"
"\t-d, --daemonize\tDaemonize even if the config file says not to.\n"
"\t-V, --verbose\t\tEnable verbose logging.\n"
"\t-v, --volume\t\tSet a volume (over-written by config file)\n"
"\t-m, --mode\t\tSet the mode: (\"player\" or \"bridge\")\n"
"\t-B, --media-directory\tSet the media directory\n"
"\t-M, --music-directory\tSet the music directory\n"
"\t-S, --sequence-directory\tSet the sequence directory\n"
"\t-P, --playlist-directory\tSet the playlist directory\n"
"\t-u, --universe-file\tSet the universe file\n"
"\t-p, --pixelnet-file\tSet the pixelnet file\n"
"\t-s, --schedule-file\tSet the schedule-file\n"
"\t-l, --log-file\t\tSet the log file\n"
"\t-b, --bytes-file\tSet the bytes received file\n"
"\t-h, --help\t\tThis menu.\n"
"\t    --mpg123-path\tSet location of mpg123 executable\n"
"\t    --silence-music\tSet location of silence.ogg file\n", appname);
}

int parseArguments(int argc, char **argv)
{
	int c;
	while (1)
	{
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] =
		{
			{"config-file",			required_argument,	0, 'c'},
			{"foreground",			no_argument,		0, 'f'},
			{"daemonize",			no_argument,		0, 'd'},
			{"verbose",				no_argument,		0, 'V'},
			{"volume",				required_argument,	0, 'v'},
			{"mode",				required_argument,	0, 'm'},
			{"media-directory",		required_argument,	0, 'B'},
			{"music-directory",		required_argument,	0, 'M'},
			{"sequence-directory",	required_argument,	0, 'S'},
			{"playlist-directory",	required_argument,	0, 'P'},
			{"event-directory",		required_argument,	0, 'E'},
			{"universe-file",		required_argument,	0, 'u'},
			{"pixelnet-file",		required_argument,	0, 'p'},
			{"schedule-file",		required_argument,	0, 's'},
			{"log-file",			required_argument,	0, 'l'},
			{"silence-music",		required_argument,	0,	1 },
			{"bytes-file",			required_argument,	0, 'b'},
			{"help",				no_argument,		0, 'h'},
			{0,						0,					0,	0}
		};

		c = getopt_long(argc, argv, "c:fdVv:m:B:M:S:P:u:p:s:l:b:h",
		long_options, &option_index);
		if (c == -1)
			break;

		switch (c)
		{
			case 1: //silence-music
				free(settings.silenceMusic);
				settings.silenceMusic = strdup(optarg);
				break;
			case 'c': //config-file
				if ( loadSettings(optarg) != 0 )
					LogWrite("Failed to load settings file given as argument: '%s'\n", optarg);
				break;
			case 'f': //foreground
				settings.daemonize = false;
				break;
			case 'd': //daemonize
				settings.daemonize = true;
				break;
			case 'V': //verbose
				settings.verbose = true;
				break;
			case 'v': //volume
				settings.volume = atoi(optarg);
				break;
			case 'm': //mode
				if ( strcmp(optarg, "player") == 0 )
					settings.fppMode = PLAYER_MODE;
				else if ( strcmp(optarg, "bridge") == 0 )
					settings.fppMode = BRIDGE_MODE;
				else
				{
					printf("Error parsing mode\n");
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
				settings.eventDirectory = strdup(optarg);
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
			case 'h': //help
				usage(argv[0]);
				exit(EXIT_SUCCESS);
				break;
			default:
				usage(argv[0]);
				exit(EXIT_FAILURE);
		}
	}

	return 0;
}

int loadSettings(const char *filename)
{
	FILE *file = fopen(filename, "r");

	if (file != NULL)
	{
		char * line = NULL;
		size_t len = 0;
		ssize_t read;

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

			if ( strcmp(key, "verbose") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for verbose setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strcmp(value, "false") == 0 )
					settings.verbose = false;
				else if ( strcmp(value, "true") == 0 )
					settings.verbose = true;
				else
				{
					fprintf(stderr, "Failed to load verbose setting from config file\n");
					exit(EXIT_FAILURE);
				}
			}
			else if ( strcmp(key, "daemonize") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for daemonize setting\n");
					continue;
				}
				value = trimwhitespace(token);

				if ( strcmp(value, "false") == 0 )
					settings.daemonize = false;
				else if ( strcmp(value, "true") == 0 )
					settings.daemonize = true;
				else
				{
					fprintf(stderr, "Failed to load daemonize setting from config file\n");
					exit(EXIT_FAILURE);
				}
			}
			else if ( strcmp(key, "fppMode") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for fppMode setting\n");
					continue;
				}
				value = trimwhitespace(token);

				if ( strcmp(value, "player") == 0 )
					settings.fppMode = PLAYER_MODE;
				else if ( strcmp(value, "bridge") == 0 )
					settings.fppMode = BRIDGE_MODE;
				else
				{
					printf("Error parsing mode\n");
					exit(EXIT_FAILURE);
				}
			}
			else if ( strcmp(key, "volume") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for volume setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
					settings.volume = atoi(value);
				else
					fprintf(stderr, "Failed to load volume setting from config file\n");
			}
			else if ( strcmp(key, "mediaDirectory") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for mediaDirectory setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.mediaDirectory);
					settings.mediaDirectory = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load mediaDirectory from config file\n");
			}
			else if ( strcmp(key, "musicDirectory") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for musicDirectory setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.musicDirectory);
					settings.musicDirectory = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load musicDirectory from config file\n");
			}
			else if ( strcmp(key, "sequenceDirectory") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for sequenceDirectory setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.sequenceDirectory);
					settings.sequenceDirectory = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load sequenceDirectory from config file\n");
			}
			else if ( strcmp(key, "eventDirectory") == 0 )
			{
				if ( ! settings.eventDirectory )
				{
					token = strtok(NULL, "=");
					if ( ! token )
					{
						fprintf(stderr, "Error tokenizing value for eventDirectory setting\n");
						continue;
					}
					value = trimwhitespace(token);
					if ( strlen(value) )
						settings.eventDirectory = strdup(token);
					else
						fprintf(stderr, "Failed to load eventDirectory from config file\n");
				}
			}
			else if ( strcmp(key, "playlistDirectory") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for playlistDirectory setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.playlistDirectory);
					settings.playlistDirectory = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load playlistDirectory from config file\n");
			}
			else if ( strcmp(key, "universeFile") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for universeFile setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.universeFile);
					settings.universeFile = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load universeFile from config file\n");
			}
			else if ( strcmp(key, "pixelnetFile") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for pixelnetFile setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.pixelnetFile);
					settings.pixelnetFile = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load pixelnetFile from config file\n");
			}
			else if ( strcmp(key, "scheduleFile") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for scheduleFile setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.scheduleFile);
					settings.scheduleFile = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load scheduleFile from config file\n");
			}
			else if ( strcmp(key, "logFile") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for logFile setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.logFile);
					settings.logFile = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load logFile from config file\n");
			}
			else if ( strcmp(key, "silenceMusic") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for silenceMusic setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.silenceMusic);
					settings.silenceMusic = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load silenceMusic from config file\n");
			}
			else if ( strcmp(key, "bytesFile") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for bytesFile setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					free(settings.bytesFile);
					settings.bytesFile = strdup(value);
				}
				else
					fprintf(stderr, "Failed to load bytesFile from config file\n");
			}
			else if ( strcmp(key, "controlMajor") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for controlMajor setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					int ivalue = atoi(value);
					if (ivalue >= 0)
						settings.controlMajor = (unsigned int)ivalue;
					else
						fprintf(stderr, "Error, controlMajor value negative in config file\n");
				}
				else
					fprintf(stderr, "Failed to load controlMajor setting from config file\n");
			}
			else if ( strcmp(key, "controlMinor") == 0 )
			{
				token = strtok(NULL, "=");
				if ( ! token )
				{
					fprintf(stderr, "Error tokenizing value for controlMinor setting\n");
					continue;
				}
				value = trimwhitespace(token);
				if ( strlen(value) )
				{
					int ivalue = atoi(value);
					if (ivalue >= 0)
						settings.controlMinor = (unsigned int)ivalue;
					else
						fprintf(stderr, "Error, controlMinor value negative in config file\n");
				}
				else
					fprintf(stderr, "Failed to load controlMinor setting from config file\n");
			}
			else
			{
				fprintf(stderr, "Warning: unknown key: '%s', skipping\n", key);
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

			if ( line )
			{
				free(line);
				line = NULL;
			}
		}
	
		fclose(file);
	}
	else
	{
		LogWrite("Warning: couldn't open settings file: '%s'!\n", filename);
		return -1;
	}

	return 0;
}

int getVerbose(void)
{
	return settings.verbose;
}

int getDaemonize(void)
{
	return settings.daemonize;
}

int getFPPmode(void)
{
	return settings.fppMode;
}

int getVolume(void)
{
	// Default of 0 is also a settable value, just return our data
	return settings.volume;
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
	if ( volume < 0 )
		settings.volume = 0;
	else if ( volume > 100 )
		settings.volume = 100;
	else
		settings.volume = volume;
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

	snprintf(buffer, 1024, "%s = %s\n", "verbose", fpp_bool_to_string[getVerbose()]);
	bytes  = fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "daemonize", fpp_bool_to_string[getDaemonize()]);
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	if ( getFPPmode() == PLAYER_MODE )
		snprintf(buffer, 1024, "%s = %s\n", "fppMode", "player");
	else if ( getFPPmode() == BRIDGE_MODE )
		snprintf(buffer, 1024, "%s = %s\n", "fppMode", "bridge");
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

	LogWrite("Wrote config file of size %d\n", bytes);

	return 0;
}
*/

void CheckExistanceOfDirectoriesAndFiles(void)
{
	if(!DirectoryExists(getMediaDirectory()))
	{
		LogWrite("FPP directory does not exist, creating it.\n");

		if ( mkdir(getMediaDirectory(), 0777) != 0 )
		{
			LogWrite("Error: Unable to create media directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getMusicDirectory()))
	{
		LogWrite("Music directory does not exist, creating it.\n");

		if ( mkdir(getMusicDirectory(), 0777) != 0 )
		{
			LogWrite("Error: Unable to create music directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getSequenceDirectory()))
	{
		LogWrite("Sequence directory does not exist, creating it.\n");

		if ( mkdir(getSequenceDirectory(), 0777) != 0 )
		{
			LogWrite("Error: Unable to create sequence directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getEventDirectory()))
	{
		LogWrite("Event directory does not exist, creating it.\n");

		if ( mkdir(getEventDirectory(), 0777) != 0 )
		{
			LogWrite("Error: Unable to create event directory.\n");
			exit(EXIT_FAILURE);
		}
	}
	if(!DirectoryExists(getPlaylistDirectory()))
	{
		LogWrite("Playlist directory does not exist, creating it.\n");

		if ( mkdir(getPlaylistDirectory(), 0777) != 0 )
		{
			LogWrite("Error: Unable to create playlist directory.\n");
			exit(EXIT_FAILURE);
		}
	}

	if(!FileExists(getUniverseFile()))
	{
		LogWrite("Universe file does not exist, creating it.\n");

		char *cmd, *file = getUniverseFile();
		cmd = malloc(strlen(file)+7);
		snprintf(cmd, strlen(file)+7, "touch %s", file);
		if ( system(cmd) != 0 )
		{
			LogWrite("Error: Unable to create universe file.\n");
			exit(EXIT_FAILURE);
		}
		free(cmd);
	}
	if(!FileExists(getPixelnetFile()))
	{
		LogWrite("Pixelnet file does not exist, creating it.\n");

		CreatePixelnetDMXfile(getPixelnetFile());
	}
	if(!FileExists(getScheduleFile()))
	{
		LogWrite("Schedule file does not exist, creating it.\n");

		char *cmd, *file = getScheduleFile();
		cmd = malloc(strlen(file)+7);
		snprintf(cmd, strlen(file)+7, "touch %s", file);
		if ( system(cmd) != 0 )
		{
			LogWrite("Error: Unable to create schedule file.\n");
			exit(EXIT_FAILURE);
		}
		free(cmd);
	}
	if(!FileExists(getBytesFile()))
	{
		LogWrite("Bytes file does not exist, creating it.\n");

		char *cmd, *file = getBytesFile();
		cmd = malloc(strlen(file)+7);
		snprintf(cmd, strlen(file)+7, "touch %s", file);
		if ( system(cmd) != 0 )
		{
			LogWrite("Error: Unable to create bytes file.\n");
			exit(EXIT_FAILURE);
		}
		free(cmd);
	}
}
