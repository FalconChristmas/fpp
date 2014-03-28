#include "log.h"
#include "settings.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>

int logLevel = LOG_INFO;
int logMask  = VB_MOST;

char logLevelStr[16];
char logMaskStr[128];


void _LogWrite(char *file, int line, int level, int facility, const char *format, ...)
{
	// Don't log if we're not logging this facility
	if (!(logMask & facility))
		return;

	// Don't log if we're not concerned about anything at this level
	if (logLevel < level)
		return;

	va_list arg;
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char timeStr[32];
	sprintf(timeStr,"%4d-%.2d-%.2d %.2d:%.2d:%.2d",
					1900+tm.tm_year,
					tm.tm_mon+1,
					tm.tm_mday,
					tm.tm_hour,
					tm.tm_min,
					tm.tm_sec);

	if ( getDaemonize() )	
	{
		FILE *logFile;

		logFile = fopen((const char *)getLogFile(), "a");
		if ( ! logFile )
		{
			fprintf(stderr, "Error: Unable to open log file for writing!\n");
			fprintf(stderr, "%s  %s:%d:",timeStr, file, line);
			va_start(arg, format);
			vfprintf(stderr, format, arg);
			va_end(arg);
			return;
		}
		fprintf(logFile, "%s  %s:%d:",timeStr, file, line);
		va_start(arg, format);
		vfprintf(logFile, format, arg);
		va_end(arg);

		fclose(logFile);
	} else {
		fprintf(stdout, "%s  %s:%d:", timeStr, file, line);
		va_start(arg, format);
		vfprintf(stdout, format, arg);
		va_end(arg);
	}
}

int SetLogLevel(char *newLevel)
{
	if (!strcmp(newLevel, "warn")) {
		logLevel = LOG_WARN;
	} else if (!strcmp(newLevel, "debug")) {
		logLevel = LOG_DEBUG;
	} else if (!strcmp(newLevel, "info")) {
		logLevel = LOG_INFO;
	} else if (!strcmp(newLevel, "excess")) {
		logLevel = LOG_EXCESSIVE;
	} else {
		LogErr(VB_SETTING, "Unknown Log Level: %s\n", newLevel);
		return 0;
	}

	strcpy(logLevelStr, newLevel);

	return 1;
}

int SetLogMask(char *newMask)
{
	char delim[2];

	// we use comma separated lists everywhere except when
	// transferring over the command socket which is already
	// comma-separated, so use semi-colons there instead
	if (strchr(newMask, ','))
		strcpy(delim, ",");
	else if (strchr(newMask, ';'))
		strcpy(delim, ";");
	else
		strcpy(delim, ",");

	logMask = VB_NONE;

	char tmpMask[256];
	strcpy(tmpMask, newMask);

	char *s = strtok(tmpMask, delim);
	while (s) {
		if (!strcmp(s, "none")) {
			logMask = VB_NONE;
		} else if (!strcmp(s, "all")) {
			logMask = VB_ALL;
		} else if (!strcmp(s, "most")) {
			logMask = VB_MOST;
		} else if (!strcmp(s, "general")) {
			logMask |= VB_GENERAL;
		} else if (!strcmp(s, "channelout")) {
			logMask |= VB_CHANNELOUT;
		} else if (!strcmp(s, "channeldata")) {
			logMask |= VB_CHANNELDATA;
		} else if (!strcmp(s, "command")) {
			logMask |= VB_COMMAND;
		} else if (!strcmp(s, "e131bridge")) {
			logMask |= VB_E131BRIDGE;
		} else if (!strcmp(s, "effect")) {
			logMask |= VB_EFFECT;
		} else if (!strcmp(s, "event")) {
			logMask |= VB_EVENT;
		} else if (!strcmp(s, "mediaout")) {
			logMask |= VB_MEDIAOUT;
		} else if (!strcmp(s, "playlist")) {
			logMask |= VB_PLAYLIST;
		} else if (!strcmp(s, "schedule")) {
			logMask |= VB_SCHEDULE;
		} else if (!strcmp(s, "sequence")) {
			logMask |= VB_SEQUENCE;
		} else if (!strcmp(s, "setting")) {
			logMask |= VB_SETTING;
		} else if (!strcmp(s, "sync")) {
			logMask |= VB_SYNC;
		} else if (!strcmp(s, "control")) {
			logMask |= VB_CONTROL;
		} else {
			LogErr(VB_SETTING, "Unknown Log Mask: %s\n", s);
			return 0;
		}
		s = strtok(NULL, delim);
	}

	strcpy(logMaskStr, newMask);

	return 1;
}


