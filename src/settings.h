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

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdbool.h>

#define MAXBUF 1024

enum fppModes {
	BRIDGE_MODE = 0x01,
	PLAYER_MODE = 0x02,
	/* Skip 0x04 since MASTER_MODE is a bitmask of player 0x02 & master 0x04 */
	MASTER_MODE = 0x06,
	REMOTE_MODE = 0x08
};

struct config
{
	int		daemonize;
	int		fppMode;
	int		volume;
	char	*mediaDirectory;
	char	*musicDirectory;
	char	*sequenceDirectory;
	char	*eventDirectory;
	char	*videoDirectory;
	char	*effectDirectory;
	char	*scriptDirectory;
	char	*pluginDirectory;
	char	*playlistDirectory;
	char	*universeFile;
	char	*pixelnetFile;
	char	*scheduleFile;
	char	*logFile;
	char	*silenceMusic;
	char	*settingsFile;
	char	*bytesFile;
	char	*E131interface;

	unsigned int controlMajor;
	unsigned int controlMinor;

	char *keys[1024];
	char *values[1024];
};


// Helpers
void initSettings(void);
char *trimwhitespace(const char *str);
void printSettings(void);
void usage(char *appname);


// Action functions
int parseArguments(int argc, char **argv);
int loadSettings(const char *filename);
void CreateSettingsFile(char * file);
void CheckExistanceOfDirectoriesAndFiles(void);
int saveSettingsFile(void);


// Getters
char *getSetting(char *setting);
int   getSettingInt(char *setting);

int getDaemonize(void);
int  getFPPmode(void);
int  getVolume(void);
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
char *getScheduleFile(void);
char *getLogFile(void);
char *getSilenceMusic(void);
char *getBytesFile(void);
char *getSettingsFile(void);
char *getE131interface(void);
unsigned int getControlMajor(void);
unsigned int getControlMinor(void);


// Setters
void setVolume(int volume);


#endif //__SETTINGS_H__
