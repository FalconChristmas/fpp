/*
 *   Setting manager for Falcon Player (FPP)
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


#include "settings.h"
#include "fppversion.h"
#include "log.h"
#include "fpp.h"

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include "common.h"

const char *fpp_bool_to_string[] = { "false", "true", "default" };

SettingsConfig settings;

SettingsConfig::~SettingsConfig() {
    if (binDirectory) free(binDirectory);
    if (fppDirectory) free(fppDirectory);
    if (mediaDirectory) free(mediaDirectory);
    if (musicDirectory) free(musicDirectory);
    if (sequenceDirectory) free(sequenceDirectory);
    if (eventDirectory) free(eventDirectory);
    if (videoDirectory) free(videoDirectory);
    if (effectDirectory) free(effectDirectory);
    if (scriptDirectory) free(scriptDirectory);
    if (pluginDirectory) free(pluginDirectory);
    if (playlistDirectory) free(playlistDirectory);
    if (pixelnetFile) free(pixelnetFile);
    if (logFile) free(logFile);
    if (silenceMusic) free(silenceMusic);
    if (settingsFile) free(settingsFile);
    if (E131interface) free(E131interface);
    
    for (auto &a :keyVal) {
        if (a.second) {
            free(a.second);
        }
    }
}

/*
 *
 */
void initSettings(int argc, char **argv)
{
    char tmpDir[256];
    char mediaDir[256];
    
    memset(tmpDir, 0, sizeof(tmpDir));
    memset(mediaDir, 0, sizeof(mediaDir));
    
	settings.binDirectory = strdup(dirname(argv[0]));
    if (strlen(settings.binDirectory) == 1 && settings.binDirectory[0] == '.') {
        getcwd(tmpDir, sizeof(tmpDir));
        free(settings.binDirectory);
        settings.binDirectory = strdup(tmpDir);
    }

	settings.fppMode = PLAYER_MODE;
    
    strcpy(tmpDir, settings.binDirectory);

    // trim off src/ or bin/
    char *offset = NULL;
    int size = strlen(tmpDir);

    if ((size > 4) && (!strcmp(&tmpDir[size - 4], "/src")))
        offset = &tmpDir[size - 4];
    else if ((size > 4) && (!strcmp(&tmpDir[size - 4], "/bin")))
        offset = &tmpDir[size - 4];
    else if ((size > 8) && (!strcmp(&tmpDir[size - 8], "/scripts")))
        offset = &tmpDir[size - 8];

    if (offset != NULL)
        *offset = 0;
    
    settings.fppDirectory = strdup(tmpDir);

	strcpy(mediaDir, "/home/fpp/media");
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
	settings.pixelnetFile = strdup(strcat(tmpDir, "/config/Falcon.FPDV1"));
	strcpy(tmpDir, mediaDir);
	settings.logFile = strdup(strcat(tmpDir, "/logs/fppd.log"));
	strcpy(tmpDir, mediaDir);
	settings.silenceMusic = strdup(strcat(tmpDir, "/silence.ogg"));
	strcpy(tmpDir, mediaDir);
	settings.settingsFile = strdup(strcat(tmpDir, "/settings"));
	settings.daemonize = 1;
	settings.restarted = 0;
	settings.E131interface = strdup("eth0");
	settings.controlMajor = 0;
	settings.controlMinor = 0;

	SetLogLevel("info");
	SetLogMask("most");
}

// Returns a string that's the white-space trimmed version
// of the input string.  Also trim double quotes now that the
// settings file will have double quotes in it.
char *trimwhitespace(const char *str, int quotesAlso)
{
	const char *end;
	size_t out_size;
	char *out;

	// Trim leading space
	while((isspace(*str)) || (quotesAlso && (*str == '"'))) str++;

	if(*str == 0)  // All spaces?
		return strdup("");

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && ((isspace(*end)) || (quotesAlso && (*end == '"')))) end--;
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
		return strdup("player");
	else if ( mode == BRIDGE_MODE )
		return strdup("bridge");
	else if ( mode == MASTER_MODE )
		return strdup("master");
	else if ( mode == REMOTE_MODE )
		return strdup("remote");
	
	return NULL;
}

int parseSetting(char *key, char *value)
{
    if (settings.keyVal[key]) {
        free(settings.keyVal[key]);
        settings.keyVal[key] = strdup(value);
    }

	if ( strcmp(key, "daemonize") == 0 )
	{
		settings.daemonize = atoi(value);
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
		char * line = (char*)calloc(256, 1);
		size_t len = 256;
		ssize_t read;
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

            settings.keyVal[key] = strdup(value);

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

    SetLogFile(getLogFile(), !getDaemonize());
	return 0;
}

const char *getSetting(const char *setting)
{
	if (!setting) {
		LogErr(VB_SETTING, "getSetting() called with NULL value\n");
		return "";
	}

    if (settings.keyVal[setting] != nullptr) {
        return settings.keyVal[setting];
	}

	LogExcess(VB_SETTING, "getSetting(%s) returned setting not found\n", setting);
	return "";
}

int getSettingInt(const char *setting, int defaultVal)
{
	const char *valueStr = getSetting(setting);
    if (!valueStr || *valueStr == 0) {
        return defaultVal;
    }
	int   value = strtol(valueStr, NULL, 10);

	LogExcess(VB_SETTING, "getSettingInt(%s) returning %d\n", setting, value);

	return value;
}

int getDaemonize(void)
{
	return settings.daemonize;
}

int getRestarted(void)
{
	return settings.restarted;
}

#ifndef __GNUG__
inline
#endif
FPPMode getFPPmode(void)
{
	return settings.fppMode;
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
char *getPixelnetFile(void)
{
	return settings.pixelnetFile;
}
char *getLogFile(void)
{
	return settings.logFile;
}
char *getSilenceMusic(void)
{
	return settings.silenceMusic;
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

static inline bool createFile(const char *file) {
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    int i = open(file, O_RDWR | O_CREAT | O_TRUNC, mode);
    if (i < 0) {
        return false;
    }
    struct passwd *pwd = getpwnam("fpp");
    fchown(i, pwd->pw_uid, pwd->pw_gid);
    close(i);
    return true;
}

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

	if(!FileExists(getSettingsFile()))
	{
		LogWarn(VB_SETTING, "Settings file does not exist, creating it.\n");
        if (!createFile(getSettingsFile())) {
			LogErr(VB_SETTING, "Error: Unable to create settings file.\n");
			exit(EXIT_FAILURE);
		}
	}
  


  
}
