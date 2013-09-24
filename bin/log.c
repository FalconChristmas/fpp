#include "log.h"
#include "settings.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>


void _LogWrite(char *file, int line, const char *format, ...)
{
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
	if ( getVerbose() == FPP_TRUE )
	{
		fprintf(stdout, "%s  %s:%d:", timeStr, file, line);
		va_start(arg, format);
		vfprintf(stdout, format, arg);
		va_end(arg);
	}

	if ( getDaemonize() == FPP_TRUE )	
	{
		FILE *logFile;

		logFile = fopen((const char *)getLogFile(), "a");
		if ( ! logFile )
		{
			fprintf(stderr, "Error: Unable to write to log file!\n");
			return;
		}
		fprintf(logFile, "%s  %s:%d:",timeStr, file, line);
		va_start(arg, format);
		vfprintf(logFile, format, arg);
		va_end(arg);

		fclose(logFile);
	}
}
