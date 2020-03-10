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

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdbool.h>
#include <map>
#include <string>

#define MAXBUF 1024

typedef enum fppMode {
	BRIDGE_MODE = 0x01,
	PLAYER_MODE = 0x02,
	/* Skip 0x04 since MASTER_MODE is a bitmask of player 0x02 & master 0x04 */
	MASTER_MODE = 0x06,
	REMOTE_MODE = 0x08
} FPPMode;

class SettingsConfig {
public:
    SettingsConfig() {}
    ~SettingsConfig();
    
    int        daemonize;
    int        restarted;
    FPPMode    fppMode;
    int        alwaysTransmit;
    char    *binDirectory = nullptr;
    char    *fppDirectory = nullptr;
    char    *mediaDirectory = nullptr;
    char    *musicDirectory = nullptr;
    char    *sequenceDirectory = nullptr;
    char    *eventDirectory = nullptr;
    char    *videoDirectory = nullptr;
    char    *effectDirectory = nullptr;
    char    *scriptDirectory = nullptr;
    char    *pluginDirectory = nullptr;
    char    *playlistDirectory = nullptr;
    char    *pixelnetFile = nullptr;
    char    *logFile = nullptr;
    char    *silenceMusic = nullptr;
    char    *settingsFile = nullptr;
    char    *E131interface = nullptr;
    
    unsigned int controlMajor;
    unsigned int controlMinor;
    
    std::map<std::string, char *> keyVal;
};

// Helpers
void initSettings(int argc, char **argv);
char *trimwhitespace(const char *str, int quotesAlso = 1);
char *modeToString(int mode);
void usage(char *appname);


// Action functions
int parseArguments(int argc, char **argv);
int loadSettings(const char *filename);
void CreateSettingsFile(char * file);
void CheckExistanceOfDirectoriesAndFiles(void);
int parseSetting(char *key, char *value);

// Getters
const char *getSetting(const char *setting);
int   getSettingInt(const char *setting, int defaultVal = 0);

int getDaemonize(void);
int getRestarted(void);
FPPMode getFPPmode(void);
int  getAlwaysTransmit(void);
char *getBinDirectory(void);
char *getFPPDirectory(void);
char *getMediaDirectory(void);
char *getMusicDirectory(void);
char *getSequenceDirectory(void);
char *getEventDirectory(void);
char *getVideoDirectory(void);
char *getEffectDirectory(void);
char *getScriptDirectory(void);
char *getPluginDirectory(void);
char *getPlaylistDirectory(void);
char *getUniverseFile(void);
char *getPixelnetFile(void);
char *getLogFile(void);
char *getSilenceMusic(void);
char *getSettingsFile(void);
char *getE131interface(void);
unsigned int getControlMajor(void);
unsigned int getControlMinor(void);


#endif //__SETTINGS_H__
