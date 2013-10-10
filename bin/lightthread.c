#include <signal.h>
#include <stdio.h>
#include <sys/time.h>

#include "E131.h"
#include "log.h"
#include "ogg123.h"

float RefreshRate = 20.00;

pthread_t LightThreadID;
int DefaultLightDelay = 0;
int LightDelay = 0;
int RunThread = 0;
int ThreadIsRunning = 0;

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
 * Main loop in lighting thread
 */
void *RunLightThread(void)
{
	long long startTime;
	struct timespec ts;

	ThreadIsRunning = 1;
	while (RunThread)
	{
		startTime = GetTime();
		E131_Send();

		if (E131status != E131_STATUS_IDLE)
		{
			int elapsedTime = GetTime() - startTime;
			int delay = LightDelay - elapsedTime;

			LogWrite("RLT: elapsed: %d, delay: %d\n", elapsedTime, delay);

			ts.tv_sec = 0;
			ts.tv_nsec = (LightDelay - (GetTime() - startTime)) * 1000;
			LogWrite("RLT: ts.tv_nsec: %d\n", ts.tv_nsec);
			nanosleep(&ts, NULL);
//			usleep(LightDelay - (GetTime() - startTime));
		}
		else
		{
			RunThread = 0;
		}
	}

	ThreadIsRunning = 0;
	pthread_exit(NULL);
}

/*
 * Kick off the lighting thread
 */
int StartLightThread(void)
{
	if (ThreadIsRunning)
		return 1;

	RunThread = 1;
	DefaultLightDelay = 1 / RefreshRate * 1000000;
	LightDelay = DefaultLightDelay;

	pthread_create(&LightThreadID, NULL,&RunLightThread, NULL);
}

/*
 * Stop the lighting thread
 */
int StopLightThread(void)
{
	RunThread = 0;
	pthread_join(LightThreadID,NULL);
}

/*
 * Check to see if the lighting thread is running
 */
int IsLightThreadRunning(void)
{
	return ThreadIsRunning;
}

