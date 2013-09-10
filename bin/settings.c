#include "settings.h"
#include "fppd.h"
#include "log.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>


static struct config settings = { 0 };

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
	{
		*out = 0;
		return NULL;
	}

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && (isspace(*end))) end--;
	end++;

	// Set output size to minimum of trimmed string length
	out_size = end - str;

	// All whitespace
	if ( out_size == 0 )
		return NULL;

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
			settings.verbose ? "true" : "false" );
	fprintf(fd, "daemonize: %s\n",
			settings.daemonize ? "true" : "false" );
	
	fprintf(fd, "fppMode: %s\n", "TODO");//TODO: enum this
	fprintf(fd, "volume: %u\n", settings.volume);

	if ( settings.settingsFile )
		fprintf(fd, "settingsFile(%u): %s\n",
				strlen(settings.settingsFile),
				settings.settingsFile);
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
	if ( settings.MPG123Path )
		fprintf(fd, "mpg123Path(%u): %s\n",
				strlen(settings.MPG123Path),
				settings.MPG123Path);
}

int parseArguments(int argc, char **argv)
{
	settings.daemonize = true;

	/*
c config-file
f foreground
V verbose
v volume
m mode
B media-directory
M music-directory
S sequence-directory
P playlist-directory
u universe-file
p pixelnet-file
s schedule-file
l log-file
b bytes-file
mpg123-path
silence-music
	*/

	int c;
	while (1)
	{
		int this_option_optind = optind ? optind : 1;
		int option_index = 0;
		static struct option long_options[] =
		{
			{"config-file",			required_argument,	0, 'c'},
			{"foreground",			no_argument,		0, 'f'},
			{"verbose",				no_argument,		0, 'V'},
			{"volume",				required_argument,	0, 'v'},
			{"mode",				required_argument,	0, 'm'},
			{"media-directory",		required_argument,	0, 'B'},
			{"music-directory",		required_argument,	0, 'M'},
			{"sequence-directory",	required_argument,	0, 'S'},
			{"playlist-directory",	required_argument,	0, 'P'},
			{"universe-file",		required_argument,	0, 'u'},
			{"pixelnet-file",		required_argument,	0, 'p'},
			{"schedule-file",		required_argument,	0, 's'},
			{"log-file",			required_argument,	0, 'l'},
			{"silence-music",		required_argument,	0,	1 },
			{"mpg123-path",			required_argument,	0,	2 },
			{"bytes-file",			required_argument,	0, 'b'},
			{0,						0,					0,	0}
		};

		c = getopt_long(argc, argv, "c:fVv:m:B:M:S:P:u:p:s:l:b:",
		long_options, &option_index);
		if (c == -1)
			break;

		switch (c)
		{
			case 1: //silence-music
				settings.silenceMusic = strdup(optarg);
				break;
			case 2: //mpg123-path
				settings.MPG123Path = strdup(optarg);
				break;
			case 'c': //config-file
				settings.settingsFile = strdup(optarg);
				break;
			case 'f': //foreground
				settings.daemonize = false;
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
				settings.mediaDirectory = strdup(optarg);
				break;
			case 'M': //music-directory
				settings.musicDirectory = strdup(optarg);
				break;
			case 'S': //sequence-directory
				settings.sequenceDirectory = strdup(optarg);
				break;
			case 'P': //playlist-directory
				settings.playlistDirectory = strdup(optarg);
				break;
			case 'u': //universe-file
				settings.universeFile = strdup(optarg);
				break;
			case 'p': //pixelnet-file
				settings.pixelnetFile = strdup(optarg);
				break;
			case 's': //schedule-file
				settings.scheduleFile = strdup(optarg);
				break;
			case 'l': //log-file
				settings.logFile = strdup(optarg);
				break;
			case 'b': //bytes-file
				settings.bytesFile = strdup(optarg);
				break;
			default:
				printf("?? getopt returned character code 0%o ??\n", c);
		}
	}

	return 0;
}

int loadSettings(const char *filename)
{
	FILE *file = fopen (filename, "r");

	if (file != NULL)
	{
		char * line = NULL;
		size_t len = 0;
		ssize_t read;

		while ((read = getline(&line, &len, file)) != -1)
		{
			if (( ! line ) || ( ! read ) || ( read == 1 ))
				continue;

			char *token = strtok(line, "=");
			if ( ! token )
				continue;

			token = trimwhitespace(token);
			if ( ! token )
				continue;

			if ( strcmp(token, "verbose") == 0 )
			{
				token = trimwhitespace(strtok(NULL, "="));
				if ( strcmp(token, "false") == 0 )
					settings.verbose = false;
				else if ( strcmp(token, "true") == 0 )
					settings.verbose = true;
				else
				{
					fprintf(stderr, "Failed to load verbose setting from config file\n");
					exit(EXIT_FAILURE);
				}
				free(token); token = NULL;
			}
			else if ( strcmp(token, "daemonize") == 0 )
			{
				token = trimwhitespace(strtok(NULL, "="));
				if ( strcmp(token, "false") == 0 )
					settings.daemonize = false;
				else if ( strcmp(token, "true") == 0 )
					settings.daemonize = true;
				else
				{
					fprintf(stderr, "Failed to load daemonize setting from config file\n");
					exit(EXIT_FAILURE);
				}
				free(token); token = NULL;
			}
			else if ( strcmp(token, "fppMode") == 0 )
			{
				if ( settings.fppMode == DEFAULT_MODE )
				{
					//TODO: enum here
					token = trimwhitespace(strtok(NULL, "="));
					settings.fppMode = atoi(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "volume") == 0 )
			{
				token = trimwhitespace(strtok(NULL, "="));
				settings.volume = atoi(token);
				free(token); token = NULL;
			}
			else if ( strcmp(token, "settingsFile") == 0 )
			{
				if ( ! settings.settingsFile )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.settingsFile = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "mediaDirectory") == 0 )
			{
				if ( ! settings.mediaDirectory )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.mediaDirectory = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "musicDirectory") == 0 )
			{
				if ( ! settings.musicDirectory )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.musicDirectory = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "sequenceDirectory") == 0 )
			{
				if ( ! settings.sequenceDirectory )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.sequenceDirectory = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "playlistDirectory") == 0 )
			{
				if ( ! settings.playlistDirectory )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.playlistDirectory = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "universeFile") == 0 )
			{
				if ( ! settings.universeFile )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.universeFile = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "pixelnetFile") == 0 )
			{
				if ( ! settings.pixelnetFile )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.pixelnetFile = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "scheduleFile") == 0 )
			{
				if ( ! settings.scheduleFile )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.scheduleFile = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "logFile") == 0 )
			{
				if ( ! settings.logFile )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.logFile = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "silenceMusic") == 0 )
			{
				if ( ! settings.silenceMusic )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.silenceMusic = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "mpg123Path") == 0 )
			{
				if ( ! settings.silenceMusic )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.MPG123Path = strdup(token);
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "bytesFile") == 0 )
			{
				if ( ! settings.bytesFile )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.bytesFile = strdup(token);
					free(token); token = NULL;
				}
			}
			else
			{
				fprintf(stderr, "Error parsing config file\n");
				exit(EXIT_FAILURE);
			}

			if ( line )
			{
				free(line);
				line = NULL;
			}
			
		}
	}
	else
	{
		fprintf(stderr,"Failed to open settings file!\n");
		exit(EXIT_FAILURE);
	}

	fclose(file);

	return 0;
}

bool getVerbose(void)
{
	return settings.verbose;
}

bool getDaemonize(void)
{
	return settings.daemonize;
}

int getFPPmode(void)
{
	if ( settings.fppMode == DEFAULT_MODE )
	{
		LogWrite("Default mode coded for PLAYER\n");
		return PLAYER_MODE;
	}

	return settings.fppMode;
}

int getVolume(void)
{
	return settings.volume;
}

char *getSettingsFile(void)
{
	if ( !settings.settingsFile )
		return "/home/pi/media/settings";

	return settings.settingsFile;
}

char *getMediaDirectory(void)
{
	if ( !settings.mediaDirectory )
		return "/home/pi/media";

	return settings.mediaDirectory;
}
char *getMusicDirectory(void)
{
	if ( !settings.musicDirectory )
		return "/home/pi/media/music";

	return settings.musicDirectory;
}
char *getSequenceDirectory(void)
{
	if ( !settings.sequenceDirectory )
		return "/home/pi/media/sequences";

	return settings.sequenceDirectory;
}
char *getPlaylistDirectory(void)
{
	if ( !settings.playlistDirectory )
		return "/home/pi/media/playlists";

	return settings.playlistDirectory;
}
char *getUniverseFile(void)
{
	if ( !settings.universeFile )
		return "/home/pi/media/universes";

	return settings.universeFile;
}
char *getPixelnetFile(void)
{
	if ( !settings.pixelnetFile )
		return "/home/pi/media/pixelnetDMX";

	return settings.pixelnetFile;
}
char *getScheduleFile(void)
{
	if ( !settings.scheduleFile )
		return "/home/pi/media/schedule";

	return settings.scheduleFile;
}
char *getLogFile(void)
{
	if ( !settings.logFile )
		return "/home/pi/media/fppdLog.txt";

	return settings.logFile;
}
char *getSilenceMusic(void)
{
	if ( !settings.silenceMusic )
		return "/home/pi/media/music/silence.mp3";

	return settings.silenceMusic;
}
char *getMPG123Path(void)
{
	if ( !settings.MPG123Path )
		return "/usr/bin/mpg123";

	return settings.MPG123Path;
}
char *getBytesFile(void)
{
	if ( !settings.bytesFile )
		return "/home/pi/media/bytesReceived";

	return settings.bytesFile;
}

void setVolume(int volume)
{
	settings.volume = volume;

	if ( saveSettingsFile() != 0 )
	{
		LogWrite("Error saving settings!\n");
	}
}

int saveSettingsFile(void)
{
	//TODO
	return 0;
}

void CreateSettingsFile(char * file)
{
  FILE *fp;
	char * settings = "0,100";			// Mode, Volume
	char command[32];//FIXME
	fp = fopen(file, "w");
	LogWrite("Creating file: %s\n",file);
	if ( !fp )
	{
		LogWrite("Couldn't open file for writing: %d\n", errno);
		exit(errno);
	}
	fwrite(settings, 1, 4, fp);
	fclose(fp);
	sprintf(command,"sudo chmod 777 %s",file);
	system(command);
}

void CheckExistanceOfDirectoriesAndFiles(void)
{
	if(!DirectoryExists(getMediaDirectory()))
	{
		mkdir(getMediaDirectory(), 0777);
		LogWrite("Directory FPP Does Not Exist\n");
	}
	if(!DirectoryExists(getMusicDirectory()))
	{
		mkdir(getMusicDirectory(), 0777);
		LogWrite("Directory Music Does Not Exist\n");
	}
	if(!DirectoryExists(getSequenceDirectory()))
	{
		mkdir(getSequenceDirectory(), 0777);
		LogWrite("Directory sequences Does Not Exist\n");
	}
	if(!DirectoryExists(getPlaylistDirectory()))
	{
		mkdir(getPlaylistDirectory(), 0777);
		LogWrite("Directory playlists Does Not Exist\n");
	}

	if(!FileExists(getUniverseFile()))
	{
		char *cmd, *file = getUniverseFile();
		cmd = malloc(strlen(file)+6);
		snprintf(cmd, strlen(file)+6, "touch %s", file);
		system(cmd);
		free(cmd);
	}
	if(!FileExists(getPixelnetFile()))
	{
		CreatePixelnetDMXfile(getPixelnetFile());
	}
	if(!FileExists(getScheduleFile()))
	{
		char *cmd, *file = getScheduleFile();
		cmd = malloc(strlen(file)+6);
		snprintf(cmd, strlen(file)+6, "touch %s", file);
		system(cmd);
		free(cmd);
	}
	if(!FileExists(getBytesFile()))
	{
		char *cmd, *file = getBytesFile();
		cmd = malloc(strlen(file)+6);
		snprintf(cmd, strlen(file)+6, "touch %s", file);
		system(cmd);
		free(cmd);
	}
	if(!FileExists(getSettingsFile()))
	{
		CreateSettingsFile(getSettingsFile());
	}
}
