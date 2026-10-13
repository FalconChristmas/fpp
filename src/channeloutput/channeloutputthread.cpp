/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "../fpp-pch.h"

#include <sys/time.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <mutex>
#include <thread>

#include "../MultiSync.h"
#include "../Sequence.h"
#include "../Warnings.h"
#include "../channeltester/ChannelTester.h"
#include "../commands/Commands.h"
#include "../common.h"
#include "../effects.h"
#include "../log.h"
#include "../mediaoutput/GStreamerOut.h"
#include "../overlays/PixelOverlay.h"
#include "../settings.h"

#include "ChannelOutputSetup.h"
#include "channeloutputthread.h"

/* used by external sync code */
// Atomic: the output thread, media sync (on the media or MultiSync thread)
// and sequence start all read and write these.
std::atomic<float> RefreshRate{ 20 };
std::atomic_int SequenceLightDelay{ 50000 };
std::atomic_int BridgeLightDelay{ 0 };
std::atomic_int LightDelay{ 50000 };
std::atomic_int FrameSkip{ 0 };
std::atomic_int MasterFramesPlayed{ -1 };
std::atomic_int OutputFrames{ 1 };
float mediaOffset = 0.0;

/* local variables */
// Atomic, not volatile: volatile orders nothing between threads, and the main
// loop, HTTP threads and MultiSync read and set these while the output thread
// runs.
std::atomic_int RunThread{ 0 };
std::atomic_int ThreadIsRunning{ 0 };
std::atomic_int ThreadIsExiting{ 0 };
std::atomic_int outputForced{ 0 };
std::atomic_int alwaysTransmit{ 0 };

// declared in channeloutputthread.h, which documents what it protects
std::mutex outputThreadLock;
std::recursive_mutex inlineOutputLock;
std::mutex outputThreadStatusLock;
// bumped under outputThreadStatusLock by every StartChannelOutputThread()
static uint64_t outputStartRequests = 0;
std::condition_variable outputThreadCond;
std::condition_variable outputThreadSatusCond;

// The pace of the media clock the sequence follows, relative to the system
// clock, and when it was last reported (GetMonotonicTimeMicros()).  A card's crystal is
// tens to hundreds of ppm off; frames run at its pace so the lights don't walk
// across media sync's whole-frame dead band over a song.  It goes stale (back
// to 1.0) a second after the media stops reporting it.
static std::atomic<double> mediaClockRate{ 1.0 };
static std::atomic<long long> mediaClockRateAt{ 0 };

void SetMediaClockRate(double rate) {
    mediaClockRate = rate;
    mediaClockRateAt = GetMonotonicTimeMicros();
}

static double CurrentMediaClockRate(long long now) {
    // A remote follows the master's frame numbers, not its own media.
    if (now - mediaClockRateAt.load() > 1000000 || !sequence->IsSequenceRunning() ||
        getFPPmode() == REMOTE_MODE) {
        return 1.0;
    }
    return mediaClockRate.load();
}

/* prototypes for functions below */
void CalculateNewChannelOutputDelayForFrame(int expectedFramesSent);

/*
 * Check to see if the channel output thread is running
 */
int ChannelOutputThreadIsRunning(void) {
    return ThreadIsRunning;
}

int ChannelOutputThreadIsEnabled() {
    return OutputFrames;
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

// Bumped under outputThreadStatusLock by every ForceChannelOutputNow().  The
// thread sends a frame for each change it sees, whichever wait it is in.  A
// bare notify was lost whenever the thread wasn't waiting on that condition
// at that moment -- and the idle wait never listened for it at all -- so a
// Stop Now landing within a frame of a sequence hand-over blanked the buffer
// and nothing ever sent it: the last frame stayed lit.
static std::atomic<uint64_t> outputForceRequests{ 0 };

bool ForceChannelOutputNow(void) {
    LogDebug(VB_CHANNELOUT, "ForceChannelOutputNow()\n");
    {
        std::unique_lock<std::mutex> statusLock(outputThreadStatusLock);
        if (!ThreadIsRunning || !RunThread) {
            // Not running, or on its way out: it won't send again.
            return false;
        }
        outputForceRequests++;
    }
    outputThreadSatusCond.notify_all();
    outputThreadCond.notify_all();
    return true;
}

static inline bool forceOutput() {
    return IsEffectRunning() ||
           PixelOverlayManager::INSTANCE.hasActiveOverlays() ||
#ifdef HAS_GSTREAMER
           GStreamerOutput::IsOverlayingVideo() ||
#endif
           ChannelTester::INSTANCE.Testing() ||
           alwaysTransmit ||
           sequence->hasBridgeData() ||
           outputForced;
}

/*
 * Main loop in channel output thread
 */
void RunChannelOutputThread(void) {
    SetThreadName("FPP-ChannelOut");

    // Frame timing must stay consistent under load; a delayed frame bunches
    // with the next one and doubles the size of the UDP burst hitting the
    // network.  The loop sleeps every frame (and kernel RT throttling caps
    // RT CPU at ~95%) so this will not starve the rest of the system.
    if (SetThreadRealtimePriority(10)) {
        LogInfo(VB_CHANNELOUT, "Channel output thread running with real-time priority\n");
    } else {
        LogInfo(VB_CHANNELOUT, "Could not set real-time priority on channel output thread; frame timing may jitter under load\n");
    }

    static long long lastStatTime = 0;
    long long startTime;
    long long sendTime;
    long long readTime;
    long long processTime;
    int onceMore = (getFPPmode() == REMOTE_MODE) ? 20 : 1;
    // When the next frame is due (GetMonotonicTimeMicros()), 0 before the
    // first.  Every time in this loop is on the monotonic clock: the cadence
    // carries nextFrameTime from frame to frame, so on the wall clock a step
    // back (NTP, an RTC that boots in the past) left the next frame due that
    // much later, and output stopped for as long as the step.
    long long nextFrameTime = 0;
    double frameTimeCarry = 0.0;
    struct timespec ts;
    struct timeval tv;
    int slowFrameCount = 0;

    LogDebug(VB_CHANNELOUT, "RunChannelOutputThread() starting\n");

    std::unique_lock<std::mutex> lock(outputThreadLock);
    std::unique_lock<std::mutex> statusLock(outputThreadStatusLock);
    ThreadIsRunning = 1;
    ThreadIsExiting = 0;
    statusLock.unlock();

    StartingOutput();

    if ((getFPPmode() == REMOTE_MODE) && !forceOutput()) {
        // Sleep about 2 seconds waiting for the master
        int loops = 0;
        while ((MasterFramesPlayed < 0) && (loops < 2000) && !forceOutput()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            loops++;
        }

        // Stop playback if the master hasn't sent any sync packets yet
        if (MasterFramesPlayed < 0)
            RunThread = 0;
    }

    bool doForceOutput = false;
    uint64_t forceSeen = outputForceRequests.load();
    while (RunThread) {
        startTime = GetMonotonicTimeMicros();
        // A forced frame asked for before this pass's send is that send.
        uint64_t forceNow = outputForceRequests.load();
        if (forceNow != forceSeen) {
            forceSeen = forceNow;
            doForceOutput = true;
        }
        if (multiSync->isMultiSyncEnabled() && sequence->IsSequenceRunning()) {
            // Per-frame string copy under an uncontended leaf lock: the name is
            // short and the only writers are the open/close paths, so this costs
            // far less than the sync packet it feeds.
            multiSync->SendSeqSyncPacket(sequence->GetSeqFilenameCopy(), channelOutputFrame, 1.0 * ((float)channelOutputFrame) / RefreshRate);
        }

        doForceOutput |= forceOutput();
        if (OutputFrames) {
            if (!sequence->isDataProcessed() || sequence->hasBridgeData()) {
                // first time through or immediately after sequence load, the data might not be
                // processed yet, need to do it
                int msTime = 1000.0 * channelOutputFrame / RefreshRate;
                if (!sequence->IsSequenceRunning()) {
                    msTime = mediaElapsedSeconds * 1000;
                }
                sequence->ProcessSequenceData(msTime);
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

        sendTime = GetMonotonicTimeMicros();

        if (sequence->IsSequenceRunning() || (onceMore >= 1)) {
            if (sequence->IsSequenceRunning()) {
                // Taken in one step: media sync sets it from another thread,
                // and a skip it set between a read and a reset was lost.
                if (int skip = FrameSkip.exchange(0)) {
                    sequence->SeekSequenceFile(channelOutputFrame + skip + 1);
                }
            }
            sequence->ReadSequenceData();
        }
        readTime = GetMonotonicTimeMicros();

        int msTime = 1000.0 * channelOutputFrame / RefreshRate;
        if (!sequence->IsSequenceRunning()) {
            msTime = mediaElapsedSeconds * 1000;
        }
        if (!sequence->hasBridgeData()) {
            // if bridging, we'll have to process later
            sequence->ProcessSequenceData(msTime);
        }
        processTime = GetMonotonicTimeMicros();

        long long totalTime = processTime - startTime;
        if (totalTime > 150000) {
            // very slow, log immediately (this was 3, which the "> 3" test
            // below didn't pass, so it waited for the next slow frame)
            slowFrameCount = 4;
        } else if (totalTime > 50000) {
            // could be a very transient blip, we'll log if
            // it happens 3 frames in a row
            slowFrameCount++;
        } else {
            slowFrameCount = 0;
        }
        if (slowFrameCount > 3) {
            // Once slow, every frame is slow: logging each one wrote a line
            // per frame to the SD card for as long as it lasted.  Log when it
            // starts and then every 10 s, counting the slow frames between.
            static long long lastSlowLogUS = 0;
            static int slowFramesSinceLog = 0;
            slowFramesSinceLog++;
            if (!lastSlowLogUS || processTime - lastSlowLogUS >= 10000000) {
                LogWarn(VB_CHANNELOUT,
                        "SLOW Output Thread: Loop: %dus, Send: %lldus, Read: %lldus, Process: %lldus, FrameNum: %ld (%d slow frames%s)\n",
                        LightDelay.load(),
                        sendTime - startTime,
                        readTime - sendTime,
                        processTime - readTime,
                        channelOutputFrame,
                        slowFramesSinceLog, lastSlowLogUS ? " in the last 10 s" : "");
                lastSlowLogUS = processTime;
                slowFramesSinceLog = 0;
            }
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
                         LightDelay.load(),
                         sendTime - startTime,
                         readTime - sendTime,
                         processTime - readTime,
                         sleepTime, channelOutputFrame);
            }
        } else {
            LightDelay = BridgeLightDelay.load();
            if (onceMore) {
                onceMore--;
            } else {
                // we will wait up to 2500ms to see if the thread is still needed
                ThreadIsExiting = 1;
                // On a start request, counted under statusLock, not on any
                // notify: StartChannelOutputThread() used to notify without
                // the lock, and a notify that landed before this wait began
                // was lost - the start then spawned a replacement and this
                // thread's timeout below cleared RunThread under it.
                const uint64_t seenStarts = outputStartRequests;
                if (outputThreadSatusCond.wait_for(statusLock, std::chrono::milliseconds(2500),
                                                   [seenStarts, forceSeen]() {
                                                       return outputStartRequests != seenStarts ||
                                                              outputForceRequests.load() != forceSeen;
                                                   })) {
                    // signal to keep going
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
        // Frames are due on a fixed cadence: each one LightDelay after the
        // previous one was due, not after this pass happened to start. Waking
        // is always a little late, and measuring from the wake-up added that
        // to every frame: on a Pi 5 the lights ran 125 ppm slow, and media
        // sync, which only acts on whole frames, let them fall behind the
        // sound by tens of ms over a song before correcting.
        // The period is LightDelay of media time (see SetMediaClockRate()),
        // kept to a fraction of a microsecond so 100 fps doesn't round it off.
        double period = LightDelay / CurrentMediaClockRate(startTime);
        if (nextFrameTime == 0 || startTime >= nextFrameTime + LightDelay) {
            // First frame, or more than a frame behind (back from idle, or a
            // stall): start the cadence again here rather than racing to catch up.
            nextFrameTime = startTime + LightDelay;
            frameTimeCarry = 0.0;
        } else if (startTime >= nextFrameTime) {
            frameTimeCarry += period;
            long long whole = (long long)frameTimeCarry;
            frameTimeCarry -= whole;
            nextFrameTime += whole;
        }
        // else: a forced wake ran this pass early; the regular frame stays due
        // when it was.
        long dt = (nextFrameTime - GetMonotonicTimeMicros()) * 1000;
        if (RunThread && dt > 0) {
            if (outputThreadCond.wait_for(lock, std::chrono::nanoseconds(dt),
                                          [&forceSeen]() { return outputForceRequests.load() != forceSeen; })) {
                LogDebug(VB_CHANNELOUT, "Forced output\n");
            }
        }
    }

    // StoppingOutput() fires the OUTPUTS_DISABLED presets, which run arbitrary
    // commands on this thread. It must run while outputThreadLock is still held
    // so a replacement thread's StartingOutput() cannot interleave with it (a
    // disable landing after the new thread's enable would leave outputs dark),
    // but ThreadIsRunning must still be set and statusLock must be released.
    // Previously ThreadIsRunning was cleared first: a preset that reached
    // StartChannelOutputThread() spawned a replacement that blocked on
    // outputThreadLock (held here) while the preset spun on
    // ChannelOutputThreadIsRunning() -- which only the blocked replacement
    // could set -- wedging this thread, the replacement, and then the main
    // loop at the next config reload. With ThreadIsRunning still set, such a
    // preset queues its replacement on outputThreadLock (it runs once this
    // thread releases) and its startup wait returns immediately.
    // ThreadIsExiting is set so a Start during this window falls through to
    // spawn rather than early-returning against a thread already on its way
    // out.
    statusLock.lock();
    ThreadIsExiting = 1;
    statusLock.unlock();
    StoppingOutput();
    statusLock.lock();
    ThreadIsRunning = 0;
    statusLock.unlock();
    lock.unlock();
    statusLock.lock();
    ThreadIsExiting = 0;
    statusLock.unlock();
    outputThreadSatusCond.notify_all();

    LogDebug(VB_CHANNELOUT, "RunChannelOutputThread() completed\n");
}

/*
 * Set the step time
 */
void SetChannelOutputRefreshRate(float rate) {
    // Under the status lock, because the output thread sets LightDelay to the
    // bridging interval, under that lock, when it finds no sequence running.
    // Unlocked, that store could land just after this one when a sequence was
    // starting, and the whole sequence then ran at the bridging interval
    // (50ms by default) instead of its own frame time. StartSequence() marks
    // the sequence running before calling this, so the thread either sees it
    // running or finishes its store first.
    std::unique_lock<std::mutex> statusLock(outputThreadStatusLock);
    RefreshRate = rate;
    SequenceLightDelay = 1000000 / RefreshRate;
    LightDelay = SequenceLightDelay.load();
}
float GetChannelOutputRefreshRate() {
    return RefreshRate;
}

/*
 * Kick off the channel output thread
 */
void StartChannelOutputThread(void) {
    LogDebug(VB_CHANNELOUT, "StartChannelOutputThread()\n");

    if (BridgeLightDelay == 0) {
        alwaysTransmit = getSettingInt("alwaysTransmit");
        BridgeLightDelay = getSettingInt("E131BridgingInterval", 50) * 1000;
        registerSettingsListener("ChannelOutputThread", "alwaysTransmit", [](const std::string& value) {
            alwaysTransmit = getSettingInt("alwaysTransmit");
        });
        registerSettingsListener("ChannelOutputThread", "E131BridgingInterval", [](const std::string& value) {
            BridgeLightDelay = getSettingInt("E131BridgingInterval", 50) * 1000;
        });
    }
    if (getFPPmode() & PLAYER_MODE) {
        int mediaOffsetInt = getSettingInt("mediaOffset");
        if (mediaOffsetInt) {
            mediaOffset = (float)mediaOffsetInt * 0.001;
        } else {
            mediaOffset = 0.0;
        }
        LogDebug(VB_MEDIAOUT, "Using mediaOffset of %.3f\n", mediaOffset);
    }

    {
        std::unique_lock<std::mutex> lock(outputThreadStatusLock);
        ++outputStartRequests;
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
    // The thread is detached immediately (as the pthread version was via
    // pthread_detach) — its lifecycle is coordinated through the
    // RunThread/ThreadIsRunning/ThreadIsExiting flags and condition
    // variables, never by joining.  std::thread reports creation failure
    // by throwing std::system_error rather than returning an errno.
    try {
        std::thread(RunChannelOutputThread).detach();
    } catch (const std::system_error& ex) {
        RunThread = 0;
        LogErr(VB_CHANNELOUT, "ERROR creating channel output thread: %s (code %d)\n",
               ex.what(), ex.code().value());
        return;
    }

    // Wait for thread to start, but bounded (mirrors StopChannelOutputThread's
    // cap): if the new thread cannot come up — e.g. it is queued on
    // outputThreadLock behind an exiting thread that is stuck in a slow
    // OUTPUTS_DISABLED command — spinning here forever would pin the calling
    // thread (which can be an HTTP handler or a command preset) as well.
    int waitLoops = 0;
    while (!ChannelOutputThreadIsRunning() && (waitLoops++ < 1000))
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!ChannelOutputThreadIsRunning())
        LogErr(VB_CHANNELOUT, "Channel output thread did not start within 5 seconds\n");
}

/*
 *
 */
int StopChannelOutputThread(void) {
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

void StartForcingChannelOutput(void) {
    outputForced = outputForced + 1;

    StartChannelOutputThread();
}

void StopForcingChannelOutput(void) {
    outputForced = outputForced - 1;

    if (outputForced < 0)
        outputForced = 0;
}

/*
 * Reset the master frames played position
 */
void ResetMasterPosition(void) {
    MasterFramesPlayed = -1;
}

/*
 * Update the count of frames that the master has played so we can sync to it
 */
void UpdateMasterPosition(int frameNumber) {
    MasterFramesPlayed = frameNumber;
    CalculateNewChannelOutputDelayForFrame(frameNumber);
}

/*
 * Calculate the new sync offset based on the current position reported
 * by the media player.
 */
void CalculateNewChannelOutputDelay(float mediaPosition) {
    static float nextSyncCheck = 0.5;

    float offsetMediaPosition = mediaPosition - mediaOffset;
    mediaElapsedSeconds = offsetMediaPosition < 0.0 ? 0.0 : offsetMediaPosition;

    if (getFPPmode() == REMOTE_MODE || !sequence->IsSequenceRunning())
        return;

    if ((mediaPosition <= nextSyncCheck) &&
        (nextSyncCheck < (mediaPosition + 2.0))) {
        return;
    }

    nextSyncCheck = mediaPosition + (20.0 / RefreshRate);

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
void CalculateNewChannelOutputDelayForFrame(int expectedFramesSent) {
    int diff = channelOutputFrame - expectedFramesSent;
    bool allowFrameSkip = !multiSync->isMultiSyncEnabled();
    if (!allowFrameSkip && diff <= -(RefreshRate / 2)) {
        // A player sending multisync normally only slews LightDelay so the
        // remotes aren't yanked around by frame jumps, but the slew is capped
        // at 15ms per frame.  If the output is persistently slower than the
        // step time, slewing can never catch back up to the media clock and
        // playback drifts out of sync without bound.  Once we are more than
        // half a second behind, jump — the remotes follow the frame numbers
        // in the sync packets, so they jump with us.
        LogWarn(VB_CHANNELOUT, "Sequence is %d frames behind the media position and cannot catch up by slewing; skipping ahead.  Channel output may be too slow for %d fps.\n",
                -diff, (int)RefreshRate);
        WarningHolder::AddWarningTimeout("Sequence playback cannot keep up with the media; skipping frames to stay in sync", 30);
        allowFrameSkip = true;
    }
    if (allowFrameSkip) {
        if (diff < -4) {
            // pretty far behind master, lets just skip forward
            if (diff > -(RefreshRate / 2)) {
                LogDebug(VB_CHANNELOUT, "Skipping a few frames - We are at %d, master is at: %d\n", channelOutputFrame, expectedFramesSent);
                // off, but not super off, we'll skip a few frames, but not too much to try and keep using
                // the frames in the cache and avoid hitting the storage, we'll then have the OS preload
                // the next bunch and we can skip a few more next time
                FrameSkip = 4;
                diff += 4;
            } else {
                LogDebug(VB_CHANNELOUT, "Skipping many frames - We are at %d, master is at: %d\n", channelOutputFrame, expectedFramesSent);
                // more than 1/2 second behind, just jump
                FrameSkip = expectedFramesSent - channelOutputFrame;
                LightDelay = sequence->IsSequenceRunning() ? SequenceLightDelay.load() : BridgeLightDelay.load();
                return;
            }
        } else if (diff > 2) {
            // hold the last frame
            FrameSkip = -1;
            diff--;
        }
    }
    int DefaultLightDelay = sequence->IsSequenceRunning() ? SequenceLightDelay.load() : BridgeLightDelay.load();
    if (diff > 1 || diff < -1) {
        int timerOffset = diff * (DefaultLightDelay / 100);
        int newLightDelay = LightDelay;
        if (channelOutputFrame > expectedFramesSent) {
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
        // Same bound when we are ahead.  The offset added above scales with
        // the frame difference and accumulates on every check, so without it a
        // large lead (a media position that fell back to 0 while the sequence
        // resumed minutes in) grew LightDelay to seconds.  The output thread
        // sleeps for LightDelay, so the lights froze until the media caught
        // up.  At +15ms per frame a lead still closes steadily.
        if ((DefaultLightDelay + 15000) < newLightDelay)
            newLightDelay = DefaultLightDelay + 15000;

        LogDebug(VB_CHANNELOUT, "LightDelay: %d, newLightDelay: %d,   DiffFrames: %d     %d/%d\n",
                 LightDelay.load(), newLightDelay, diff, channelOutputFrame, expectedFramesSent);
        LightDelay = newLightDelay;
    } else if (diff == -1) {
        // for the one frame off cases, keep the existing light delay unless the
        // previous "off" frame was on the other side of default
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
