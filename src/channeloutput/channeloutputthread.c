#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "channeloutput.h"
#include "../common.h"
#include "../log.h"
#include "../memorymap.h"
#include "../sequence.h"

/* used by external sync code */
float RefreshRate = 20.00;
int   DefaultLightDelay = 0;
int   LightDelay = 0;

/* local variables */
pthread_t ChannelOutputThreadID;
int       RunThread = 0;
int       ThreadIsRunning = 0;

/*
 * Check to see if the channel output thread is running
 */
inline int ChannelOutputThreadIsRunning(void) {
	return ThreadIsRunning;
}
/*
 * Main loop in channel output thread
 */
void *RunChannelOutputThread(void *data)
{
	(void)data;

	static long long lastStatTime = 0;
	long long startTime;
	long long sendTime;
	long long readTime;
	struct timespec ts;

	ThreadIsRunning = 1;
	while (RunThread)
	{
		startTime = GetTime();
		SendSequenceData();
		sendTime = GetTime();
		ReadSequenceData();
		readTime = GetTime();

		if ((IsSequenceRunning()) ||
			(IsEffectRunning()) ||
			(UsingMemoryMapInput()))
		{
			if (startTime > (lastStatTime + 1000000)) {
				int sleepTime = LightDelay - (GetTime() - startTime);
				lastStatTime = startTime;
				LogDebug(VB_CHANNELOUT,
					"Output Thread: Loop: %dus, Send: %dus, Read: %dus, Sleep: %dus\n",
					LightDelay, sendTime - startTime,
					readTime - sendTime, sleepTime);
			}

			ts.tv_sec = 0;
			ts.tv_nsec = (LightDelay - (GetTime() - startTime)) * 1000;

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
 * Kick off the channel output thread
 */
int StartChannelOutputThread(void)
{
	if (ChannelOutputThreadIsRunning())
		return 1;

	RunThread = 1;
	DefaultLightDelay = 1 / RefreshRate * 1000000;
	LightDelay = DefaultLightDelay;

	int result = pthread_create(&ChannelOutputThreadID, NULL, &RunChannelOutputThread, NULL);

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
		LogErr(VB_CHANNELOUT, "ERROR creating channel output thread: %s\n", msg );
	}
	else
	{
		pthread_detach(ChannelOutputThreadID);
	}
}

/*
 * Calculate the new sync offset based on the current position reported
 * by the media player.
 */
void CalculateNewChannelOutputDelay(float mediaPosition)
{
	int expectedFramesSent = (int)(mediaPosition * RefreshRate);

	LogDebug(VB_CHANNELOUT,
		"Media Position: %.2f, Frames Sent: %d, Expected: %d, Diff: %d\n",
		mediaPosition, channelOutputFrame, expectedFramesSent,
		channelOutputFrame - expectedFramesSent);

	int diff = channelOutputFrame - expectedFramesSent;
	if (diff)
	{
		int timerOffset = diff * 500;
		int newLightDelay = LightDelay;

		if (channelOutputFrame >  expectedFramesSent)
		{
			// correct if we slingshot past 0, otherwise offset further
			if (LightDelay < DefaultLightDelay)
				newLightDelay = DefaultLightDelay;
			else
				newLightDelay += timerOffset;
		}
		else
		{
			// correct if we slingshot past 0, otherwise offset further
			if (LightDelay > DefaultLightDelay)
				newLightDelay = DefaultLightDelay;
			else
				newLightDelay += timerOffset;
		}

		// This is bad if we hit this, but still don't let us go negative
		if (newLightDelay < 0)
			newLightDelay = 0;

		LogDebug(VB_CHANNELOUT, "LightDelay: %d, newLightDelay: %d\n",
			LightDelay, newLightDelay);
		LightDelay = newLightDelay;
	}
	else if (LightDelay != DefaultLightDelay)
	{
		LightDelay = DefaultLightDelay;
	}
}

