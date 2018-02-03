/*
 *   output channel thread for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "channeloutput.h"
#include "common.h"
#include "controlsend.h"
#include "effects.h"
#include "fppd.h"
#include "log.h"
#include "PixelOverlay.h"
#include "Sequence.h"
#include "settings.h"

/* used by external sync code */
int   RefreshRate = 20;
int   DefaultLightDelay = 0;
int   LightDelay = 0;
int   MasterFramesPlayed = -1;
int   OutputFrames = 1;
float mediaOffset = 0.0;

/* local variables */
pthread_t ChannelOutputThreadID;
int       RunThread = 0;
int       ThreadIsRunning = 0;

/* prototypes for functions below */
void CalculateNewChannelOutputDelayForFrame(int expectedFramesSent);

/*
 * Check to see if the channel output thread is running
 */
int ChannelOutputThreadIsRunning(void) {
	return ThreadIsRunning;
}

/*
 *
 */
void DisableChannelOutput(void) {
	LogDebug(VB_CHANNELOUT, "DisableChannelOutput()\n");
	OutputFrames = 0;
}

/*
 *
 */
void EnableChannelOutput(void) {
	LogDebug(VB_CHANNELOUT, "EnableChannelOutput()\n");
	OutputFrames = 1;
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
	int onceMore = 0;
	struct timespec ts;
	int syncFrameCounter = 0;

	LogDebug(VB_CHANNELOUT, "RunChannelOutputThread() starting\n");

	ThreadIsRunning = 1;

	if ((getFPPmode() == REMOTE_MODE) &&
		(!IsEffectRunning()) &&
		(!UsingMemoryMapInput()) &&
		(!channelTester->Testing()) &&
		(!getAlwaysTransmit()))
	{
		// Sleep about 2 seconds waiting for the master
		int loops = 0;
		while ((MasterFramesPlayed < 0) && (loops < 200))
		{
			usleep(10000);
			loops++;
		}

		// Stop playback if the master hasn't sent any sync packets yet
		if (MasterFramesPlayed < 0)
			RunThread = 0;
	}

	StartOutputThreads();

	while (RunThread)
	{
		startTime = GetTime();

		if ((getFPPmode() == MASTER_MODE) &&
			(sequence->IsSequenceRunning()))
		{
			if (syncFrameCounter & 0x10)
			{
				// Send sync every 16 frames (use 16 to make the check simpler)
				syncFrameCounter = 1;
				SendSeqSyncPacket(
					sequence->m_seqFilename, channelOutputFrame,
					(mediaElapsedSeconds > 0) ? mediaElapsedSeconds
						: 1.0 * channelOutputFrame / RefreshRate );
			}
			else
			{
				syncFrameCounter++;
			}
		}

		if (OutputFrames)
			sequence->SendSequenceData();

		sendTime = GetTime();

		if (getFPPmode() != BRIDGE_MODE)
			sequence->ReadSequenceData();

		sequence->ProcessSequenceData();

		readTime = GetTime();

		if ((sequence->IsSequenceRunning()) ||
			(IsEffectRunning()) ||
			(UsingMemoryMapInput()) ||
			(channelTester->Testing()) ||
			(getAlwaysTransmit()) ||
			(getFPPmode() == BRIDGE_MODE))
		{
			onceMore = 1;

			if (startTime > (lastStatTime + 1000000)) {
				int sleepTime = LightDelay - (GetTime() - startTime);
				if (sleepTime < 0)
					sleepTime = 0;
				lastStatTime = startTime;
				LogDebug(VB_CHANNELOUT,
					"Output Thread: Loop: %dus, Send: %lldus, Read: %lldus, Sleep: %dus, FrameNum: %ld\n",
					LightDelay, sendTime - startTime,
					readTime - sendTime, sleepTime, channelOutputFrame);
			}
		}
		else
		{
			LightDelay = DefaultLightDelay;

			if (onceMore)
				onceMore = 0;
			else
				RunThread = 0;
		}

		// Calculate how long we need to nanosleep()
		ts.tv_sec = 0;
		ts.tv_nsec = (LightDelay - (GetTime() - startTime)) * 1000;
		nanosleep(&ts, NULL);
	}

	StopOutputThreads();

	ThreadIsRunning = 0;

	LogDebug(VB_CHANNELOUT, "RunChannelOutputThread() completed\n");

	pthread_exit(NULL);
}

/*
 * Set the step time
 */
void SetChannelOutputRefreshRate(int rate)
{
	RefreshRate = rate;
	DefaultLightDelay = 1000000 / RefreshRate;
}

/*
 * Kick off the channel output thread
 */
int StartChannelOutputThread(void)
{
	LogDebug(VB_CHANNELOUT, "StartChannelOutputThread()\n");

	int E131BridgingInterval = getSettingInt("E131BridgingInterval");

	if ((getFPPmode() == BRIDGE_MODE) && (E131BridgingInterval))
		DefaultLightDelay = E131BridgingInterval * 1000;
	else
		DefaultLightDelay = 1000000 / RefreshRate;

	LightDelay = DefaultLightDelay;

	if (ChannelOutputThreadIsRunning())
	{
		// Give a little time in case we were shutting down
		usleep(200000);
		if (ChannelOutputThreadIsRunning())
		{
			LogDebug(VB_CHANNELOUT, "Channel Output thread is already running\n");
			return 1;
		}
	}

	int mediaOffsetInt = getSettingInt("mediaOffset");
	if (mediaOffsetInt)
		mediaOffset = (float)mediaOffsetInt * 0.001;
	else
		mediaOffset = 0.0;

	LogDebug(VB_MEDIAOUT, "Using mediaOffset of %.3f\n", mediaOffset);

	RunThread = 1;
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

	// Wait for thread to start
	while (!ChannelOutputThreadIsRunning())
		usleep(10000);
}

/*
 *
 */
int StopChannelOutputThread(void)
{
	int i = 0;

	// Stop the thread and wait a few seconds
	RunThread = 0;
	while (ThreadIsRunning && (i < 5))
	{
		sleep(1);
		i++;
	}

	// Didn't stop for some reason, so it was hung somewhere
	if (ThreadIsRunning)
		return -1;

	return 0;
}

/*
 * Reset the master frames played position
 */
void ResetMasterPosition(void)
{
	MasterFramesPlayed = -1;
}

/*
 * Update the count of frames that the master has played so we can sync to it
 */
void UpdateMasterPosition(int frameNumber)
{
	MasterFramesPlayed = frameNumber;
	CalculateNewChannelOutputDelayForFrame(frameNumber);
}

/*
 * Calculate the new sync offset based on the current position reported
 * by the media player.
 */
void CalculateNewChannelOutputDelay(float mediaPosition)
{
	static float nextSyncCheck = 0.5;

	if (getFPPmode() == REMOTE_MODE)
		return;

	if ((mediaPosition <= nextSyncCheck) &&
		(nextSyncCheck < (mediaPosition + 2.0)))
		return;

	nextSyncCheck = mediaPosition + (20.0 / RefreshRate);

	float offsetMediaPosition = mediaPosition - mediaOffset;

	int expectedFramesSent = (int)(offsetMediaPosition * RefreshRate);

	mediaElapsedSeconds = mediaPosition;

	LogDebug(VB_CHANNELOUT,
		"Media Position: %.2f, Offset: %.3f, Frames Sent: %d, Expected: %d, Diff: %d\n",
		mediaPosition, mediaOffset, channelOutputFrame, expectedFramesSent,
		channelOutputFrame - expectedFramesSent);

	CalculateNewChannelOutputDelayForFrame(expectedFramesSent);
}

/*
 * Calculate the new sync offset based on a desired frame number
 */
void CalculateNewChannelOutputDelayForFrame(int expectedFramesSent)
{
	int diff = channelOutputFrame - expectedFramesSent;
	if (diff)
	{
		int timerOffset = diff * (DefaultLightDelay / 100);
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

		// Don't let us go more than 15ms out from the default.  If we
		// can't keep up using that delta then we probably won't be able to.
		if ((DefaultLightDelay - 15000) > newLightDelay)
			newLightDelay = DefaultLightDelay - 15000;

		LogDebug(VB_CHANNELOUT, "LightDelay: %d, newLightDelay: %d\n",
			LightDelay, newLightDelay);
		LightDelay = newLightDelay;
	}
	else if (LightDelay != DefaultLightDelay)
	{
		LightDelay = DefaultLightDelay;
	}
}

