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

// AudioLevelMonitor -- live signal levels for PipeWire nodes.
//
// Feeds the mixer's meters. One GStreamer pipeline per metered node,
//
//     pipewiresrc target-object=<node>.monitor ! audioconvert
//       ! audio/x-raw,channels=1,rate=8000 ! level ! fakesink
//
// which is how a meter is supposed to be done: the pipeline stays open and the
// level element reports RMS on a timer. Measured on a Pi 4 that is ~3% of one
// core per node. The naive alternative -- sampling a node by starting a short
// capture each refresh -- pays the stream setup cost over and over, and cannot
// produce a usable reading anyway, since a burst short enough to be cheap is
// dominated by the ~250ms the capture takes to start.
//
// Metering is entirely on demand. Callers subscribe with a TTL and re-subscribe
// to keep it alive, so a browser that closes its tab (or crashes) stops costing
// anything within a few seconds without needing to tell us.

#if __has_include(<gst/gst.h>)
#define HAS_AUDIO_LEVEL_MONITOR

#include <gst/gst.h>

#include <atomic>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "fpp-json.h"

class AudioLevelMonitor {
public:
    static AudioLevelMonitor INSTANCE;

    // A node to meter. isSink distinguishes a sink (a group, a member's filter
    // chain, an input bus) from a playback stream (fppd_stream_N): capturing a
    // sink needs stream.capture.sink, and setting it on a stream instead makes
    // pipewiresrc fall back to the default sink -- see StartMeter().
    struct Target {
        std::string name;
        bool isSink = true;
    };

    // Meter exactly these nodes for the next ttlMs. Pipelines for nodes no
    // longer listed are torn down; nodes already running are left alone so a
    // keepalive does not interrupt them.
    void Subscribe(const std::vector<Target>& nodes, int ttlMs);

    // { "<node>": <0-100>, ... } -- empty when nothing is subscribed, which is
    // what makes this free on an idle system.
    Json::Value GetLevels();

    // True while any pipeline is running, so the WebSocket only pays for a
    // faster broadcast while someone is actually watching meters.
    bool Active() const { return m_active.load(); }

    void Shutdown();

private:
    AudioLevelMonitor() = default;
    ~AudioLevelMonitor();

    struct Meter {
        GstElement* pipeline = nullptr;
        GstBus* bus = nullptr;
        // Written by the bus poll, read by GetLevels().
        double rmsDb = -100.0;
        long long updatedMS = 0;
        bool isSink = true;
    };

    void StartMeter(const std::string& node, bool isSink);
    void StopMeter(const std::string& node);
    void PollThread();

    mutable std::mutex m_lock;
    // Guards thread lifecycle only. Deliberately not m_lock: joining the poll
    // thread while holding m_lock would deadlock, because the thread takes
    // m_lock on every pass.
    std::mutex m_threadLock;
    std::map<std::string, Meter> m_meters;
    long long m_expiresMS = 0;
    std::atomic<bool> m_active{ false };
    std::atomic<bool> m_running{ false };
    std::thread m_thread;
};

#endif
