#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "common.h"

/*
 * Get the current time down to the microsecond
 */
long long GetTime(void)
{
	struct timeval now_tv;
	gettimeofday(&now_tv, NULL);
	return now_tv.tv_sec * 1000000LL + now_tv.tv_usec;
}

/*
 * Check to see if the specified directory exists
 */
int DirectoryExists(char * Directory)
{
	DIR* dir = opendir(Directory);
	if (dir)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

/*
 * Check if the specified file exists or not
 */
int FileExists(char * File)
{
	struct stat sts;
	if (stat(File, &sts) == -1 && errno == ENOENT)
	{
		return 0;
	}
	else
	{
		return 1;
	}
	
}

