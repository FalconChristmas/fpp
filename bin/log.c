#include "log.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

FILE *logFile;
const char *filename = "/home/pi/media/fppLog.txt";

static bool verbose = false;
static bool foreground = false;
void _LogWrite(char *file, int line, const char *format, ...)
{
	return;
	va_list arg;
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	char timeStr[32];
	sprintf(timeStr,"%.2d-%.2d-%d %.2d:%.2d:%.2d",
	                tm.tm_mon+1,tm.tm_mday,tm.tm_year,tm.tm_hour,tm.tm_min,tm.tm_sec);
	if ( verbose )
	{
		fprintf(stdout, "%s  %s:%d:", timeStr, file, line);
		va_start(arg, format);
		vfprintf(stdout, format, arg);
		va_end(arg);
	}

	if ( ! foreground )	
	{
		logFile = fopen(filename, "a");
		fprintf(logFile, "%s  %s:%d:",timeStr, file, line);
		va_start(arg, format);
		vfprintf(logFile, format, arg);
		va_end(arg);

		fclose(logFile);
	}
}
