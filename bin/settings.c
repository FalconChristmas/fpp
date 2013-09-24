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

	if ( settings.verbose != FPP_DEFAULT )
		fprintf(fd, "verbose: %s\n",
				fpp_bool_to_string[settings.verbose]);
	if ( settings.daemonize != FPP_DEFAULT )
		fprintf(fd, "daemonize: %s\n",
				fpp_bool_to_string[settings.daemonize]);
	
	if ( settings.fppMode == PLAYER_MODE )
		fprintf(fd, "fppMode: %s\n", "player");//TODO: enum this
	else if ( settings.fppMode == BRIDGE_MODE )
		fprintf(fd, "fppMode: %s\n", "bridge");//TODO: enum this
	if ( settings.volume != -1 )
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
"\t    --silence-music\tSet location of silence.mp3 file\n", appname);
}

int parseArguments(int argc, char **argv)
{
	settings.daemonize = FPP_DEFAULT;
	settings.verbose = FPP_DEFAULT;
	settings.volume = -1;
	settings.fppMode = DEFAULT_MODE;

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
			{"universe-file",		required_argument,	0, 'u'},
			{"pixelnet-file",		required_argument,	0, 'p'},
			{"schedule-file",		required_argument,	0, 's'},
			{"log-file",			required_argument,	0, 'l'},
			{"silence-music",		required_argument,	0,	1 },
			{"mpg123-path",			required_argument,	0,	2 },
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
				settings.silenceMusic = strdup(optarg);
				break;
			case 2: //mpg123-path
				settings.MPG123Path = strdup(optarg);
				break;
			case 'c': //config-file
				settings.settingsFile = strdup(optarg);
				break;
			case 'f': //foreground
				settings.daemonize = FPP_FALSE;
				break;
			case 'd': //daemonize
				settings.daemonize = FPP_TRUE;
				break;
			case 'V': //verbose
				settings.verbose = FPP_TRUE;
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

			char *token = strtok(line, "=");
			if ( ! token )
				continue;

			token = trimwhitespace(token);
			if ( ! token )
				continue;

			if ( strcmp(token, "verbose") == 0 )
			{
				if ( settings.verbose == FPP_DEFAULT )
				{
					token = trimwhitespace(strtok(NULL, "="));
					if ( strcmp(token, "false") == 0 )
						settings.verbose = FPP_FALSE;
					else if ( strcmp(token, "true") == 0 )
						settings.verbose = FPP_TRUE;
					else if ( settings.verbose != FPP_DEFAULT )
					{
						fprintf(stderr, "Failed to load verbose setting from config file\n");
						exit(EXIT_FAILURE);
					}
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "daemonize") == 0 )
			{
				if ( settings.daemonize == FPP_DEFAULT )
				{
					token = trimwhitespace(strtok(NULL, "="));
					if ( strcmp(token, "false") == 0 )
						settings.daemonize = FPP_FALSE;
					else if ( strcmp(token, "true") == 0 )
						settings.daemonize = FPP_TRUE;
					else
					{
						fprintf(stderr, "Failed to load daemonize setting from config file\n");
						exit(EXIT_FAILURE);
					}
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "fppMode") == 0 )
			{
				if ( settings.fppMode == DEFAULT_MODE )
				{
					token = trimwhitespace(strtok(NULL, "="));
					if ( strcmp(token, "player") == 0 )
						settings.fppMode = PLAYER_MODE;
					else if ( strcmp(token, "bridge") == 0 )
						settings.fppMode = BRIDGE_MODE;
					else
					{
						printf("Error parsing mode\n");
						exit(EXIT_FAILURE);
					}
					free(token); token = NULL;
				}
			}
			else if ( strcmp(token, "volume") == 0 )
			{
				if ( settings.volume != -1 )
				{
					token = trimwhitespace(strtok(NULL, "="));
					settings.volume = atoi(token);
					free(token); token = NULL;
				}
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
		fprintf(stderr,"Warning: couldn't open settings file, creating default!\n");
		return saveSettingsFile();
	}

	fclose(file);

	return 0;
}

int getVerbose(void)
{
	if ( settings.verbose == FPP_DEFAULT )
	{
//		Don't use LogWrite here since it reads this setting and will
//		cause a stack overflow when not set!
		fprintf(stderr, "Default verbosity set to false\n");
		settings.verbose = FPP_FALSE;
	}

	return settings.verbose;
}

int getDaemonize(void)
{
	if ( settings.daemonize == FPP_DEFAULT )
	{
//		Don't use LogWrite here since it reads this setting and will
//		cause a stack overflow when not set!
		fprintf(stderr, "Default daemonization set to true\n");
		settings.daemonize = FPP_TRUE;
	}

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
	if ( settings.volume == -1 )
	{
		LogWrite("Default volume coded at 75\n");
		return 75;
	}

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
	snprintf(buffer, 1024, "%s = %s\n", "settingsFile", getSettingsFile());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "mediaDirectory", getMediaDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "musicDirectory", getMusicDirectory());
	bytes += fwrite(buffer, 1, strlen(buffer), fd);
	snprintf(buffer, 1024, "%s = %s\n", "sequenceDirectory", getSequenceDirectory());
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

	fclose(fd);

	LogWrite("Wrote config file of size %d\n", bytes);

	return 0;
}

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
