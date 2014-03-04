#include "log.h"
#include "settings.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

// FIXME, need a way to set/adjust these
int logLevel = LOG_INFO;
int logMask  = VB_MOST;

void _LogWrite(char *file, int line, int level, int facility, const char *format, ...)
{
	// Don't log if we're not logging this facility
	if (!(logMask & facility))
		return;

	// Don't if we're not concerned about anything at or below this level
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
	if ( getVerbose() )
	{
		fprintf(stdout, "%s  %s:%d:", timeStr, file, line);
		va_start(arg, format);
		vfprintf(stdout, format, arg);
		va_end(arg);
	}

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
	}
}
