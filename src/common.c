#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
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
