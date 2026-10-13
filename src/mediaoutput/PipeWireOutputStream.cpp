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

#include "fpp-pch.h"

#include "PipeWireOutputStream.h"

#ifdef HAS_GSTREAMER

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <thread>

#include "common.h"
#include "common_mini.h"
#include "log.h"
#include "settings.h"
#include "StreamSlotManager.h"

namespace {
// Every stream carries F32 stereo at the graph's own rate, so PipeWire never
// resamples it and a feed only converts what differs.
constexpr int kChannels = 2;
constexpr int kBytesPerFrame = 4 * kChannels;

// Time from a buffer handed to the stream to it being written to the card,
// on top of the card's own queue: pipewiresink queues it for the graph's next
// cycle, so one graph quantum (FPP's default is 1024 frames).
constexpr int64_t kGraphQuantum = 1024;

// Longest a stream may take to come up before it is abandoned.  A cold open is
// ~0.6 s on an AM335x; this only trips when PipeWire itself is unwell.
constexpr int kOpenTimeoutMs = 8000;
// How often an opening stream's links are checked (each check runs pw-link).
constexpr int kLinkCheckIntervalMs = 50;
// With no card to watch (a group of network sinks), how long the stream must
// have been linked before it is trusted to be on the real driver.
constexpr int kNoCardSettleMs = 150;
// With the stream kept open, how long after a failure to try again.  A
// PipeWire restart under a running fppd leaves GStreamer's PipeWire connection
// dead until fppd restarts, so retrying hard would only fill the log.
constexpr int kKeepOpenRetryMs = 30000;

// The card clock (see DisciplineClock()).  How often the card's position is
// read, how much of its history the rate is fitted over, how fast a phase
// error is closed, and how far the clock may be pushed off the system's.
constexpr int kClockSampleMs = 250;
constexpr double kClockWindowS = 120.0;
constexpr double kClockMinFitS = 4.0;
constexpr double kClockPhaseGain = 1.0 / 20.0;   // per second of error, per second
constexpr double kClockMaxPhaseSlew = 500e-6;
constexpr double kClockMaxOffset = 2000e-6;
// A jump this big in the card's position is a restart or an xrun, not drift:
// start again from it rather than chase it.
constexpr double kClockMaxPhaseError = 0.050;

std::mutex s_slotsLock;
std::array<PipeWireOutputStream*, StreamSlotManager::MAX_SLOTS + 1> s_slots{};

std::mutex s_workerLock;
std::condition_variable s_workerWake;
std::thread s_worker;
bool s_workerRunning = false;
// Set by ShutdownAll() and never cleared: a ForSlot() racing it (a prewarm, a
// settings listener) must not start a worker again, let alone assign a new
// std::thread over the one ShutdownAll() is joining.
bool s_workerShutDown = false;
bool s_workerKick = false;

std::atomic<bool> s_keepOpen{ false };

uint64_t NowMs() {
    return (uint64_t)GetTimeMS();
}
}

PipeWireOutputStream::PipeWireOutputStream(int slot) :
    m_slot(slot) {
}

PipeWireOutputStream* PipeWireOutputStream::ForSlot(int slot) {
    if (slot < 1 || slot > StreamSlotManager::MAX_SLOTS || !isPipeWireBackend()) {
        return nullptr;
    }
    std::unique_lock<std::mutex> lock(s_slotsLock);
    if (!s_slots[slot]) {
        s_slots[slot] = new PipeWireOutputStream(slot);
    }
    {
        std::unique_lock<std::mutex> wlock(s_workerLock);
        if (!s_workerRunning && !s_workerShutDown) {
            s_workerRunning = true;
            s_worker = std::thread(WorkerLoop);
        }
    }
    return s_slots[slot];
}

void PipeWireOutputStream::Prewarm(int slot, int holdMs) {
    PipeWireOutputStream* s = ForSlot(slot);
    if (!s) {
        return;
    }
    std::string target = GStreamerOutput::PipeWireSinkNameForSlot(slot);
    {
        std::unique_lock<std::mutex> lock(s->m_lock);
        s->m_wantTarget = target;
        s->m_holdUntilMs = std::max(s->m_holdUntilMs, NowMs() + (uint64_t)std::max(holdMs, 0));
        s->m_wantOpen = true;
        s->ForgetFailureLocked();
    }
    WakeWorker();
}

void PipeWireOutputStream::SetKeepOpen(bool keepOpen) {
    if (s_keepOpen.exchange(keepOpen) == keepOpen) {
        return;
    }
    if (keepOpen) {
        LogInfo(VB_MEDIAOUT, "PipeWire output stream 1 will be kept open\n");
        Prewarm(1, 0);
    } else {
        // Falls back to the usual linger; the worker closes it once that
        // and any playing track are done.
        LogInfo(VB_MEDIAOUT, "PipeWire output stream 1 no longer kept open\n");
        if (PipeWireOutputStream* s = ForSlot(1)) {
            std::unique_lock<std::mutex> lock(s->m_lock);
            s->m_holdUntilMs = std::max(s->m_holdUntilMs, NowMs() + LINGER_MS);
        }
        WakeWorker();
    }
}

std::string PipeWireOutputStream::CapsFields() {
    return ",format=F32LE,rate=" + std::to_string(GStreamerOutput::PipeWireGraphRate()) +
           ",channels=" + std::to_string(kChannels) + ",layout=interleaved";
}

void PipeWireOutputStream::ShutdownAll() {
    std::thread worker;
    {
        std::unique_lock<std::mutex> wlock(s_workerLock);
        s_workerShutDown = true;
        if (!s_workerRunning) {
            return;
        }
        s_workerRunning = false;
        worker = std::move(s_worker);
    }
    s_workerWake.notify_all();
    if (worker.joinable()) {
        worker.join();
    }
    std::unique_lock<std::mutex> lock(s_slotsLock);
    for (PipeWireOutputStream* s : s_slots) {
        if (s) {
            std::unique_lock<std::mutex> slock(s->m_lock);
            s->m_users = 0;
            s->m_holdUntilMs = 0;
            s->CloseLocked("fppd shutting down");
        }
    }
}

void PipeWireOutputStream::Acquire(const std::string& target) {
    {
        std::unique_lock<std::mutex> lock(m_lock);
        m_users++;
        m_wantTarget = target;
        m_wantOpen = true;
        ForgetFailureLocked();
    }
    WakeWorker();
}

// A failure belongs to the attempt that made it.  Left in place, the next
// track's WaitReady() returned on it at once while the worker was already
// opening the stream again, and the head of that track went nowhere.
void PipeWireOutputStream::ForgetFailureLocked() {
    if (m_state == State::Failed) {
        m_state = State::Closed;
    }
}

void PipeWireOutputStream::Release(GstElement* feed) {
    {
        std::unique_lock<std::mutex> lock(m_lock);
        if (feed && m_feed == feed) {
            m_feed = nullptr;
            // Push the track's last fragment out behind some silence.  PipeWire
            // holds a partial cycle until there is more data, so with nothing
            // behind it the tail of a stopped track sat in the stream and played
            // at the head of the next one.  (Flushing it instead deactivates and
            // reactivates the stream, and pipewiresink drops whatever arrives
            // before that settles -- the next track's first ~40 ms.)
            PushSilenceLocked(TailFlushMs());
        }
        if (m_users > 0) {
            m_users--;
        }
        m_holdUntilMs = std::max(m_holdUntilMs, NowMs() + LINGER_MS);
    }
    WakeWorker();
}

static int64_t MonoUs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

void PipeWireOutputStream::NoteQueuedLocked(size_t bytes) {
    if (m_rate <= 0) {
        return;
    }
    int64_t now = MonoUs();
    int64_t durUs = (int64_t)(bytes / kBytesPerFrame) * 1000000LL / m_rate;
    m_queuedUntilUs = std::max(m_queuedUntilUs, now) + durUs;
}

void PipeWireOutputStream::WaitDrained(int maxMs) {
    int64_t deadline = MonoUs() + (int64_t)maxMs * 1000;
    while (true) {
        int64_t remaining;
        {
            std::unique_lock<std::mutex> lock(m_lock);
            remaining = m_queuedUntilUs - MonoUs();
        }
        if (remaining <= 0 || MonoUs() >= deadline) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(std::min<int64_t>(remaining, 5000)));
    }
}

bool PipeWireOutputStream::WaitReady(int timeoutMs) {
    // Ready only counts on the target asked for: a stream still Ready on the
    // previous sink is about to be closed and reopened by the worker.
    std::unique_lock<std::mutex> lock(m_lock);
    auto readyHere = [this] { return m_state == State::Ready && m_target == m_wantTarget; };
    m_stateChanged.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, &readyHere] {
        return readyHere() || m_state == State::Failed ||
               (m_state == State::Closed && !m_wantOpen);
    });
    return readyHere();
}

int64_t PipeWireOutputStream::LatencyNs() {
    return kGraphQuantum * 1000000000LL / GStreamerOutput::PipeWireGraphRate();
}

int PipeWireOutputStream::TailFlushMs() {
    // Three graph cycles: more than any partial cycle plus the feed's lead.
    return (int)(3 * kGraphQuantum * 1000 / GStreamerOutput::PipeWireGraphRate()) + 1;
}

int64_t PipeWireOutputStream::FeedLeadNs() {
    return kGraphQuantum * 1000000000LL / GStreamerOutput::PipeWireGraphRate();
}

void PipeWireOutputStream::AttachFeed(GstElement* appsink) {
    {
        std::unique_lock<std::mutex> lock(m_lock);
        m_feed = appsink;
    }
    // Run the feed one graph cycle ahead of the clock.  Handed over exactly
    // on time, a buffer can miss the cycle that needed it by a hair; PipeWire
    // then plays a quantum of silence there and the rest of the track is a
    // quantum late.  A cycle's worth queued absorbs that.
    g_object_set(appsink, "emit-signals", TRUE, "ts-offset", -FeedLeadNs(), NULL);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(OnFeedSample), this);
}

// Runs on the feeding pipeline's streaming thread.  `this` is a per-slot
// singleton that is never freed, so a feed outliving its GStreamerOutput (async
// teardown) can still land here safely; it is simply no longer m_feed.
GstFlowReturn PipeWireOutputStream::OnFeedSample(GstElement* appsink, gpointer userData) {
    PipeWireOutputStream* self = static_cast<PipeWireOutputStream*>(userData);
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
    if (!sample) {
        return GST_FLOW_OK;
    }
    GstBuffer* in = gst_sample_get_buffer(sample);
    const GstSegment* segment = gst_sample_get_segment(sample);
    GstCaps* caps = gst_sample_get_caps(sample);

    GstElement* src = nullptr;
    GstBuffer* out = nullptr;
    {
        std::unique_lock<std::mutex> lock(self->m_lock);
        bool ours = in && caps && self->m_src && self->m_feed == appsink;
        if (ours && self->m_caps && !gst_caps_can_intersect(self->m_caps, caps)) {
            // Can't happen unless the graph rate moved under a running fppd;
            // playing it would be noise, so drop it and say so once.
            if (!self->m_warnedCaps) {
                self->m_warnedCaps = true;
                gchar* got = gst_caps_to_string(caps);
                gchar* want = gst_caps_to_string(self->m_caps);
                LogErr(VB_MEDIAOUT, "PipeWire output stream %d: feed is %s, stream is %s -- dropping it\n",
                       self->m_slot, got, want);
                g_free(got);
                g_free(want);
            }
            ours = false;
        }
        if (ours) {
            // Both pipelines run on the card clock, so a buffer's running
            // time in the feeding pipeline maps onto ours through the two base
            // times.  The sink doesn't sync on it (the feed already paced the
            // buffer); it is carried for PipeWire's own bookkeeping.
            out = gst_buffer_copy(in);
            GstClockTime rt = gst_segment_to_running_time(segment, GST_FORMAT_TIME, GST_BUFFER_PTS(in));
            if (GST_CLOCK_TIME_IS_VALID(rt)) {
                GstClockTime abs = gst_element_get_base_time(appsink) + rt;
                GstClockTime ourBase = gst_element_get_base_time(self->m_pipeline);
                GST_BUFFER_PTS(out) = abs > ourBase ? abs - ourBase : 0;
            } else {
                GST_BUFFER_PTS(out) = GST_CLOCK_TIME_NONE;
            }
            GST_BUFFER_DTS(out) = GST_CLOCK_TIME_NONE;
            self->NoteQueuedLocked(gst_buffer_get_size(out));
            src = GST_ELEMENT(gst_object_ref(self->m_src));
        }
    }
    if (src) {
        // Outside the lock: the push can take the sink's own locks.
        gst_app_src_push_buffer(GST_APP_SRC(src), out);
        gst_object_unref(src);
    }
    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

void PipeWireOutputStream::WakeWorker() {
    {
        std::unique_lock<std::mutex> wlock(s_workerLock);
        s_workerKick = true;
    }
    s_workerWake.notify_all();
}

void PipeWireOutputStream::WorkerLoop() {
    SetThreadName("FPP-PWStream");
    while (true) {
        // Slots are created on demand but never freed, so a snapshot of the
        // table is safe to service without holding s_slotsLock -- which
        // ForSlot() callers on the main loop would otherwise wait on while a
        // stream connects to the daemon.
        std::array<PipeWireOutputStream*, StreamSlotManager::MAX_SLOTS + 1> slots;
        {
            std::unique_lock<std::mutex> lock(s_slotsLock);
            slots = s_slots;
        }
        bool anyOpening = false;
        uint64_t now = NowMs();
        for (PipeWireOutputStream* s : slots) {
            if (s) {
                s->DisciplineClock();
                s->Service(now);
                std::unique_lock<std::mutex> slock(s->m_lock);
                anyOpening |= (s->m_state == State::Opening);
            }
        }
        std::unique_lock<std::mutex> wlock(s_workerLock);
        if (!s_workerRunning) {
            break;
        }
        // Opening streams are polled every 10 ms; otherwise the only thing to
        // watch for is a linger running out.
        s_workerWake.wait_for(wlock, std::chrono::milliseconds(anyOpening ? 10 : 250),
                              [] { return s_workerKick || !s_workerRunning; });
        s_workerKick = false;
        if (!s_workerRunning) {
            break;
        }
    }
}

void PipeWireOutputStream::Service(uint64_t now) {
    std::unique_lock<std::mutex> lock(m_lock);

    bool keepOpen = s_keepOpen && m_slot == 1;
    if (keepOpen && !m_wantOpen && (m_state == State::Closed || m_state == State::Failed) &&
        now - m_failedMs >= (uint64_t)kKeepOpenRetryMs) {
        if (m_wantTarget.empty()) {
            m_wantTarget = GStreamerOutput::PipeWireSinkNameForSlot(m_slot);
        }
        m_wantOpen = true;
    }
    bool wanted = keepOpen || m_users > 0 || now < m_holdUntilMs;
    if (!wanted) {
        m_wantOpen = false;
        if (m_state != State::Closed) {
            CloseLocked("idle");
        }
        return;
    }

    if (m_pipeline) {
        // A bus error means the stream is dead (PipeWire restarted, sink gone).
        // Drop it; the next track or prewarm opens a fresh one.
        GstBus* bus = gst_element_get_bus(m_pipeline);
        while (GstMessage* msg = gst_bus_pop_filtered(bus, GST_MESSAGE_ERROR)) {
            GError* err = nullptr;
            gst_message_parse_error(msg, &err, nullptr);
            LogWarn(VB_MEDIAOUT, "PipeWire output stream %d: %s\n", m_slot, err ? err->message : "error");
            if (err) {
                g_error_free(err);
            }
            gst_message_unref(msg);
            m_wantOpen = false;
            CloseLocked("stream error");
            m_state = State::Failed;
            m_failedMs = now;
            m_stateChanged.notify_all();
        }
        gst_object_unref(bus);
    }

    if (m_state != State::Closed && m_state != State::Failed && m_wantTarget != m_target) {
        LogInfo(VB_MEDIAOUT, "PipeWire output stream %d: target changed '%s' -> '%s', reopening\n",
                m_slot, m_target.c_str(), m_wantTarget.c_str());
        CloseLocked("target changed");
        m_wantOpen = true;
    }

    if ((m_state == State::Closed || m_state == State::Failed) && m_wantOpen) {
        if (!OpenLocked(lock)) {
            m_wantOpen = false;
            m_state = State::Failed;
            m_failedMs = NowMs();
            m_stateChanged.notify_all();
        }
        return;
    }

    if (m_state != State::Opening) {
        return;
    }

    // Opening.  Two things have to be true, and neither implies the other.
    // Our stream must be linked into the graph -- until it is, pipewiresink
    // drops whatever it is given.  And the card must be RUNNING: PipeWire runs
    // a newly linked chain on a placeholder driver for ~100 ms while the ALSA
    // node is still opening, and audio fed then is consumed into nothing.
    //
    // If the card was idle when we opened, it starting is the proof of both:
    // nothing else woke it.  If something else (AES67, an input mix) already
    // had it running, that proves nothing about us, so ask pw-link -- as for a
    // sink that reaches no card at all (a group of network senders only),
    // where there is nothing to watch but the link.
    //
    // (The stream's GstPipeWireClock would be the natural thing to watch, but
    // it does not advance at all for a follower of an ALSA-driven graph --
    // pw_stream_get_time_n() reports a zero rate and the clock returns its last
    // value -- so nothing here runs on it.)
    bool ready = false;
    //
    // No settling time after the card starts is needed: a cold-start sweep on
    // an AM335x with a USB card, checking what actually crossed the bus to the
    // card, kept the head of the track intact with none, and 25-400 ms bought
    // nothing.
    if (!m_cardStatusPath.empty() && !m_cardWasRunning) {
        ready = CardRunning();
    } else {
        if (!m_linked && NowMs() - m_lastLinkCheckMs >= (uint64_t)kLinkCheckIntervalMs) {
            // pw-link talks to the daemon; don't hold the lock the feed and
            // Acquire() need while it does.
            GstElement* pipeline = GST_ELEMENT(gst_object_ref(m_pipeline));
            lock.unlock();
            bool linked = StreamLinked();
            lock.lock();
            bool same = (m_pipeline == pipeline);
            gst_object_unref(pipeline);
            if (!same || m_state != State::Opening) {
                return;
            }
            m_lastLinkCheckMs = NowMs();
            if (linked) {
                m_linked = true;
                m_linkedMs = NowMs();
            }
        }
        ready = m_linked && NowMs() - m_linkedMs >= (uint64_t)kNoCardSettleMs &&
                (m_cardStatusPath.empty() || CardRunning());
    }
    now = NowMs();
    if (ready) {
        m_state = State::Ready;
        LogInfo(VB_MEDIAOUT, "PipeWire output stream %d ready on '%s' after %d ms\n",
                m_slot, m_target.c_str(), (int)(now - m_openStartedMs));
        m_stateChanged.notify_all();
    } else if (now - m_openStartedMs > (uint64_t)kOpenTimeoutMs) {
        LogWarn(VB_MEDIAOUT, "PipeWire output stream %d: not running after %d ms (linked %s, card %s), giving up\n",
                m_slot, kOpenTimeoutMs, m_linked ? "yes" : "no",
                m_cardStatusPath.empty() ? "n/a" : (CardRunning() ? "running" : "not running"));
        m_wantOpen = false;
        CloseLocked("open timed out");
        m_state = State::Failed;
        m_failedMs = now;
        m_stateChanged.notify_all();
    }
}

bool PipeWireOutputStream::OpenLocked(std::unique_lock<std::mutex>& lock) {
    std::string target = m_wantTarget;

    // Nothing below touches this object's state until the pipeline is
    // running, and all of it can be slow -- gst_init() on first use (~20 s
    // the first time a box ever runs GStreamer), and pipewiresink connecting
    // to the daemon -- so none of it runs under the slot lock, which the feed
    // callback and Acquire() on the playlist thread both need.
    lock.unlock();
    GStreamerOutput::EnsureGStreamerInit();
    int rate = GStreamerOutput::PipeWireGraphRate();
    std::string capsStr = "audio/x-raw" + CapsFields();
    std::string launch = std::string("appsrc name=src is-live=true format=time do-timestamp=false block=false "
                                     "max-time=1000000000 leaky-type=downstream caps=\"") +
                         capsStr + "\" ! pipewiresink name=pwsink sync=false";
    if (!target.empty()) {
        launch += " target-object=" + target;
    }
    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(launch.c_str(), &error);
    if (error || !pipeline) {
        LogErr(VB_MEDIAOUT, "PipeWire output stream %d: could not build: %s\n", m_slot,
               error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        if (pipeline) {
            gst_object_unref(pipeline);
        }
        lock.lock();
        return false;
    }
    // The identity every per-track pipewiresink used to present, so anything
    // that matches or routes fppd_stream_N sees no difference.
    if (GstElement* pwsink = gst_bin_get_by_name(GST_BIN(pipeline), "pwsink")) {
        std::string nodeName = StreamSlotManager::GetNodeName(m_slot);
        std::string nodeDesc = StreamSlotManager::GetNodeDescription(m_slot);
        GstStructure* props = gst_structure_new("props",
                                                "node.name", G_TYPE_STRING, nodeName.c_str(),
                                                "node.description", G_TYPE_STRING, nodeDesc.c_str(),
                                                "stream.dont-remix", G_TYPE_BOOLEAN, TRUE,
                                                "channelmix.disable", G_TYPE_BOOLEAN, TRUE,
                                                NULL);
        g_object_set(pwsink, "stream-properties", props, NULL);
        gst_structure_free(props);
        gst_object_unref(pwsink);
    }
    GstElement* src = gst_bin_get_by_name(GST_BIN(pipeline), "src");
    std::string cardStatus = GStreamerOutput::AlsaSinkStatusPath(target);
    bool cardWasRunning = !cardStatus.empty() &&
                          GetFileContents(cardStatus).find("state: RUNNING") != std::string::npos;

    // The card clock, like every pipeline feeding this one, so that the base
    // times OnFeedSample() maps between are on one clock (see the note in
    // Service() on why not pipewiresink's own).
    GstClock* clock = Clock();
    gst_pipeline_use_clock(GST_PIPELINE(pipeline), clock);
    gst_object_unref(clock);

    uint64_t started = NowMs();
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    lock.lock();

    if (ret == GST_STATE_CHANGE_FAILURE) {
        LogErr(VB_MEDIAOUT, "PipeWire output stream %d: could not start on '%s'\n", m_slot, target.c_str());
        gst_object_unref(src);
        std::thread([pipeline]() {
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
        }).detach();
        return false;
    }

    m_pipeline = pipeline;
    m_src = src;
    GstCaps* fixedCaps = gst_caps_from_string(capsStr.c_str());
    gst_caps_replace(&m_caps, fixedCaps);
    gst_caps_unref(fixedCaps);
    m_rate = rate;
    m_warnedCaps = false;
    m_queuedUntilUs = 0;
    m_target = target;
    m_cardStatusPath = cardStatus;
    m_cardWasRunning = cardWasRunning;
    m_openStartedMs = started;
    m_linked = false;
    m_linkedMs = 0;
    m_lastLinkCheckMs = 0;
    m_state = State::Opening;
    LogInfo(VB_MEDIAOUT, "PipeWire output stream %d opening on '%s' (card status %s)\n", m_slot,
            target.empty() ? "(default)" : target.c_str(), cardStatus.empty() ? "n/a" : cardStatus.c_str());
    // pipewiresink only connects its stream once it has caps, which arrive
    // with the first buffer.
    PushSilenceLocked(20);
    return true;
}

void PipeWireOutputStream::PushSilenceLocked(int ms) {
    if (!m_src || m_rate <= 0) {
        return;
    }
    size_t frames = (size_t)m_rate * ms / 1000;
    GstBuffer* buf = gst_buffer_new_and_alloc(frames * kBytesPerFrame);
    gst_buffer_memset(buf, 0, 0, frames * kBytesPerFrame);
    GstClockTime now = gst_clock_get_time(m_clock);
    GstClockTime base = gst_element_get_base_time(m_pipeline);
    GST_BUFFER_PTS(buf) = now > base ? now - base : 0;
    GST_BUFFER_DURATION(buf) = gst_util_uint64_scale(frames, GST_SECOND, m_rate);
    NoteQueuedLocked(frames * kBytesPerFrame);
    gst_app_src_push_buffer(GST_APP_SRC(m_src), buf);
}

// Whether this slot's stream has an output port linked to anything, per
// pw-link.  Only run while a stream is opening, every kLinkCheckIntervalMs.
bool PipeWireOutputStream::StreamLinked() {
    std::string prefix = StreamSlotManager::GetNodeName(m_slot) + ":";
    FILE* f = popen("pw-link -o -l 2>/dev/null", "r");
    if (!f) {
        return false;
    }
    // Output ports are listed at column 0, each followed by its links
    // indented as "  |-> peer:port".
    bool ours = false;
    bool linked = false;
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] != ' ') {
            ours = strncmp(line, prefix.c_str(), prefix.size()) == 0;
        } else if (ours && strstr(line, "|->")) {
            linked = true;
        }
    }
    pclose(f);
    return linked;
}

// ---------------------------------------------------------------------------
// The card clock
//
// Nothing in this stream resamples: PipeWire takes the stream's samples at
// the card's pace, which is whatever the card's crystal makes it -- 432 ppm
// fast on an AM62x I2S cape, 47 ppm slow on a Pi 5 HAT, tens of ppm either way
// for USB.  A feed paced by the system clock delivers at the system's pace
// instead.  A fast card then drains the stream's cushion (one quantum) and
// plays a quantum of silence: on the 432 ppm cape, 12 ms every 27 s.  A slow
// one builds a queue that delays the sound further behind the lights for as
// long as the stream is fed.
//
// So every pipeline feeding the stream runs on this clock, which is held to
// the card.  Its rate is fitted to the card's own hardware pointer (hw_ptr in
// /proc/asound/.../status, which ALSA updates from the DMA position when the
// file is read) against CLOCK_MONOTONIC, the clock's internal time, over the
// last two minutes.  On top of that, any accumulated phase error is closed
// over ~20 s, so the queue between the feed and the card stays where it
// started instead of wandering with every error in the fit.  The media
// position the lights follow comes from the feed, so it is card time too;
// ClockRate() lets the channel output thread run its frames at the same pace.
//
// Without a card (a group of network sinks) the clock is left at the system
// rate.
// ---------------------------------------------------------------------------

GstClock* PipeWireOutputStream::Clock() {
    std::unique_lock<std::mutex> lock(m_lock);
    if (!m_clock) {
        std::string name = "fpp-card-clock-" + std::to_string(m_slot);
        m_clock = GST_CLOCK(g_object_new(GST_TYPE_SYSTEM_CLOCK, "name", name.c_str(),
                                         "clock-type", GST_CLOCK_TYPE_MONOTONIC, NULL));
        gst_object_ref_sink(m_clock);
    }
    return GST_CLOCK(gst_object_ref(m_clock));
}

void PipeWireOutputStream::ResetClockSyncLocked() {
    m_syncAnchored = false;
    m_syncWindow.clear();
}

static bool ReadCardPosition(const std::string& path, int64_t& hw) {
    std::string status = GetFileContents(path);
    if (status.find("state: RUNNING") == std::string::npos) {
        return false;
    }
    std::size_t p = status.find("hw_ptr");
    if (p == std::string::npos || (p = status.find(':', p)) == std::string::npos) {
        return false;
    }
    hw = strtoll(status.c_str() + p + 1, nullptr, 10);
    return true;
}

void PipeWireOutputStream::DisciplineClock() {
    std::string path;
    {
        std::unique_lock<std::mutex> lock(m_lock);
        if (!m_clock || m_state != State::Ready || m_cardStatusPath.empty()) {
            ResetClockSyncLocked();
            return;
        }
        GstClockTime nowInt = gst_clock_get_internal_time(m_clock);
        if (m_syncAnchored && nowInt - m_syncLastInt < (GstClockTime)kClockSampleMs * GST_MSECOND) {
            return;
        }
        path = m_cardStatusPath;
        if (path != m_syncPath) {
            // Another card: what was learnt about the last one doesn't apply.
            m_syncPath = path;
            m_syncCardRate = 0;
            m_clockRate = 1.0;
            ResetClockSyncLocked();
        }
    }
    if (m_syncCardRate <= 0) {
        std::string hwParams = path.substr(0, path.rfind('/')) + "/hw_params";
        std::string text = GetFileContents(hwParams);
        std::size_t p = text.find("rate:");
        m_syncCardRate = p == std::string::npos ? 0 : atoi(text.c_str() + p + 5);
        if (m_syncCardRate <= 0) {
            return;
        }
    }
    // The card's position and the clock's internal time, as close together
    // as they can be read: the midpoint of the read.
    GstClockTime before = gst_clock_get_internal_time(m_clock);
    int64_t hw = 0;
    bool running = ReadCardPosition(path, hw);
    GstClockTime after = gst_clock_get_internal_time(m_clock);
    GstClockTime internal = before + (after - before) / 2;

    std::unique_lock<std::mutex> lock(m_lock);
    if (path != m_cardStatusPath || m_state != State::Ready) {
        return;
    }
    if (!running) {
        ResetClockSyncLocked();
        return;
    }
    GstClockTime cInt, cExt, num, den;
    gst_clock_get_calibration(m_clock, &cInt, &cExt, &num, &den);
    GstClockTime external = gst_clock_adjust_with_calibration(m_clock, internal, cInt, cExt, num, den);

    double cardS = m_syncAnchored ? (double)(hw - m_syncHw0) / m_syncCardRate : 0.0;
    double clockS = m_syncAnchored ? (double)(GstClockTimeDiff)(external - m_syncExt0) / GST_SECOND : 0.0;
    double phase = cardS - clockS;  // > 0: the card is ahead of the clock
    if (!m_syncAnchored || hw < m_syncLastHw || std::fabs(phase) > kClockMaxPhaseError) {
        if (m_syncAnchored) {
            LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: card clock re-anchored (phase %.1f ms)\n",
                     m_slot, phase * 1000.0);
        }
        // Start (again) from here, keeping the rate learnt so far.
        m_syncAnchored = true;
        m_syncHw0 = hw;
        m_syncInt0 = internal;
        m_syncExt0 = external;
        m_syncWindow.clear();
        cardS = clockS = phase = 0.0;
    }
    m_syncLastHw = hw;
    m_syncLastInt = internal;

    double intS = (double)(internal - m_syncInt0) / GST_SECOND;
    m_syncWindow.emplace_back(intS, cardS);
    while (m_syncWindow.size() > 2 && intS - m_syncWindow.front().first > kClockWindowS) {
        m_syncWindow.pop_front();
    }
    double rate = m_clockRate.load(std::memory_order_relaxed);
    if (intS - m_syncWindow.front().first >= kClockMinFitS) {
        double mx = 0, my = 0;
        for (auto& [x, y] : m_syncWindow) {
            mx += x;
            my += y;
        }
        mx /= m_syncWindow.size();
        my /= m_syncWindow.size();
        double sxy = 0, sxx = 0;
        for (auto& [x, y] : m_syncWindow) {
            sxy += (x - mx) * (y - my);
            sxx += (x - mx) * (x - mx);
        }
        if (sxx > 0) {
            rate = std::clamp(sxy / sxx, 1.0 - kClockMaxOffset, 1.0 + kClockMaxOffset);
            m_clockRate = rate;
        }
    }
    double applied = rate + std::clamp(phase * kClockPhaseGain, -kClockMaxPhaseSlew, kClockMaxPhaseSlew);
    applied = std::clamp(applied, 1.0 - kClockMaxOffset, 1.0 + kClockMaxOffset);
    // Continuous at `internal`: only the rate from here on changes.
    gst_clock_set_calibration(m_clock, internal, external, (GstClockTime)llround(applied * 1e9), 1000000000ULL);
    LogExcess(VB_MEDIAOUT, "PipeWire output stream %d: card %+.1f ppm, phase %+.2f ms, clock %+.1f ppm\n",
              m_slot, (rate - 1.0) * 1e6, phase * 1000.0, (applied - 1.0) * 1e6);
}

bool PipeWireOutputStream::CardRunning() {
    if (m_cardStatusPath.empty()) {
        return false;
    }
    return GetFileContents(m_cardStatusPath).find("state: RUNNING") != std::string::npos;
}

void PipeWireOutputStream::CloseLocked(const char* why) {
    if (m_pipeline) {
        LogInfo(VB_MEDIAOUT, "PipeWire output stream %d closing (%s)\n", m_slot, why);
        GstElement* pipeline = m_pipeline;
        GstElement* src = m_src;
        // Off this thread: a pipewiresink state change can block on the
        // daemon, and the worker services every slot.
        std::thread([pipeline, src]() {
            SetThreadName("FPP-PWStreamEnd");
            gst_element_set_state(pipeline, GST_STATE_NULL);
            if (src) {
                gst_object_unref(src);
            }
            gst_object_unref(pipeline);
        }).detach();
    }
    gst_caps_replace(&m_caps, nullptr);
    m_rate = 0;
    m_queuedUntilUs = 0;
    m_pipeline = nullptr;
    m_src = nullptr;
    // m_feed stays: it belongs to the track using the slot (AttachFeed() /
    // Release()), not to this stream.  Clearing it here orphaned a track
    // whose start triggered a reopen -- a new target -- and the whole track
    // played silent.  Its samples are dropped while there is no m_src.
    m_target.clear();
    m_linked = false;
    m_state = State::Closed;
    m_stateChanged.notify_all();
}

#endif
