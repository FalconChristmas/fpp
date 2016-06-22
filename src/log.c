/*
 *   Log handler for Falcon Pi Player (FPP)
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

#include "fppversion.h"
#include "log.h"
#include "settings.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

int logLevel = LOG_INFO;
int logMask  = VB_MOST;

char logFileName[1024] = "";
char logLevelStr[16];
char logMaskStr[1024];

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
	struct tm tm;
	char timeStr[32];

	localtime_r(&t, &tm);
	sprintf(timeStr,"%4d-%.2d-%.2d %.2d:%.2d:%.2d",
					1900+tm.tm_year,
					tm.tm_mon+1,
					tm.tm_mday,
					tm.tm_hour,
					tm.tm_min,
					tm.tm_sec);

	if (logFileName[0])
	{
		FILE *logFile;

		if (!strcmp(logFileName, "stderr"))
		{
			logFile = stderr;
		}
		else if (!strcmp(logFileName, "stdout"))
		{
			logFile = stdout;
		}
		else
		{
			logFile = fopen(logFileName, "a");
			if ( ! logFile )
			{
				fprintf(stderr, "Error: Unable to open log file for writing!\n");
				fprintf(stderr, "%s (%d) %s:%d:",timeStr, syscall(SYS_gettid), file, line);
				va_start(arg, format);
				vfprintf(stderr, format, arg);
				va_end(arg);
				return;
			}
		}

		fprintf(logFile, "%s (%d) %s:%d:",timeStr, syscall(SYS_gettid), file, line);
		va_start(arg, format);
		vfprintf(logFile, format, arg);
		va_end(arg);

		if (strcmp(logFileName, "stderr") || strcmp(logFileName, "stdout"))
			fclose(logFile);
	} else {
		fprintf(stdout, "%s (%d) %s:%d:", timeStr, syscall(SYS_gettid), file, line);
		va_start(arg, format);
		vfprintf(stdout, format, arg);
		va_end(arg);
	}
}

void SetLogFile(char *filename)
{
	strcpy(logFileName, filename);
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
		} else if (!strcmp(s, "plugin")) {
			logMask |= VB_PLUGIN;
		} else if (!strcmp(s, "gpio")) {
			logMask |= VB_GPIO;
#ifdef USEHTTPAPI
		} else if (!strcmp(s, "http")) {
			logMask |= VB_HTTP;
#endif
		} else {
			fprintf(stderr, "Unknown Log Mask: %s\n", s);
		}
		s = strtok(NULL, delim);
	}

	strcpy(logMaskStr, newMask);

	return 1;
}

int loggingToFile(void)
{
	if ((logFileName[0]) &&
		(strcmp(logFileName, "stderr")) &&
		(strcmp(logFileName, "stdout")))
		return 1;

	return 0;
}

void logVersionInfo(void) {
	LogErr(VB_ALL, "=========================================\n");
	LogErr(VB_ALL, "FPP %s\n", getFPPVersion());
	LogErr(VB_ALL, "Branch: %s\n", getFPPBranch());
	LogErr(VB_ALL, "=========================================\n");
}

