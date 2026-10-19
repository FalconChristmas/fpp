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

    // Route `appsink`'s samples into this stream, replacing any previous
    // feed.  Samples from a feed that has since been replaced are dropped, so
    // a track still tearing down can't interleave with the next one.  The
    // appsink must sync (on Clock()): it paces the audio, and this
    // stream forwards each buffer as soon as it arrives.
    void AttachFeed(GstElement* appsink);

    // How far ahead of the clock a feed delivers (see AttachFeed()).
    static int64_t FeedLeadNs();
    // The clock every pipeline feeding this stream must run on (a new
    // reference).  It runs at the rate the card plays samples, not the
    // system's: see DisciplineClock().
    GstClock* Clock();
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
    bool WaitQueuedUntil(int64_t deadlineUs);
    void Flush(bool prime);
    bool CardRunning();
    static int TailFlushMs();
    bool StreamLinked();

    static GstFlowReturn OnFeedSample(GstElement* appsink, gpointer userData);
    // Whether the card lost time (an xrun) while a track was feeding the stream.
    bool DisciplineClock();
    void ResetClockSyncLocked();

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
    int m_rate = 0;                    // ...and their rate
    bool m_warnedCaps = false;         // logged a feed in the wrong format
    GstElement* m_feed = nullptr;      // appsink currently allowed to feed (no ref held)
    std::string m_cardStatusPath;      // /proc/asound/.../status of the sink's card, "" if none

    // The card clock (DisciplineClock()).  Created on first use, never freed.
    GstClock* m_clock = nullptr;
    std::atomic<double> m_clockRate{ 1.0 };
    std::string m_syncPath;            // the card the fields below describe
    int m_syncCardRate = 0;            // its nominal rate
    bool m_syncAnchored = false;
    int64_t m_syncHw0 = 0;             // hw_ptr at the anchor...
    GstClockTime m_syncInt0 = 0;       // ...the clock's internal time then...
    GstClockTime m_syncExt0 = 0;       // ...and its own time then
    int64_t m_syncLastHw = 0;
    GstClockTime m_syncLastInt = 0;
    // (seconds of internal time, seconds of card time) since the anchor.
    std::deque<std::pair<double, double>> m_syncWindow;
};

#endif
