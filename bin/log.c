#include "log.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>

FILE *logFile;

static bool verbose = false;
static bool foreground = false;

void _LogWrite(const char *format, ...)
{
	va_list arg;
	int done;

	if ( verbose )
	{
		va_start(arg, format);
		done = vfprintf(stdout, format, arg);
		va_end(arg);
	}

	if ( ! foreground )
	{
	    logFile = fopen("fppdLog.txt", "a");

		va_start(arg, format);
		done = fprintf(logFile, format, arg);
		va_end(arg);

	    fclose(logFile);
	}
}
