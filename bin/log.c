#include "log.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

FILE *logFile;
const char *filename = "/home/pi/media/fppLog.txt";

static bool verbose = false;
static bool foreground = false;

void _LogWrite(char *file, int line, const char *format, ...)
{
	va_list arg;

	if ( verbose )
	{
		fprintf(stdout, "%s:%d:", file, line);
		va_start(arg, format);
		vfprintf(stdout, format, arg);
		va_end(arg);
	}

	if ( ! foreground )
	{
		logFile = fopen(filename, "a");

		fprintf(logFile, "%s:%d:", file, line);
		va_start(arg, format);
		fprintf(logFile, format, arg);
		va_end(arg);

		fclose(logFile);
	}
}
