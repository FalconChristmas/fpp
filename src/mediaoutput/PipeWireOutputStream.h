#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "GStreamerOut.h"

#ifdef HAS_GSTREAMER

#include <gst/gst.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

// A PipeWire playback stream that outlives the media played through it.
//
// Giving every track its own pipewiresink costs ~700 ms on an AM335x before the
// first sample reaches the card, and almost none of it is the card: opening a
// full-speed USB card directly takes ~25 ms.  The rest is the new stream node
// appearing, WirePlumber linking it (~300 ms of policy on one slow core), the
// group and filter-chain nodes waking and the ALSA node starting.  A sequence
// started alongside the media runs ahead of the sound for all of that.
//
// One stream per slot, opened ahead of need and held open between tracks, pays
// that once.  A track fed into a stream that is already open reaches the graph
// in tens of milliseconds.  Media pipelines feed it through an appsink (see
// AttachFeed()), converted to the stream's one fixed format (CapsFields()).
// While open and idle PipeWire plays silence, which keeps the card running.
//
// The stream presents exactly the node a per-track pipewiresink used to --
// node.name fppd_stream_N, same description, same target and stream
// properties -- so routing, input mixing and the graph view are unaffected.
class PipeWireOutputStream {
public:
    // How long a stream stays open after its last track stops feeding it, and
    // how far ahead of a scheduled playlist it is opened.  Matches how long
    // the channel output thread keeps running after output goes idle.
    static constexpr int LINGER_MS = 5000;

    // The slot's stream, or nullptr for a slot outside 1..MAX_SLOTS or when
    // PipeWire is not the media backend.
    static PipeWireOutputStream* ForSlot(int slot);

    // Open the slot's stream now if it isn't, and keep it open for at least
    // holdMs.  Never blocks.
    static void Prewarm(int slot, int holdMs);

    // Close every stream immediately (fppd shutdown).
    static void ShutdownAll();

    // Keep slot 1's stream open while set (the PipeWireKeepOutputOpen
    // setting, applied live), so media triggered at any moment -- GPIO, a
    // command, the API -- never waits for it to open.  Costs the CPU of a
    // stream that is always running.  Clearing it lets the stream close after
    // the usual linger.
    static void SetKeepOpen(bool keepOpen);

    // The fixed format every stream carries, as caps fields to append to
    // "audio/x-raw" (",format=F32LE,rate=...,channels=2,layout=interleaved").
    // A feed must deliver exactly this: pipewiresink fixes its PipeWire format
    // when the stream connects and never renegotiates it, so a feed in any
    // other format would be reinterpreted, not converted.
    static std::string CapsFields();

    // A track is about to play through this stream.  Opens it (or reopens it
    // on `target`, if it is open on another sink) and keeps it open until
    // Release().  Never blocks.
    void Acquire(const std::string& target);
    // The track stopped.  Its feed is detached and the stream closes after
    // LINGER_MS unless something acquires it again first.
    void Release(GstElement* feed);

    // Block until audio fed now will reach the card, or timeoutMs passes.
    bool WaitReady(int timeoutMs);
    // Block until what earlier feeds (and the silence pushed behind them)
    // queued has played out, or maxMs passes.  A track that starts right
    // behind another must not go to PLAYING before then: its position runs
    // from PLAYING, so audio still queued ahead of it would leave the lights
    // ahead of the sound for the whole track.
    //
    // Then flush the stream, unless nothing was handed to it since the last
    // flush: what that wait can't see -- audio held up by a stall in the
    // graph -- would otherwise sit ahead of every track to come, and every
    // track starts from the same empty queue (see Flush()).
    void WaitDrained(int maxMs);
    // Instead of WaitDrained(), for a track that starts right behind another:
    // carry on the stream's timeline rather than wait for it to drain.
    //
    // A stopped track's feed leaves the stream bridged with silence, laid
    // exactly behind its last sample (see Release()), for a few seconds.  If
    // the timeline is still running and nothing in it can be late -- the
    // feeds have kept up since the last flush, nothing is backed up in front
    // of the stream, and a card is watched for xruns (DisciplineClock()) --
    // this stops the bridge and returns, in *baseTime, the clock time (on
    // Clock()) where the queue ends.  A pipeline given that base time (and
    // no start time) hands its first sample over so that it plays right after
    // the last one queued, and its position then maps onto the sound exactly
    // as the track before's did.  If the queue is about to run out, it is
    // first extended with silence, just long enough for the pipeline to get
    // going.  Otherwise false, and the caller uses WaitDrained().
    //
    // *earlyNs is how far ahead of `baseTime` the pipeline may hand its first
    // sample over: the feed's lead (FeedLeadNs()), or less when less than
    // that is left queued.  A sink can't render before its base time, so the
    // caller runs the pipeline on baseTime - *earlyNs with its feed's
    // ts-offset at *earlyNs - FeedLeadNs() rather than minus the lead, and
    // allows for that in its position and picture.
    bool ContinueTimeline(GstClockTime* baseTime, int64_t* earlyNs);

    // How long until the last sample handed over is due on the clock, or -1
    // if nothing is queued on a timeline.  Add the card's own queue for when
    // it is heard.
    int64_t QueuedAheadNs();
    // The same for slot's stream, if it exists (none is created).
    static int64_t QueuedAheadNsForSlot(int slot);

    // Route `appsink`'s samples into this stream, replacing any previous
    // feed.  Samples from a feed that has since been replaced are dropped, so
    // a track still tearing down can't interleave with the next one.  The
    // appsink must sync (on Clock()): it paces the audio, and this
    // stream forwards each buffer as soon as it arrives.
    void AttachFeed(GstElement* appsink);

    // How far ahead of the clock a feed delivers (see AttachFeed()).
    static int64_t FeedLeadNs();
    // The clock this stream's own pipeline runs on (a new reference).  It runs
    // at the rate the card plays samples, not the system's: see
    // DisciplineClock().
    GstClock* Clock();
    // The clock every pipeline feeding this stream must run on (a new
    // reference): the card clock, run faster or slower by SetFeedSpeed().
    // Until a feed sets a speed it is the card clock exactly.
    GstClock* FeedClock();
    // Play `feed`'s media `speed` times as fast as the card plays samples (1.0
    // is the card's own pace).  A MultiSync remote steers its copy of the
    // show audio onto the master's timeline with this.  The feed clock runs
    // `speed` times as fast, so the feed hands over media at that pace, and
    // each buffer is resampled to the card's rate on the way in: the stream
    // still gets exactly what the card plays, so nothing queues up or runs
    // dry.  A GStreamer rate seek can't do it -- this stream plays at a fixed
    // rate, so a faster feed only builds a queue the position can't see, and
    // mp3 and FLAC refuse instant rate changes anyway (GstBaseParse).  No-op
    // for a feed that isn't the current one; reset for each new feed.
    void SetFeedSpeed(GstElement* feed, double speed);
    // Drop what `feed` has handed over and not yet played, when it seeks:
    // left in place, the sound from before the seek plays out first and
    // everything after it is that much late, by an amount its position can't
    // show.  No-op for a feed that isn't the current one.
    void FlushFeed(GstElement* feed);
    // That rate, relative to the system clock, smoothed over the last couple
    // of minutes (1.000432 for a card 432 ppm fast).  1.0 until measured.
    double ClockRate() const { return m_clockRate.load(std::memory_order_relaxed); }

private:
    explicit PipeWireOutputStream(int slot);

    enum class State { Closed,
                       Opening,
                       Ready,
                       Failed };

    // Worker-thread side.
    static void WorkerLoop();
    static void WakeWorker();
    void Service(uint64_t now);  // takes and may drop/retake m_lock
    bool OpenLocked(std::unique_lock<std::mutex>& lock);
    void CloseLocked(const char* why);
    void ForgetFailureLocked();
    void PushSilenceLocked(int ms);
    // Silence laid on the timeline right behind its end, up to `until`.
    void ExtendTimelineLocked(GstClockTime until);
    // Keep a bridged timeline from running out (see Release()); whether it
    // is still bridging.
    bool ServiceBridge();
    void EndTimelineLocked();
    // A feed buffer starting at `start` (on m_clock) and `frames` long was
    // handed over, `lateNs` after its sink was due to hand it over.
    void NoteTimelineLocked(GstClockTime start, size_t frames, GstClockTimeDiff lateNs);
    bool WaitQueuedUntil(int64_t deadlineUs);
    // What follows a flush.  Between tracks: a few tens of ms of silence,
    // which the reactivated stream drops in place of the next track's head.
    // Mid-feed (an xrun): the feed carries on, its next buffers a lead early,
    // into an empty queue where they would play at once -- silence laid first
    // puts them back where they belong.  After a seek: nothing, as the feed's
    // running time starts again and its first buffers come on time.
    enum class FlushFor { NextTrack,
                          MidFeed,
                          Seek };
    void Flush(FlushFor why);
    bool CardRunning();
    static int TailFlushMs();
    bool StreamLinked();

    static GstFlowReturn OnFeedSample(GstElement* appsink, gpointer userData);
    // Whether the card lost time (an xrun) while a track was feeding the stream.
    bool DisciplineClock();
    void ResetClockSyncLocked();
    void EnsureClocksLocked();
    // Re-derive the feed clock from the card clock and m_feedSpeed, continuous
    // at `internal` (called whenever either changes).
    void RecalibrateFeedClockLocked(GstClockTime internal);
    // `in` resampled by m_feedSpeed (a new buffer), carrying m_rs* across calls.
    GstBuffer* ResampleLocked(GstBuffer* in);
    void KeepResampleHistoryLocked(GstBuffer* in);

    const int m_slot;
    std::mutex m_lock;
    std::condition_variable m_stateChanged;

    State m_state = State::Closed;
    std::string m_target;         // sink the stream is (or is being) linked to
    std::string m_wantTarget;     // sink the next Acquire/Prewarm asked for
    bool m_wantOpen = false;
    int m_users = 0;
    uint64_t m_holdUntilMs = 0;
    uint64_t m_openStartedMs = 0;
    uint64_t m_failedMs = 0;           // when the stream last failed (keep-open retry)
    bool m_linked = false;
    bool m_cardWasRunning = false;     // someone else had the card running when we opened
    uint64_t m_linkedMs = 0;
    uint64_t m_lastLinkCheckMs = 0;

    GstElement* m_pipeline = nullptr;
    GstElement* m_src = nullptr;
    GstCaps* m_caps = nullptr;         // the stream's fixed caps, set at open
    // When (CLOCK_MONOTONIC, us) everything handed to the stream so far will
    // have played, assuming the graph takes it at real time once queued.
    int64_t m_queuedUntilUs = 0;
    void NoteQueuedLocked(size_t bytes);
    // Nothing has been handed to the stream since it was last flushed.
    bool m_flushed = false;
    // The stream's timeline (ContinueTimeline()).  Valid from the first feed
    // buffer after a flush or an open for as long as what is handed over
    // stays contiguous: m_timelineEnd is the time on m_clock at which the last
    // sample handed over is due, in the terms a feed's position maps by.
    bool m_timelineValid = false;
    GstClockTime m_timelineEnd = 0;
    // Since the last flush a feed ran dry -- a buffer came after the one
    // before it had played -- so the sound may be behind its position by an
    // amount nothing here can see.  Carried on, the next track would inherit
    // it; the next track is flushed instead, as every track used to be.
    bool m_timelineLate = false;
    // No feed is attached and the timeline is kept running with silence
    // until m_bridgeUntilMs (Release()).
    bool m_bridging = false;
    uint64_t m_bridgeUntilMs = 0;
    // When the last track was released, and how much silence the bridge has
    // laid since (logged when the next track carries on).
    int64_t m_releasedUs = 0;
    GstClockTime m_bridgedNs = 0;
    // The feed attached by ContinueTimeline() has handed nothing over yet.
    // If its first buffer comes too late for the queue, the stream is flushed
    // at once (the worker, m_flushNow): the track's head skips, but it plays
    // with its position rather than behind it.
    bool m_continuedFirst = false;
    bool m_flushNow = false;
    int m_rate = 0;                    // ...and their rate
    bool m_warnedCaps = false;         // logged a feed in the wrong format
    GstElement* m_feed = nullptr;      // appsink currently allowed to feed (no ref held)
    std::string m_cardStatusPath;      // /proc/asound/.../status of the sink's card, "" if none

    // The card clock (DisciplineClock()).  Created on first use, never freed.
    GstClock* m_clock = nullptr;
    // The feed clock (FeedClock()), and the speed it runs at against the card
    // clock.  Both are under m_lock.
    GstClock* m_feedClock = nullptr;
    double m_feedSpeed = 1.0;
    // The feed clock has run at a speed other than 1 since the feed attached,
    // so it no longer reads what the card clock does.
    bool m_feedDiverged = false;
    // Resampling the feed (SetFeedSpeed()): on from the first speed other than
    // 1 to the end of the feed.  m_rsPos is where the next output frame falls,
    // in input frames from the start of the next buffer; m_rsHist the last
    // fifteen input frames, the resampler's filter span less one (kept even
    // while not resampling, so starting is seamless), oldest first,
    // m_rsHistFrames of them valid.
    bool m_rsActive = false;
    double m_rsPos = 0.0;
    float m_rsHist[15 * 2] = {};
    int m_rsHistFrames = 0;
    std::atomic<double> m_clockRate{ 1.0 };
    std::string m_syncPath;            // the card the fields below describe
    int m_syncCardRate = 0;            // its nominal rate
    bool m_syncAnchored = false;
    int64_t m_syncHw0 = 0;             // hw_ptr at the anchor...
    GstClockTime m_syncInt0 = 0;       // ...the clock's internal time then...
    GstClockTime m_syncExt0 = 0;       // ...and its own time then
    int64_t m_syncLastHw = 0;          // hw_ptr as last read
    int64_t m_syncWrap = 0;            // added to hw_ptr for each time it wrapped
    std::string m_syncTrigger;         // the card's trigger_time as last read
    // The card was seen stopped under a feed (an xrun): flush once it runs
    // again (see DisciplineClock()).
    bool m_xrunPending = false;
    uint64_t m_lastFlushMs = 0;        // NowMs() of the last Flush() (under m_lock)
    GstClockTime m_syncLastInt = 0;
    // (seconds of internal time, seconds of card time) since the anchor.
    std::deque<std::pair<double, double>> m_syncWindow;
};

#endif
