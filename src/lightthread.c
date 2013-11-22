#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

#include "E131.h"
#include "log.h"
#include "ogg123.h"
#include "common.h"


float RefreshRate = 20.00;

pthread_t LightThreadID;
int DefaultLightDelay = 0;
int LightDelay = 0;
int RunThread = 0;
int ThreadIsRunning = 0;

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
		E131_ReadData();

		if (IsSequenceRunning() || IsEffectRunning())
		{
			ts.tv_sec = 0;
			ts.tv_nsec = (LightDelay - (GetTime() - startTime)) * 1000;
			// This will kill performance writing out this 20x a second to the SD
			//LogWrite("RLT: ts.tv_nsec: %d\n", ts.tv_nsec);
			nanosleep(&ts, NULL);
		}
		else
		{
			RunThread = 0;
			LightDelay = DefaultLightDelay;
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

	int result = pthread_create(&LightThreadID, NULL,&RunLightThread, NULL);

	if (result)
	{
		char msg[256];

		RunThread = 0;
		switch (result)
		{
			case EAGAIN: strcpy(msg, "Insufficient Resources");
				break;
			case EINVAL: strcpy(msg, "Invalid settings");
				break;
			case EPERM : strcpy(msg, "Invalid Permissions");
				break;
		}
		LogWrite("ERROR creating lighting thread: %s\n", msg );
	}
	else
	{
		pthread_detach(LightThreadID);
	}
}

/*
 * Check to see if the lighting thread is running
 */
int IsLightThreadRunning(void)
{
	return ThreadIsRunning;
}

