/*
 *   output channel thread for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
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
#include <mutex>
#include <thread>

#include "channeloutput.h"
#include "common.h"
#include "effects.h"
#include "fppd.h"
#include "log.h"
#include "MultiSync.h"
#include "overlays/PixelOverlay.h"
#include "Sequence.h"
#include "settings.h"

#include "mediaoutput/SDLOut.h"

/* used by external sync code */
float   RefreshRate = 20;
int   SequenceLightDelay = 50000;
int   BridgeLightDelay = 50000;
int   LightDelay = 50000;
volatile int FrameSkip = 0;
int   MasterFramesPlayed = -1;
int   OutputFrames = 1;
float mediaOffset = 0.0;

/* local variables */
pthread_t ChannelOutputThreadID;
volatile int     RunThread = 0;
volatile int     ThreadIsRunning = 0;
volatile int     ThreadIsExiting = 0;
volatile int     outputForced = 0;
volatile int     alwaysTransmit = 0;

std::mutex outputThreadLock;
std::mutex outputThreadStatusLock;
std::condition_variable outputThreadCond;
std::condition_variable outputThreadSatusCond;

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

void ForceChannelOutputNow(void) {
    LogDebug(VB_CHANNELOUT, "ForceChannelOutputNow()\n");
    outputThreadSatusCond.notify_all();
    outputThreadCond.notify_all();
}

static inline bool forceOutput() {
    return IsEffectRunning() ||
        PixelOverlayManager::INSTANCE.hasActiveOverlays() ||
        SDLOutput::IsOverlayingVideo() ||
        ChannelTester::INSTANCE.Testing() ||
        alwaysTransmit ||
        sequence->hasBridgeData() ||
        outputForced;
}

/*
 * Main loop in channel output thread
 */
void *RunChannelOutputThread(void *data)
{
	static long long lastStatTime = 0;
	long long startTime;
	long long sendTime;
	long long readTime;
    long long processTime;
    int onceMore = (getFPPmode() == REMOTE_MODE) ? 20 : 1;
	struct timespec ts;
    struct timeval tv;
    int slowFrameCount = 0;

    alwaysTransmit = getSettingInt("alwaysTransmit");

	LogDebug(VB_CHANNELOUT, "RunChannelOutputThread() starting\n");

    std::unique_lock<std::mutex> lock(outputThreadLock);
    std::unique_lock<std::mutex> statusLock(outputThreadStatusLock);
	ThreadIsRunning = 1;
    ThreadIsExiting = 0;
    statusLock.unlock();
    
    StartOutputThreads();

	if ((getFPPmode() == REMOTE_MODE) && !forceOutput())
	{
		// Sleep about 2 seconds waiting for the master
		int loops = 0;
		while ((MasterFramesPlayed < 0) && (loops < 2000) && !forceOutput())
		{
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
			loops++;
		}

		// Stop playback if the master hasn't sent any sync packets yet
		if (MasterFramesPlayed < 0)
			RunThread = 0;
	}

    bool doForceOutput = false;
	while (RunThread) {
		startTime = GetTime();
		if (multiSync->isMultiSyncEnabled() && sequence->IsSequenceRunning()) {
            multiSync->SendSeqSyncPacket(
                sequence->m_seqFilename, channelOutputFrame,
                (mediaElapsedSeconds > 0) ? mediaElapsedSeconds
                    : 1.0 * channelOutputFrame / RefreshRate );
		}

        doForceOutput |= forceOutput();
        if (OutputFrames) {
            if (!sequence->isDataProcessed() || sequence->hasBridgeData()) {
                //first time through or immediately after sequence load, the data might not be
                //processed yet, need to do it
                int msTime = 1000.0 * channelOutputFrame / RefreshRate;
                if (!sequence->IsSequenceRunning()) {
                    msTime = mediaElapsedSeconds * 1000;
                }
                sequence->ProcessSequenceData(msTime, 1);
            }
            if (getFPPmode() == REMOTE_MODE && !doForceOutput) {
                // Sleep about 1 seconds waiting for the master
                int loops = 0;
                while ((MasterFramesPlayed < 0) && (loops < 1000) && !sequence->hasBridgeData()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    loops++;
                }
            }
			sequence->SendSequenceData();
        }

		sendTime = GetTime();


        if (sequence->IsSequenceRunning() || (onceMore >= 1)) {
            if (FrameSkip && sequence->IsSequenceRunning()) {
                sequence->SeekSequenceFile(channelOutputFrame + FrameSkip + 1);
                FrameSkip = 0;
            }
            sequence->ReadSequenceData();
        }
        readTime = GetTime();

        int msTime = 1000.0 * channelOutputFrame / RefreshRate;
        if (!sequence->IsSequenceRunning()) {
            msTime = mediaElapsedSeconds * 1000;
        }
        if (!sequence->hasBridgeData()) {
            //if bridging, we'll have to process later
            sequence->ProcessSequenceData(msTime, 1);
        }
		processTime = GetTime();
        
        long long totalTime = processTime - startTime;
        if (totalTime > 150000) {
            //very slow, log immediately
            slowFrameCount =  3;
        } else if (totalTime > 50000) {
            //could be a very transient blip, we'll log if
            //it happens 3 frames in a row
            slowFrameCount++;
        } else {
            slowFrameCount = 0;
        }
        if (slowFrameCount > 3) {
            LogWarn(VB_CHANNELOUT,
                 "SLOW Output Thread: Loop: %dus, Send: %lldus, Read: %lldus, Process: %lldus, FrameNum: %ld\n",
            LightDelay,
            sendTime - startTime,
            readTime - sendTime,
            processTime - readTime,
            channelOutputFrame);
        }

        statusLock.lock();
		if (sequence->IsSequenceRunning() || doForceOutput || sequence->hasBridgeData()) {
            // REMOTE mode keeps looping a few extra times before we blank
            onceMore = (getFPPmode() == REMOTE_MODE) ? 20 : 1;
            int sleepTime = LightDelay - (processTime - startTime);
			if ((channelOutputFrame <= 1) || (sleepTime <= 0) || (startTime > (lastStatTime + 1000000))) {
				if (sleepTime < 0)
					sleepTime = 0;
                if (startTime > (lastStatTime + 1000000)) {
                    lastStatTime = startTime;
                }
				LogDebug(VB_CHANNELOUT,
                         "Output Thread: Loop: %dus, Send: %lldus, Read: %lldus, Process: %lldus, Sleep: %dus, FrameNum: %ld\n",
					LightDelay,
                    sendTime - startTime,
					readTime - sendTime,
                    processTime - readTime, 
                    sleepTime, channelOutputFrame);
			}
		} else {
			LightDelay = BridgeLightDelay;

            if (onceMore) {
				onceMore--;
            } else {
                //we will wait up to 2500ms to see if the thread is still needed
                ThreadIsExiting = 1;
                if (outputThreadSatusCond.wait_for(statusLock, std::chrono::milliseconds(2500)) == std::cv_status::no_timeout) {
                    //signal to keep going
                    ThreadIsExiting = 0;
                    statusLock.unlock();
                    outputThreadSatusCond.notify_all();
                    onceMore = 1;
                    continue;
                } else {
                    RunThread = 0;
                    ThreadIsExiting = 1;
                }
            }
		}
        statusLock.unlock();
        doForceOutput = false;
		// Calculate how long we need to nanosleep()
		long dt = (LightDelay - (GetTime() - startTime)) * 1000;
		if (RunThread && dt > 0) {
            if (outputThreadCond.wait_for(lock, std::chrono::nanoseconds(dt)) == std::cv_status::no_timeout ) {
				LogDebug(VB_CHANNELOUT, "Forced output\n");
                doForceOutput = true;
			}
        }
	}

	StopOutputThreads();
    statusLock.lock();
    ThreadIsRunning = 0;
    statusLock.unlock();
    lock.unlock();
    statusLock.lock();
    ThreadIsExiting = 0;
    statusLock.unlock();
    outputThreadSatusCond.notify_all();

	LogDebug(VB_CHANNELOUT, "RunChannelOutputThread() completed\n");

	pthread_exit(NULL);
}

/*
 * Set the step time
 */
void SetChannelOutputRefreshRate(float rate)
{
	RefreshRate = rate;
	SequenceLightDelay = 1000000 / RefreshRate;
    LightDelay = SequenceLightDelay;
}
float GetChannelOutputRefreshRate() {
    return RefreshRate;
}

/*
 * Kick off the channel output thread
 */
void StartChannelOutputThread(void)
{
	LogDebug(VB_CHANNELOUT, "StartChannelOutputThread()\n");


    BridgeLightDelay = getSettingInt("E131BridgingInterval", 50) * 1000;
    if (getFPPmode() & PLAYER_MODE) {
        int mediaOffsetInt = getSettingInt("mediaOffset");
        if (mediaOffsetInt) {
            mediaOffset = (float)mediaOffsetInt * 0.001;
        } else {
            mediaOffset = 0.0;
        }
        LogDebug(VB_MEDIAOUT, "Using mediaOffset of %.3f\n", mediaOffset);
    }

    outputThreadSatusCond.notify_all();
    if (ChannelOutputThreadIsRunning()) {
        // Give a little time in case we were shutting down
        std::unique_lock<std::mutex> lock(outputThreadStatusLock);
        if (ThreadIsExiting) {
            outputThreadSatusCond.wait_for(lock, std::chrono::milliseconds(10));
        }
        if (ThreadIsRunning && !ThreadIsExiting) {
            LogDebug(VB_CHANNELOUT, "Channel Output thread is already running\n");
            return;
        }
	}

	RunThread = 1;
    ThreadIsExiting = 0;
	int result = pthread_create(&ChannelOutputThreadID, NULL, &RunChannelOutputThread, NULL);

	if (result) {
        char msg[256];

        RunThread = 0;
		switch (result) {
			case EAGAIN: strcpy(msg, "Insufficient Resources");
				break;
			case EINVAL: strcpy(msg, "Invalid settings");
				break;
			case EPERM : strcpy(msg, "Invalid Permissions");
				break;
		}
		LogErr(VB_CHANNELOUT, "ERROR creating channel output thread: %s\n", msg );
	} else {
		pthread_detach(ChannelOutputThreadID);
	}

	// Wait for thread to start
	while (!ChannelOutputThreadIsRunning())
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

/*
 *
 */
int StopChannelOutputThread(void)
{
	int i = 0;
    LogDebug(VB_CHANNELOUT, "StopChannelOutputThread()\n");

	// Stop the thread and wait a few seconds
	RunThread = 0;
	while (ThreadIsRunning && (i < 50)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		i++;
	}

	// Didn't stop for some reason, so it was hung somewhere
	if (ThreadIsRunning)
		return -1;
    
	return 0;
}

void StartForcingChannelOutput(void)
{
    outputForced = outputForced + 1;

    StartChannelOutputThread();
}

void StopForcingChannelOutput(void)
{
    outputForced = outputForced - 1;

    if (outputForced < 0)
        outputForced = 0;
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

    mediaElapsedSeconds = mediaPosition;

	if (getFPPmode() == REMOTE_MODE || !sequence->IsSequenceRunning())
		return;

	if ((mediaPosition <= nextSyncCheck) &&
		(nextSyncCheck < (mediaPosition + 2.0)))
		return;

	nextSyncCheck = mediaPosition + (20.0 / RefreshRate);

	float offsetMediaPosition = mediaPosition - mediaOffset;

	int expectedFramesSent = (int)(offsetMediaPosition * RefreshRate);

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
    if (!multiSync->isMultiSyncEnabled()) {
        if (diff < -4) {
            // pretty far behind master, lets just skip forward
            if (diff > -(RefreshRate/2)) {
                LogDebug(VB_CHANNELOUT, "Skipping a few frames - We are at %d, master is at: %d\n", channelOutputFrame, expectedFramesSent);
                // off, but not super off, we'll skip a few frames, but not too much to try and keep using
                // the frames in the cache and avoid hitting the storage, we'll then have the OS preload
                // the next bunch and we can skip a few more next time
                FrameSkip = 4;
                diff += 4;
            } else {
                LogDebug(VB_CHANNELOUT, "Skipping many frames - We are at %d, master is at: %d\n", channelOutputFrame, expectedFramesSent);
                //more than 1/2 second behind, just jump
                FrameSkip = expectedFramesSent - channelOutputFrame;
                LightDelay = sequence->IsSequenceRunning() ? SequenceLightDelay : BridgeLightDelay;
                return;
            }
        } else if (diff > 2) {
            //hold the last frame
            FrameSkip = -1;
            diff--;
        }
    }
    int DefaultLightDelay = sequence->IsSequenceRunning() ? SequenceLightDelay : BridgeLightDelay;
	if (diff > 1 || diff < -1) {
		int timerOffset = diff * (DefaultLightDelay / 100);
		int newLightDelay = LightDelay;
		if (channelOutputFrame >  expectedFramesSent) {
			// correct if we slingshot past 0, otherwise offset further
			if (LightDelay < DefaultLightDelay)
				newLightDelay = DefaultLightDelay;
			else
				newLightDelay += timerOffset;
		} else {
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

		LogDebug(VB_CHANNELOUT, "LightDelay: %d, newLightDelay: %d,   DiffFrames: %d     %d/%d\n",
			LightDelay, newLightDelay, diff,   channelOutputFrame , expectedFramesSent);
		LightDelay = newLightDelay;
    } else if (diff == -1) {
        //for the one frame off cases, keep the existing light delay unless the
        //previous "off" frame was on the other side of default
        if (LightDelay > DefaultLightDelay) {
            LightDelay = DefaultLightDelay;
        }
    } else if (diff == 1) {
        if (LightDelay < DefaultLightDelay) {
            LightDelay = DefaultLightDelay;
        }
	} else if (LightDelay != DefaultLightDelay) {
		LightDelay = DefaultLightDelay;
	}
}

