#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include <stdbool.h>

#define MAXBUF 1024

struct config
{
	bool	verbose;
	bool	daemonize;

	int		fppMode;
	int		volume;

	char	*settingsFile;
	char	*mediaDirectory;
	char	*musicDirectory;
	char	*sequenceDirectory;
	char	*playlistDirectory;
	char	*universeFile;
	char	*pixelnetFile;
	char	*scheduleFile;
	char	*logFile;
	char	*silenceMusic;

	char	*MPG123Path;
	char	*bytesFile;
};


// Helper functions
char *trimwhitespace(const char *str);
void printSettings(void);


// Action functions
int parseArguments(int argc, char **argv);
int loadSettings(const char *filename);
void CreateSettingsFile(char * file);
void CheckExistanceOfDirectoriesAndFiles(void);
int saveSettingsFile(void);


// Getters
bool getVerbose(void);
bool getDaemonize(void);
int  getFPPmode(void);
int  getVolume(void);
char *getSettingsFile(void);
char *getMediaDirectory(void);
char *getMusicDirectory(void);
char *getSequenceDirectory(void);
char *getPlaylistDirectory(void);
char *getUniverseFile(void);
char *getPixelnetFile(void);
char *getScheduleFile(void);
char *getLogFile(void);
char *getSilenceMusic(void);
char *getMPG123Path(void);
char *getBytesFile(void);


// Setters
void setVolume(int volume);


#endif //__SETTINGS_H__
