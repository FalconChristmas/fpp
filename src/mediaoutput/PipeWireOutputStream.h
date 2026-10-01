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

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>

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

    // Route `appsink`'s samples into this stream, replacing any previous
    // feed.  Samples from a feed that has since been replaced are dropped, so
    // a track still tearing down can't interleave with the next one.  The
    // appsink must sync (on the system clock): it paces the audio, and this
    // stream forwards each buffer as soon as it arrives.
    void AttachFeed(GstElement* appsink);

    // Time from a buffer entering this stream to it being handed to the
    // graph -- what a position read at the feed has to be backed up by, on
    // top of the card's own queue.
    int64_t LatencyNs();

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
    void PushSilenceLocked(int ms);
    bool CardRunning();
    bool StreamLinked();

    static GstFlowReturn OnFeedSample(GstElement* appsink, gpointer userData);

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
    bool m_linked = false;
    bool m_cardWasRunning = false;     // someone else had the card running when we opened
    uint64_t m_linkedMs = 0;
    uint64_t m_lastLinkCheckMs = 0;

    GstElement* m_pipeline = nullptr;
    GstElement* m_src = nullptr;
    GstCaps* m_caps = nullptr;         // the stream's fixed caps, set at open
    int m_rate = 0;                    // ...and their rate
    bool m_warnedCaps = false;         // logged a feed in the wrong format
    GstElement* m_feed = nullptr;      // appsink currently allowed to feed (no ref held)
    std::string m_cardStatusPath;      // /proc/asound/.../status of the sink's card, "" if none
};

#endif
