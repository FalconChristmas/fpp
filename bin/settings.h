#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdbool.h>

#define MAXBUF 1024

struct config
{
	int		verbose;
	int		daemonize;

	int		fppMode;
	int		volume;

	char	*mediaDirectory;
	char	*musicDirectory;
	char	*sequenceDirectory;
	char	*eventDirectory;
	char	*playlistDirectory;
	char	*universeFile;
	char	*pixelnetFile;
	char	*scheduleFile;
	char	*logFile;
	char	*silenceMusic;

	char	*MPG123Path;
	char	*bytesFile;

	unsigned int controlMajor;
	unsigned int controlMinor;
};


// Helpers
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
int getVerbose(void);
int getDaemonize(void);
int  getFPPmode(void);
int  getVolume(void);
char *getMediaDirectory(void);
char *getMusicDirectory(void);
char *getSequenceDirectory(void);
char *getEventDirectory(void);
char *getPlaylistDirectory(void);
char *getUniverseFile(void);
char *getPixelnetFile(void);
char *getScheduleFile(void);
char *getLogFile(void);
char *getSilenceMusic(void);
char *getMPG123Path(void);
char *getBytesFile(void);
unsigned int getControlMajor(void);
unsigned int getControlMinor(void);


// Setters
void setVolume(int volume);


#endif //__SETTINGS_H__
