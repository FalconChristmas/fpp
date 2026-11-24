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

#include <gst/base/gstbasesink.h>

#ifdef HAS_GSTREAMER

#include <gst/app/gstappsink.h>

// The stream is a native PipeWire stream (see OpenLocked()).  Without
// libpipewire's headers (macOS) there is no stream: ForSlot() returns none,
// and media plays through a pipewiresink of its own.
#if __has_include(<pipewire/pipewire.h>)
#define FPP_NATIVE_PIPEWIRE 1
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

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

// The graph's cycle (FPP's default quantum is 1024 frames).
constexpr int64_t kGraphQuantum = 1024;
// How many graph cycles ahead of its time a feed hands each buffer over (see
// AttachFeed()), and how far ahead of its time the graph may take a sample
// from the stream: a whole cycle, at worst.  Carrying on a track's timeline
// (ContinueTimeline()) needs the last track's sound still queued when its
// end of stream comes, a cycle more than the graph may already have taken.
constexpr int64_t kFeedLeadQuanta = 2;
constexpr int64_t kGraphAheadQuanta = 1;
// The least time the last track's sound is left queued, beyond what the
// graph may already have taken, when its end of stream comes: the time the
// next track has to carry on before silence goes in between them -- the end
// of stream reaching the playlist, which only looks on its main loop's ticks
// (a sequence's frame, up to 50 ms at 20 fps), the next entry starting, and
// its pipeline going to PLAYING.  Measured from the end of stream to the next
// track carrying on: 1-5 ms on a Pi, 3-6 ms on an AM62x, 16-32 ms on a
// BeagleBone Black at 100 fps; and its first buffer comes some ms after that.
// Short of it, the first buffer comes after the graph may need it, and the
// stream is flushed (the track's head is lost, and the next track starts on a
// drained stream).  Where a sample plays doesn't depend on this.
constexpr int64_t kFeedHandoverMs = 100;

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
// How long after one of our own flushes the card stopping or restarting is
// put down to the flush.  A flush takes the stream inactive and back, and
// where it is the card's only follower (a Pi 4's headphone jack) PipeWire
// stops the device and starts it again; the samples a few hundred ms later
// see that.
constexpr uint64_t kFlushRestartMs = 1000;
// How long a stopped track's timeline is kept running with silence for the
// next track to carry on (Release(), ContinueTimeline()).  A playlist's next
// track starts within tens of ms of the last one stopping; one that comes
// later than this (after a pause entry, or a sequence that outlasts its
// media) starts on a flushed stream, as every track used to.
constexpr int kBridgeMs = 3000;
// How little of the timeline a bridge lets be left before it adds silence,
// and how far ahead it then takes it: a little more than the worker's
// wake-ups (4 ms apart).  The next track normally comes before the last
// one's sound has run out, and any silence laid before it needs it is a gap
// between them.  A bridge that runs out anyway (a worker held up) only ends
// the timeline: the next track starts on a flushed stream.
constexpr int kBridgeLowMs = 6;
// Most silence put in front of a carried-on track whose sound starts after
// the start of its timeline (see NoteTimelineLocked()).
constexpr int kMaxLeadInGapMs = 500;
constexpr int kBridgeHighMs = 10;
// How long a pipeline carrying on the timeline is given to go to PLAYING
// and hand over its first buffer, and the least it is left in the queue
// beyond that (silence is added if less is left).  A prepared pipeline goes
// to PLAYING in about a millisecond.
constexpr int kContinueHandoverMs = 3;
// How far behind its sink's schedule a feed buffer may come before it counts
// as late (NoteTimelineLocked()): a thread's wake-up, not a stall.
constexpr int kFeedLateToleranceMs = 5;
constexpr int kContinueMinQueuedMs = 8;

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
    return (uint64_t)GetMonotonicTimeMS(); // holds and timeouts: must not step with the wall clock
}
}

PipeWireOutputStream::PipeWireOutputStream(int slot) :
    m_slot(slot) {
}

PipeWireOutputStream* PipeWireOutputStream::ForSlot(int slot) {
#ifndef FPP_NATIVE_PIPEWIRE
    return nullptr;
#endif
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

bool PipeWireOutputStream::SlotMayCarryOn(int slot) {
    if (slot < 1 || slot > StreamSlotManager::MAX_SLOTS) {
        return false;
    }
    PipeWireOutputStream* s;
    {
        std::unique_lock<std::mutex> lock(s_slotsLock);
        s = s_slots[slot];
    }
    if (!s) {
        return false;
    }
    std::unique_lock<std::mutex> lock(s->m_lock);
    return s->m_timelineValid && s->m_state == State::Ready;
}

int64_t PipeWireOutputStream::QueuedAheadNsForSlot(int slot) {
    if (slot < 1 || slot > StreamSlotManager::MAX_SLOTS) {
        return -1;
    }
    PipeWireOutputStream* s;
    {
        std::unique_lock<std::mutex> lock(s_slotsLock);
        s = s_slots[slot];
    }
    return s ? s->QueuedAheadNs() : -1;
}

int64_t PipeWireOutputStream::QueuedAheadNs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t nowNs = (int64_t)ts.tv_sec * GST_SECOND + ts.tv_nsec;
    std::unique_lock<std::mutex> lock(m_lock);
    uint64_t heard;
    if (!m_pwStream || m_rate <= 0 || !FrameHeardAt(nowNs, &heard)) {
        return -1;
    }
    uint64_t write = m_writePos.load(std::memory_order_relaxed);
    return write > heard ? (int64_t)gst_util_uint64_scale(write - heard, GST_SECOND, m_rate) : 0;
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

static int64_t MonoUs();

void PipeWireOutputStream::Release(GstElement* feed) {
    {
        std::unique_lock<std::mutex> lock(m_lock);
        if (feed) {
            // Nothing asks what this feed is heard at once it stops.
            DropFeedSpansLocked(feed);
        }
        if (feed && m_feed == feed) {
            m_feed = nullptr;
            m_continuedFirst = false;
            if (m_timelineValid && m_state == State::Ready) {
                // Keep the timeline running for the next track to carry on
                // (ContinueTimeline()): silence laid right behind the last
                // sample, topped up by the worker (ServiceBridge()).  It also
                // pushes the track's last fragment out, as below.
                m_bridging = true;
                m_bridgeUntilMs = NowMs() + kBridgeMs;
                m_releasedUs = MonoUs();
                m_bridgedNs = 0;
            } else {
                EndTimelineLocked();
            }
        }
        if (m_users > 0) {
            m_users--;
        }
        m_holdUntilMs = std::max(m_holdUntilMs, NowMs() + LINGER_MS);
    }
    WakeWorker();
}

// How far ahead of its time the graph may take a sample from the stream: the
// queue counts as run out that much before the timeline's end.
static GstClockTime GraphAheadNs() {
    return (GstClockTime)(kGraphAheadQuanta * kGraphQuantum * 1000000000LL / GStreamerOutput::PipeWireGraphRate());
}

static int64_t MonoUs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

bool PipeWireOutputStream::WaitQueueEmpty(int64_t deadlineUs) {
    while (true) {
        uint64_t left;
        int rate;
        {
            std::unique_lock<std::mutex> lock(m_lock);
            uint64_t read = std::max(m_readPos.load(std::memory_order_acquire), m_dropTo.load(std::memory_order_acquire));
            uint64_t write = m_writePos.load(std::memory_order_acquire);
            left = write > read ? write - read : 0;
            rate = m_rate;
            if (!m_pwStream || rate <= 0) {
                return true;
            }
        }
        if (left == 0) {
            return true;
        }
        if (MonoUs() >= deadlineUs) {
            return false;
        }
        int64_t us = (int64_t)(left * 1000000 / (uint64_t)rate);
        std::this_thread::sleep_for(std::chrono::microseconds(std::clamp<int64_t>(us, 1000, 5000)));
    }
}

void PipeWireOutputStream::WaitDrained(int maxMs) {
    {
        // A bridge would never let it drain.
        std::unique_lock<std::mutex> lock(m_lock);
        EndTimelineLocked();
    }
    WaitQueueEmpty(MonoUs() + (int64_t)maxMs * 1000);
    Flush(FlushFor::NextTrack);
}

// A flush drops whatever is still queued for the graph.  Between tracks that
// is normally nothing; after a stall that never reached the card (a late
// group node) it is what the stall held up, which would otherwise sit ahead
// of every track to come (issue #3083).  When the stall is an xrun on the
// card itself, DisciplineClock() sees the card stop or restart, and once it
// runs again the stream is flushed mid-track: the sound skips what the stall
// held up -- the xrun was already audible -- and is back on time at once
// rather than at the next track.  A stall that never reaches the card is
// left to the flush between tracks.
//
// Only the queue is dropped: the PipeWire stream itself runs on, playing
// silence while there is nothing to play, so nothing restarts and nothing
// after the flush is lost.  Where what follows lands doesn't need guessing
// either: HeardRunningTime() measures it.
void PipeWireOutputStream::Flush(FlushFor why) {
    {
        std::unique_lock<std::mutex> lock(m_lock);
        // Whatever the timeline was, a flush ends it, and with it anything
        // that may have been late in it.
        EndTimelineLocked();
        m_continuedFirst = false;
        // A flush covers an xrun still waiting for one.
        m_xrunPending = false;
        if (!m_pwStream || m_state != State::Ready || m_flushed) {
            return;
        }
        m_flushed = true;
        m_lastFlushMs = NowMs();
        m_dropTo.store(m_writePos.load(std::memory_order_relaxed), std::memory_order_release);
    }
    LogDebug(VB_MEDIAOUT, "PipeWire output stream %d flushed%s\n", m_slot,
             why == FlushFor::NextTrack ? " between tracks" : why == FlushFor::MidFeed ? " mid-feed" : " for a seek");
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

int64_t PipeWireOutputStream::FeedLeadNs() {
    int64_t quantum = kGraphQuantum * (int64_t)1000000000 / GStreamerOutput::PipeWireGraphRate();
    return std::max(kFeedLeadQuanta * quantum, kGraphAheadQuanta * quantum + kFeedHandoverMs * (int64_t)1000000);
}


void PipeWireOutputStream::DropFeedSpansLocked(GstElement* feed) {
    m_feedSpans.erase(std::remove_if(m_feedSpans.begin(), m_feedSpans.end(),
                                     [feed](const FeedSpan& s) { return s.feed == feed; }),
                      m_feedSpans.end());
}

void PipeWireOutputStream::AttachFeed(GstElement* appsink) {
    {
        std::unique_lock<std::mutex> lock(m_lock);
        // Spans are known by their feed's address, and an appsink can be
        // allocated where a freed one was: any already under this one are
        // that one's, and would be taken for this track's sound.
        DropFeedSpansLocked(appsink);
        m_feed = appsink;
        m_continuedFirst = false;
        // Each feed starts at the card's own pace.  The feed clock is put back
        // on the card clock now, before this feed's pipeline has a base time
        // on it, so a speed the last track was steered at doesn't carry over.
        m_feedSpeed = 1.0;
        m_feedDiverged = false;
        m_rsActive = false;
        m_rsPos = 0.0;
        m_rsHistFrames = 0;
        if (m_clock && m_feedClock) {
            RecalibrateFeedClockLocked(gst_clock_get_internal_time(m_clock));
        }
    }
    // Run the feed two graph cycles (FeedLeadNs()) ahead of the clock.  Handed over exactly
    // on time, a buffer can miss the cycle that needed it -- the graph may
    // take a sample up to a cycle before its time; PipeWire then plays a
    // quantum of silence there and the rest of the track is a quantum late.
    // Two cycles' worth queued absorbs that, and leaves the last track's
    // sound still queued when a track carrying on its timeline takes over
    // (at least kFeedHandoverMs beyond the cycle, whatever the graph's rate).
    // (Where a sample plays doesn't depend on this: the queue is contiguous
    // from the track's first sample.)
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

    GstBuffer* out = nullptr;
    {
        std::unique_lock<std::mutex> lock(self->m_lock);
        bool ours = in && caps && self->m_pwStream && self->m_feed == appsink;
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
            // Steered to another speed (SetFeedSpeed()), the media is
            // resampled to the card's rate here; otherwise it goes in as it
            // came, and only its last frames are kept for a later start.
            if (self->m_rsActive) {
                out = self->ResampleLocked(in);
            } else {
                self->KeepResampleHistoryLocked(in);
                out = gst_buffer_ref(in);
            }
            ours = out != nullptr;
        }
        GstMapInfo map;
        if (ours && gst_buffer_map(out, &map, GST_MAP_READ)) {
            size_t frames = map.size / kBytesPerFrame;
            // The feed clock is the card clock (until it is steered, then off
            // it by however far it has run ahead or behind), so a buffer's
            // running time in the feeding pipeline maps onto the stream's
            // timeline through the base time and that offset.
            GstClockTime rt = gst_segment_to_running_time(segment, GST_FORMAT_TIME, GST_BUFFER_PTS(in));
            if (GST_CLOCK_TIME_IS_VALID(rt)) {
                // A feed normally hands over a lead early (a ts-offset of minus
                // FeedLeadNs()); one carrying on the timeline runs on a base
                // time that much earlier with no offset instead (see
                // GStreamerOutput's m_timelineShiftNs).  Either way the sample
                // belongs where base + running time + offset + lead says.
                GstClockTimeDiff tsOffset = gst_base_sink_get_ts_offset(GST_BASE_SINK(appsink));
                GstClockTimeDiff shift = tsOffset + FeedLeadNs();
                GstClockTime abs = gst_element_get_base_time(appsink) + rt;
                // When the sink was due to hand it over: as for any sink, no
                // earlier than its base time.  The first buffers of a track all
                // come at once, at that, and that is on schedule.
                GstClockTimeDiff late = 0;
                if (GstClock* feedClock = gst_element_get_clock(appsink)) {
                    GstClockTimeDiff due = (GstClockTimeDiff)rt + tsOffset;
                    late = GST_CLOCK_DIFF(gst_element_get_base_time(appsink) + (due > 0 ? due : 0),
                                          gst_clock_get_time(feedClock));
                    gst_object_unref(feedClock);
                }
                abs = shift < 0 && (GstClockTime)-shift > abs ? 0 : abs + shift;
                if (self->m_feedDiverged) {
                    GstClockTimeDiff ahead = GST_CLOCK_DIFF(gst_clock_get_time(self->m_clock),
                                                            gst_clock_get_time(self->m_feedClock));
                    abs = ahead > 0 && (GstClockTime)ahead > abs ? 0 : abs - ahead;
                }
                // May queue silence ahead of it (a carried-on track's lead-in).
                self->NoteTimelineLocked(abs, frames, late);
            } else if (self->m_timelineValid && self->m_rate > 0) {
                // Outside the segment (a decoder's last, clipped buffer): no
                // time of its own, but it is queued right behind the one
                // before it all the same.
                LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: a feed buffer outside its segment\n", self->m_slot);
                self->m_timelineEnd += gst_util_uint64_scale(frames, GST_SECOND, self->m_rate);
            }
            uint64_t pos = self->WriteQueueLocked(reinterpret_cast<const float*>(map.data), frames);
            if (pos != UINT64_MAX) {
                // What this buffer is, for HeardRunningTime(): its running
                // time, or (outside the segment) the end of the one before.
                double speed = self->m_rsActive ? self->m_feedSpeed : 1.0;
                if (!GST_CLOCK_TIME_IS_VALID(rt)) {
                    if (!self->m_feedSpans.empty() && self->m_feedSpans.back().feed == appsink) {
                        const FeedSpan& last = self->m_feedSpans.back();
                        rt = last.runningTime + (GstClockTime)(last.frames * last.speed * GST_SECOND / self->m_rate);
                    }
                }
                if (GST_CLOCK_TIME_IS_VALID(rt)) {
                    self->m_feedSpans.push_back({ pos, frames, rt, speed, appsink });
                }
                // Long gone to the graph (a second is kept, as in
                // HeardRunningTime(), which trims too: a track whose position
                // comes from elsewhere never asks).
                uint64_t read = self->m_readPos.load(std::memory_order_acquire);
                while (!self->m_feedSpans.empty() &&
                       self->m_feedSpans.front().queuePos + self->m_feedSpans.front().frames + (uint64_t)self->m_rate < read) {
                    self->m_feedSpans.pop_front();
                }
            }
            gst_buffer_unmap(out, &map);
            self->m_flushed = false;
        }
    }
    if (out) {
        gst_buffer_unref(out);
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
        bool anyBridging = false;
        uint64_t now = NowMs();
        for (PipeWireOutputStream* s : slots) {
            if (s) {
                if (s->DisciplineClock()) {
                    LogInfo(VB_MEDIAOUT, "PipeWire output stream %d: the card lost time (xrun) while playing; "
                                         "flushing so the sound stays with the lights\n", s->m_slot);
                    s->Flush(FlushFor::MidFeed);
                }
                bool flushNow;
                {
                    std::unique_lock<std::mutex> slock(s->m_lock);
                    flushNow = s->m_flushNow;
                    s->m_flushNow = false;
                }
                if (flushNow) {
                    LogInfo(VB_MEDIAOUT, "PipeWire output stream %d: a track carrying on the timeline starts too far "
                                         "into it; flushing\n", s->m_slot);
                    s->Flush(FlushFor::MidFeed);
                }
                anyBridging |= s->ServiceBridge();
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
        // A bridge between tracks is topped up every few ms.
        s_workerWake.wait_for(wlock, std::chrono::milliseconds(anyBridging ? 4 : anyOpening ? 10 : 250),
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

    if (m_pwStream && m_pwError.exchange(false)) {
        // The stream is dead (PipeWire restarted, the sink went).  Drop it;
        // the next track or prewarm opens a fresh one.
        m_wantOpen = false;
        CloseLocked("stream error");
        m_state = State::Failed;
        m_failedMs = now;
        m_stateChanged.notify_all();
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
    // Our stream must be linked into the graph -- until it is, what it plays
    // goes nowhere.  And the card must be RUNNING: PipeWire runs
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
            struct pw_stream* stream = m_pwStream;
            lock.unlock();
            bool linked = StreamLinked();
            lock.lock();
            if (m_pwStream != stream || m_state != State::Opening) {
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
        LogDebug(VB_MEDIAOUT, "PipeWire output stream %d ready on '%s' after %d ms\n",
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
#ifdef FPP_NATIVE_PIPEWIRE
    std::string target = m_wantTarget;
    int rate = GStreamerOutput::PipeWireGraphRate();

    // Nothing below touches the stream's state until the stream exists, and
    // connecting to the daemon can be slow, so none of it runs under the slot
    // lock, which the feed callback and Acquire() on the playlist thread need.
    lock.unlock();
    GStreamerOutput::EnsureGStreamerInit();
    static std::once_flag pwInit;
    std::call_once(pwInit, [] { pw_init(nullptr, nullptr); });
    std::string cardStatus = GStreamerOutput::AlsaSinkStatusPath(target);
    bool cardWasRunning = !cardStatus.empty() &&
                          GetFileContents(cardStatus).find("state: RUNNING") != std::string::npos;
    std::string loopName = "fpp-pwstream-" + std::to_string(m_slot);
    struct pw_thread_loop* loop = pw_thread_loop_new(loopName.c_str(), nullptr);
    lock.lock();
    if (!loop) {
        LogErr(VB_MEDIAOUT, "PipeWire output stream %d: could not create its thread loop\n", m_slot);
        return false;
    }

    // A queue of four seconds: a feed runs a lead ahead of the graph and the
    // bridge a few ms, so it only fills if the graph stops taking from it.
    m_rate = rate;
    m_ringFrames = (uint64_t)rate * 4;
    m_ring.assign(m_ringFrames * kChannels, 0.0f);
    m_writePos.store(0, std::memory_order_relaxed);
    m_readPos.store(0, std::memory_order_relaxed);
    m_dropTo.store(0, std::memory_order_relaxed);
    m_cycleCount.store(0, std::memory_order_relaxed);
    m_feedSpans.clear();
    m_warnedFull = false;
    m_pwError = false;

    // The identity every per-track pipewiresink used to present, so anything
    // that matches or routes fppd_stream_N sees no difference.  The target is
    // a property: it comes from the PipeWireSinkName setting.
    std::string nodeName = StreamSlotManager::GetNodeName(m_slot);
    std::string nodeDesc = StreamSlotManager::GetNodeDescription(m_slot);
    struct pw_properties* props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio",
                                                    PW_KEY_MEDIA_CATEGORY, "Playback",
                                                    PW_KEY_NODE_NAME, nodeName.c_str(),
                                                    PW_KEY_NODE_DESCRIPTION, nodeDesc.c_str(),
                                                    nullptr);
    if (!target.empty()) {
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, target.c_str());
    }
    // (Lambdas here, inside a member, may call its private statics.)
    static const struct pw_stream_events events = [] {
        struct pw_stream_events e = {};
        e.version = PW_VERSION_STREAM_EVENTS;
        e.state_changed = [](void* data, enum pw_stream_state, enum pw_stream_state state, const char* error) {
            if (state == PW_STREAM_STATE_ERROR) {
                OnStreamError(data, error);
            }
        };
        e.process = [](void* data) { OnProcess(data); };
        return e;
    }();
    pw_thread_loop_lock(loop);
    // Nothing calls back before the stream connects and the loop runs, so
    // m_pwStream is set by the time OnProcess() first reads it.
    struct pw_stream* stream = pw_stream_new_simple(pw_thread_loop_get_loop(loop), nodeName.c_str(), props,
                                                    &events, this);
    bool ok = stream != nullptr;
    if (ok) {
        m_pwStream = stream;
        m_pwLoop = loop;
        uint8_t podBuffer[1024];
        struct spa_pod_builder b = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
        struct spa_audio_info_raw info = {};
        info.format = SPA_AUDIO_FORMAT_F32;
        info.channels = kChannels;
        info.rate = (uint32_t)rate;
        info.position[0] = SPA_AUDIO_CHANNEL_FL;
        info.position[1] = SPA_AUDIO_CHANNEL_FR;
        const struct spa_pod* params[1] = { spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info) };
        ok = pw_stream_connect(stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
                               (enum pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                                                      PW_STREAM_FLAG_RT_PROCESS),
                               params, 1) == 0;
    }
    // (pw_stream_new_simple() owns the properties, created or not.)
    pw_thread_loop_unlock(loop);
    if (ok) {
        ok = pw_thread_loop_start(loop) == 0;
    }
    if (!ok) {
        LogErr(VB_MEDIAOUT, "PipeWire output stream %d: could not connect to '%s'\n", m_slot, target.c_str());
        // Once the stream exists, m_pwLoop owns the loop and CloseLocked()
        // destroys it (and clears m_pwLoop, so it can't be asked afterwards).
        // Only a loop that never got that far is still ours to destroy.
        bool loopHandedOver = m_pwLoop == loop;
        CloseLocked("could not connect");
        if (!loopHandedOver) {
            pw_thread_loop_destroy(loop);
        }
        return false;
    }

    GstCaps* fixedCaps = gst_caps_from_string(("audio/x-raw" + CapsFields()).c_str());
    gst_caps_replace(&m_caps, fixedCaps);
    gst_caps_unref(fixedCaps);
    m_warnedCaps = false;
    EndTimelineLocked();
    m_continuedFirst = false;
    m_flushNow = false;
    m_xrunPending = false;
    m_flushed = true;
    m_target = target;
    m_cardStatusPath = cardStatus;
    m_cardWasRunning = cardWasRunning;
    m_openStartedMs = NowMs();
    m_linked = false;
    m_linkedMs = 0;
    m_lastLinkCheckMs = 0;
    m_state = State::Opening;
    LogDebug(VB_MEDIAOUT, "PipeWire output stream %d opening on '%s' (card status %s)\n", m_slot,
            target.empty() ? "(default)" : target.c_str(), cardStatus.empty() ? "n/a" : cardStatus.c_str());
    return true;
#else
    return false;
#endif
}

void PipeWireOutputStream::OnStreamError(void* data, const char* error) {
    PipeWireOutputStream* self = static_cast<PipeWireOutputStream*>(data);
    LogWarn(VB_MEDIAOUT, "PipeWire output stream %d: %s\n", self->m_slot, error ? error : "error");
    self->m_pwError = true;
    WakeWorker();
}

// PipeWire's data thread, once a graph cycle: the next `requested` frames
// from the queue (silence for whatever isn't there), and a record of where
// they came from and when PipeWire says they will be heard.
void PipeWireOutputStream::OnProcess(void* data) {
#ifdef FPP_NATIVE_PIPEWIRE
    PipeWireOutputStream* self = static_cast<PipeWireOutputStream*>(data);
    struct pw_buffer* b = pw_stream_dequeue_buffer(self->m_pwStream);
    if (!b) {
        return;
    }
    struct spa_data& d = b->buffer->datas[0];
    if (!d.data) {
        pw_stream_queue_buffer(self->m_pwStream, b);
        return;
    }
    uint32_t n = d.maxsize / kBytesPerFrame;
    if (b->requested && b->requested < n) {
        n = (uint32_t)b->requested;
    }
    // The time this cycle's samples reach the device: the graph's delay to
    // it, and anything the stream's own converter holds or we queued before.
    struct pw_time t = {};
    pw_stream_get_time_n(self->m_pwStream, &t, sizeof(t));
    int64_t heardNs = 0;
    if (t.rate.denom > 0 && t.now > 0) {
        heardNs = t.now + t.delay * (int64_t)GST_SECOND * t.rate.num / t.rate.denom +
                  ((int64_t)t.buffered + (int64_t)(t.queued / kBytesPerFrame)) * (int64_t)GST_SECOND / self->m_rate;
    }

    uint64_t read = self->m_readPos.load(std::memory_order_relaxed);
    uint64_t drop = self->m_dropTo.load(std::memory_order_acquire);
    if (read < drop) {
        read = drop;
    }
    uint64_t write = self->m_writePos.load(std::memory_order_acquire);
    uint64_t avail = write > read ? write - read : 0;
    uint32_t take = (uint32_t)std::min<uint64_t>(avail, n);
    float* out = static_cast<float*>(d.data);
    uint64_t at = read % self->m_ringFrames;
    uint64_t first = std::min<uint64_t>(take, self->m_ringFrames - at);
    const float* ring = self->m_ring.data();
    std::memcpy(out, ring + at * kChannels, first * kBytesPerFrame);
    if (take > first) {
        std::memcpy(out + first * kChannels, ring, (take - first) * kBytesPerFrame);
    }
    if (take < n) {
        std::memset(out + (size_t)take * kChannels, 0, (size_t)(n - take) * kBytesPerFrame);
    }
    d.chunk->offset = 0;
    d.chunk->stride = kBytesPerFrame;
    d.chunk->size = n * kBytesPerFrame;

    uint64_t count = self->m_cycleCount.load(std::memory_order_relaxed);
    CycleRecord& r = self->m_cycles[count % kCycleRecords];
    r.seq.fetch_add(1, std::memory_order_acq_rel);  // odd: being written
    r.readPos.store(read, std::memory_order_relaxed);
    r.frames.store(take, std::memory_order_relaxed);
    r.heardNs.store(heardNs, std::memory_order_relaxed);
    r.seq.fetch_add(1, std::memory_order_release);  // even: done
    self->m_cycleCount.store(count + 1, std::memory_order_release);
    self->m_readPos.store(read + take, std::memory_order_release);
    pw_stream_queue_buffer(self->m_pwStream, b);
#else
    (void)data;
#endif
}

uint64_t PipeWireOutputStream::WriteQueueLocked(const float* data, size_t frames) {
    if (!m_pwStream || m_ringFrames == 0 || frames == 0) {
        return UINT64_MAX;
    }
    uint64_t write = m_writePos.load(std::memory_order_relaxed);
    uint64_t read = std::max(m_readPos.load(std::memory_order_acquire), m_dropTo.load(std::memory_order_relaxed));
    uint64_t used = write > read ? write - read : 0;
    if (used + frames > m_ringFrames) {
        // The graph stopped taking from the stream for seconds.  Dropping
        // what comes is the least bad thing; the xrun handling flushes.
        if (!m_warnedFull) {
            m_warnedFull = true;
            LogWarn(VB_MEDIAOUT, "PipeWire output stream %d: queue full, dropping audio\n", m_slot);
        }
        return UINT64_MAX;
    }
    m_warnedFull = false;
    uint64_t at = write % m_ringFrames;
    uint64_t first = std::min<uint64_t>(frames, m_ringFrames - at);
    float* ring = m_ring.data();
    if (data) {
        std::memcpy(ring + at * kChannels, data, first * kBytesPerFrame);
        std::memcpy(ring, data + first * kChannels, (frames - first) * kBytesPerFrame);
    } else {
        std::memset(ring + at * kChannels, 0, first * kBytesPerFrame);
        std::memset(ring, 0, (frames - first) * kBytesPerFrame);
    }
    m_writePos.store(write + frames, std::memory_order_release);
    return write;
}

bool PipeWireOutputStream::FrameHeardAt(int64_t nowNs, uint64_t* frame) {
    uint64_t count = m_cycleCount.load(std::memory_order_acquire);
    if (count == 0 || m_rate <= 0) {
        return false;
    }
    // The newest cycle whose first sample is heard by now; samples go out at
    // the rate from there, up to the last one that cycle was given.
    uint64_t oldest = count > (uint64_t)kCycleRecords - 4 ? count - (kCycleRecords - 4) : 0;
    for (uint64_t c = count; c-- > oldest;) {
        CycleRecord& r = m_cycles[c % kCycleRecords];
        uint32_t seq = r.seq.load(std::memory_order_acquire);
        uint64_t readPos = r.readPos.load(std::memory_order_relaxed);
        uint32_t frames = r.frames.load(std::memory_order_relaxed);
        int64_t heardNs = r.heardNs.load(std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_acquire);
        if ((seq & 1) || r.seq.load(std::memory_order_relaxed) != seq || heardNs == 0) {
            continue;
        }
        if (heardNs <= nowNs) {
            uint64_t since = (uint64_t)((nowNs - heardNs) * (int64_t)m_rate / (int64_t)GST_SECOND);
            *frame = readPos + std::min<uint64_t>(since, frames);
            return true;
        }
        if (c == oldest) {
            // Even the oldest record isn't heard yet: nothing after it is.
            *frame = readPos;
            return true;
        }
    }
    return false;
}

int PipeWireOutputStream::HeardRunningTime(GstElement* feed, GstClockTime* runningTime) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t nowNs = (int64_t)ts.tv_sec * GST_SECOND + ts.tv_nsec;
    std::unique_lock<std::mutex> lock(m_lock);
    uint64_t heard;
    if (!feed || !m_pwStream || !FrameHeardAt(nowNs, &heard)) {
        return -1;
    }
    // Spans long heard are no longer needed (a second is kept).
    while (!m_feedSpans.empty() &&
           m_feedSpans.front().queuePos + m_feedSpans.front().frames + (uint64_t)m_rate < heard) {
        m_feedSpans.pop_front();
    }
    const FeedSpan* last = nullptr;
    bool inside = false;
    for (const FeedSpan& span : m_feedSpans) {
        if (span.feed != feed) {
            continue;
        }
        if (heard < span.queuePos) {
            inside = true;  // between two of its spans: at the end of the first
            break;
        }
        last = &span;
        if (heard < span.queuePos + span.frames) {
            inside = true;
            break;
        }
    }
    if (!last) {
        return 0;
    }
    if (!inside) {
        // All of it heard and nothing more given: a seek flushed it and it
        // hasn't resumed, or it ran dry.  The speaker isn't playing it, so
        // its last sample's running time stays put while the pipeline's
        // runs on (or, PAUSED, its clock does): the difference between them
        // is no latency.
        return -2;
    }
    uint64_t into = std::min<uint64_t>(heard - last->queuePos, last->frames);
    *runningTime = last->runningTime + (GstClockTime)(into * last->speed * GST_SECOND / m_rate);
    return 1;
}

void PipeWireOutputStream::NoteTimelineLocked(GstClockTime start, size_t frames, GstClockTimeDiff lateNs) {
    if (!m_clock || m_rate <= 0) {
        return;
    }
    if (m_continuedFirst) {
        GstClockTime nowT = gst_clock_get_time(m_clock);
        LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: first buffer of a carried-on track: starts %+.1f ms from the timeline's end, handed over %+.1f ms from it\n",
                 m_slot, ((double)(GstClockTimeDiff)(start - m_timelineEnd)) / 1e6,
                 ((double)(GstClockTimeDiff)(nowT - m_timelineEnd)) / 1e6);
    }
    bool ranDry = gst_clock_get_time(m_clock) + GraphAheadNs() > m_timelineEnd;
    if (m_continuedFirst && m_timelineValid && start > m_timelineEnd + GST_SECOND / (GstClockTime)m_rate) {
        // Its sound doesn't start at the start of its timeline (an MP4's
        // audio often starts a frame or so after its video): the stream plays
        // what it is given back to back, so what lies between goes in as
        // silence, or the track would play that much ahead of its position.
        // The silence is pushed now, ahead of this buffer.  (Whether or not
        // the graph may already be taking from the end of the queue: if it
        // ran dry, the check below sees this buffer late.)
        if (start - m_timelineEnd <= (GstClockTime)kMaxLeadInGapMs * GST_MSECOND) {
            LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: the carried-on track's sound starts %.1f ms in; "
                                  "silence until then\n", m_slot, (double)(start - m_timelineEnd) / 1e6);
            ExtendTimelineLocked(start);
        } else {
            // Far too much to make up in silence: take it as a fault.
            m_flushNow = true;
            WakeWorker();
        }
    }
    if (m_timelineValid && lateNs > (GstClockTimeDiff)kFeedLateToleranceMs * GST_MSECOND && ranDry) {
        // Handed over late, and after the graph may have needed it: the queue
        // may have run dry, and then this and everything after it play that
        // much later.  Nothing to do about it: the position follows what is
        // heard (HeardRunningTime()), so the lights stay with the sound, and
        // a next track carrying on still queues right behind it.  (Before the
        // position was measured, a late feed was a lag nothing could see: the
        // track carrying on was flushed instead, and the next one drained.)
        LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: a feed buffer came %.1f ms late\n", m_slot, lateNs / 1e6);
    }
    m_continuedFirst = false;
    m_bridging = false;
    m_timelineValid = true;
    m_timelineEnd = start + gst_util_uint64_scale(frames, GST_SECOND, m_rate);
}

void PipeWireOutputStream::EndTimelineLocked() {
    m_timelineValid = false;
    m_bridging = false;
}

void PipeWireOutputStream::ExtendTimelineLocked(GstClockTime until) {
    if (!m_pwStream || m_rate <= 0 || !m_timelineValid || until <= m_timelineEnd) {
        return;
    }
    size_t frames = gst_util_uint64_scale_ceil(until - m_timelineEnd, m_rate, GST_SECOND);
    if (WriteQueueLocked(nullptr, frames) == UINT64_MAX) {
        return;
    }
    m_timelineEnd += gst_util_uint64_scale(frames, GST_SECOND, m_rate);
    m_flushed = false;
}

bool PipeWireOutputStream::ServiceBridge() {
    std::unique_lock<std::mutex> lock(m_lock);
    if (!m_bridging) {
        return false;
    }
    if (m_state != State::Ready || !m_timelineValid || !m_clock || NowMs() >= m_bridgeUntilMs) {
        EndTimelineLocked();
        return false;
    }
    GstClockTime now = gst_clock_get_time(m_clock) + GraphAheadNs();
    if (now > m_timelineEnd) {
        LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: the bridge between tracks ran out\n", m_slot);
        EndTimelineLocked();
        return false;
    }
    if (m_timelineEnd < now + (GstClockTime)kBridgeLowMs * GST_MSECOND) {
        GstClockTime before = m_timelineEnd;
        ExtendTimelineLocked(now + (GstClockTime)kBridgeHighMs * GST_MSECOND);
        m_bridgedNs += m_timelineEnd - before;
    }
    return true;
}

bool PipeWireOutputStream::ContinueTimeline(GstClockTime* baseTime, int64_t* earlyNs) {
    std::unique_lock<std::mutex> lock(m_lock);
    const char* why = nullptr;
    GstClockTime lead = (GstClockTime)FeedLeadNs();
    if (!m_timelineValid) {
        why = "nothing to carry on";
    } else if (m_state != State::Ready || m_target != m_wantTarget || !m_pwStream || !m_clock) {
        why = "the stream is not ready on this sink";
    } else if (m_cardStatusPath.empty() || !m_syncAnchored) {
        // Nothing would see a stall in the graph (DisciplineClock()).
        why = "no card is watched";
    } else {
        uint64_t read = std::max(m_readPos.load(std::memory_order_acquire), m_dropTo.load(std::memory_order_relaxed));
        uint64_t write = m_writePos.load(std::memory_order_relaxed);
        GstClockTime backlog = write > read ? gst_util_uint64_scale(write - read, GST_SECOND, m_rate) : 0;
        if (backlog > 2 * lead) {
            why = "audio is backed up in front of the stream";
        }
    }
    // Counted from where the graph may already be taking samples.
    GstClockTime now = m_clock ? gst_clock_get_time(m_clock) + GraphAheadNs() : 0;
    if (!why && now > m_timelineEnd) {
        why = "it ran out";
    }
    if (why) {
        LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: not carrying on the timeline (%s)\n", m_slot, why);
        EndTimelineLocked();
        return false;
    }
    GstClockTime queued = m_timelineEnd - now;
    ExtendTimelineLocked(now + (GstClockTime)kContinueMinQueuedMs * GST_MSECOND);
    m_bridging = false;
    m_continuedFirst = true;
    *baseTime = m_timelineEnd;
    // Handed over by `now` (a cycle ahead of the clock) plus the handover
    // time; never more than the feed's lead ahead of its time.
    *earlyNs = (int64_t)std::min(lead, m_timelineEnd - now + GraphAheadNs() - (GstClockTime)kContinueHandoverMs * GST_MSECOND);
    LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: carrying on the timeline %.1f ms after the last track ended (%.1f ms queued, "
                          "%.1f ms of silence added, %.1f ms by the bridge before; first sample handed over %.1f ms early)\n",
             m_slot, (MonoUs() - m_releasedUs) / 1e3, queued / 1e6, (m_timelineEnd - now - queued) / 1e6,
             m_bridgedNs / 1e6, *earlyNs / 1e6);
    return true;
}

// Whether this slot's stream has an output port linked to anything, per
// pw-link.  Only run while a stream is opening, every kLinkCheckIntervalMs.
// Time-limited, like the other PipeWire tools fppd runs: a pw-link that never
// answers (a wedged daemon) would otherwise hold this worker for good, and
// shutdown joins it.
bool PipeWireOutputStream::StreamLinked() {
    std::string prefix = StreamSlotManager::GetNodeName(m_slot) + ":";
    FILE* f = popen("timeout -k 1 2 pw-link -o -l 2>/dev/null", "r");
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

void PipeWireOutputStream::EnsureClocksLocked() {
    if (!m_clock) {
        std::string name = "fpp-card-clock-" + std::to_string(m_slot);
        m_clock = GST_CLOCK(g_object_new(GST_TYPE_SYSTEM_CLOCK, "name", name.c_str(),
                                         "clock-type", GST_CLOCK_TYPE_MONOTONIC, NULL));
        gst_object_ref_sink(m_clock);
    }
    if (!m_feedClock) {
        // Both read CLOCK_MONOTONIC as their internal time, and start out
        // calibrated alike, so at speed 1 the feed clock reads exactly what
        // the card clock does.
        std::string name = "fpp-feed-clock-" + std::to_string(m_slot);
        m_feedClock = GST_CLOCK(g_object_new(GST_TYPE_SYSTEM_CLOCK, "name", name.c_str(),
                                             "clock-type", GST_CLOCK_TYPE_MONOTONIC, NULL));
        gst_object_ref_sink(m_feedClock);
        GstClockTime cInt, cExt, num, den;
        gst_clock_get_calibration(m_clock, &cInt, &cExt, &num, &den);
        gst_clock_set_calibration(m_feedClock, cInt, cExt, num, den);
    }
}

GstClock* PipeWireOutputStream::Clock() {
    std::unique_lock<std::mutex> lock(m_lock);
    EnsureClocksLocked();
    return GST_CLOCK(gst_object_ref(m_clock));
}

GstClock* PipeWireOutputStream::FeedClock() {
    std::unique_lock<std::mutex> lock(m_lock);
    EnsureClocksLocked();
    return GST_CLOCK(gst_object_ref(m_feedClock));
}

void PipeWireOutputStream::RecalibrateFeedClockLocked(GstClockTime internal) {
    if (!m_clock || !m_feedClock) {
        return;
    }
    GstClockTime cInt, cExt, num, den;
    gst_clock_get_calibration(m_clock, &cInt, &cExt, &num, &den);
    if (!m_feedDiverged) {
        // Not yet steered: keep it the card clock exactly, no rounding.
        gst_clock_set_calibration(m_feedClock, cInt, cExt, num, den);
        return;
    }
    // Where the feed clock is now on its current calibration, from here on
    // running at the card clock's rate times the speed: continuous at
    // `internal`, so a feed waiting on it never sees it step.
    GstClockTime fInt, fExt, fNum, fDen;
    gst_clock_get_calibration(m_feedClock, &fInt, &fExt, &fNum, &fDen);
    GstClockTime external = gst_clock_adjust_with_calibration(m_feedClock, internal, fInt, fExt, fNum, fDen);
    double rate = (double)num / (double)(den ? den : 1) * m_feedSpeed;
    gst_clock_set_calibration(m_feedClock, internal, external, (GstClockTime)llround(rate * 1e9), 1000000000ULL);
}

void PipeWireOutputStream::SetFeedSpeed(GstElement* feed, double speed) {
    std::unique_lock<std::mutex> lock(m_lock);
    if (!feed || feed != m_feed || !m_clock || !m_feedClock || speed <= 0.0) {
        return;
    }
    if (speed == m_feedSpeed && m_feedDiverged) {
        return;
    }
    // A speed of 1 before any other leaves everything untouched.
    if (speed == 1.0 && !m_feedDiverged) {
        return;
    }
    GstClockTime internal = gst_clock_get_internal_time(m_clock);
    if (!m_feedDiverged) {
        m_feedDiverged = true;
        m_rsActive = true;
        // The next output frame is the next buffer's first: everything up to
        // it has gone out as it came, and m_rsHist holds the frames before.
        m_rsPos = 0.0;
    }
    // Recalibrate at the old speed first (a no-op continuation), then switch.
    RecalibrateFeedClockLocked(internal);
    m_feedSpeed = speed;
    RecalibrateFeedClockLocked(internal);
}

void PipeWireOutputStream::FlushFeed(GstElement* feed) {
    {
        std::unique_lock<std::mutex> lock(m_lock);
        if (!feed || feed != m_feed) {
            return;
        }
        // What comes next doesn't follow on from what was resampled before.
        m_rsHistFrames = 0;
        m_rsPos = 0.0;
    }
    // Unprimed: the seek is already a jump in the sound, and the feed's
    // running time starts again, so its first buffers come on time (a sink
    // can't render ahead of its base time).  Priming would only put silence
    // ahead of where the feed lands.
    Flush(FlushFor::Seek);
}

// The feed's resampler (SetFeedSpeed()): a 16-tap Kaiser-windowed sinc, its
// fractional delay taken from a 256-phase table with linear interpolation
// between phases.  A speed servo only moves the ratio a few tenths of a
// percent from 1, but every sample then sits at a fraction that sweeps
// through the whole range, and a short interpolator's response changes with
// that fraction: a 4-point cubic measured -33 dB THD+N on an 8 kHz tone,
// audible as warbling highs.  This one measures -81 dB at 1 kHz, -88 dB at
// 8 kHz and -74 dB at 15 kHz (44.1 kHz, ratios 0.9995-1.003), and with NEON
// costs 1.7% of a BBB's core (0.3% of a PocketBeagle2's) for 44.1 kHz stereo.
namespace {
constexpr int kRsTaps = 16;
constexpr int kRsHalf = kRsTaps / 2;
constexpr int kRsPhases = 256;
constexpr int kRsHist = kRsTaps - 1;  // frames before the next buffer it reaches back to
constexpr double kRsKaiserBeta = 7.0;

const float* ResampleTable() {
    static const std::vector<float> table = [] {
        auto bessel0 = [](double x) {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 30; k++) {
                term *= (x / (2 * k)) * (x / (2 * k));
                sum += term;
            }
            return sum;
        };
        std::vector<float> t((kRsPhases + 1) * kRsTaps);
        for (int ph = 0; ph <= kRsPhases; ph++) {
            double frac = (double)ph / kRsPhases;
            double sum = 0.0;
            for (int k = 0; k < kRsTaps; k++) {
                // Tap k reads input frame i + k - (kRsHalf - 1).
                double x = (double)(k - (kRsHalf - 1)) - frac;
                double sinc = x == 0.0 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
                double r = x / kRsHalf;
                double w = std::fabs(r) >= 1.0 ? 0.0 : bessel0(kRsKaiserBeta * std::sqrt(1.0 - r * r)) / bessel0(kRsKaiserBeta);
                t[ph * kRsTaps + k] = (float)(sinc * w);
                sum += sinc * w;
            }
            for (int k = 0; k < kRsTaps; k++) {
                t[ph * kRsTaps + k] = (float)(t[ph * kRsTaps + k] / sum);  // unity gain at DC
            }
        }
        return t;
    }();
    return table.data();
}
}

void PipeWireOutputStream::KeepResampleHistoryLocked(GstBuffer* in) {
    GstMapInfo map;
    if (!gst_buffer_map(in, &map, GST_MAP_READ)) {
        return;
    }
    const float* x = reinterpret_cast<const float*>(map.data);
    int n = (int)(map.size / kBytesPerFrame);
    int take = std::min(n, kRsHist);
    // Shift out what the new frames replace, then append them.
    int keep = std::min(m_rsHistFrames, kRsHist - take);
    std::memmove(m_rsHist, m_rsHist + (m_rsHistFrames - keep) * kChannels, keep * kChannels * sizeof(float));
    std::memcpy(m_rsHist + keep * kChannels, x + (n - take) * kChannels, take * kChannels * sizeof(float));
    m_rsHistFrames = keep + take;
    gst_buffer_unmap(in, &map);
}

GstBuffer* PipeWireOutputStream::ResampleLocked(GstBuffer* in) {
    static_assert(sizeof(m_rsHist) == kRsHist * kChannels * sizeof(float), "m_rsHist holds kRsHist frames");
    GstMapInfo im;
    if (!gst_buffer_map(in, &im, GST_MAP_READ)) {
        return nullptr;
    }
    const float* x = reinterpret_cast<const float*>(im.data);
    const long n = (long)(im.size / kBytesPerFrame);
    const double step = m_feedSpeed;
    const float* table = ResampleTable();
    // Input frame i of this buffer, or of the history for i in -kRsHist..-1
    // (a frame before the first of the track reads as silence).
    const int hist = m_rsHistFrames;
    auto at = [&](long i, int c) -> float {
        if (i >= 0) {
            return x[i * kChannels + c];
        }
        long h = hist + i;
        return h >= 0 ? m_rsHist[h * kChannels + c] : 0.0f;
    };
    size_t cap = (size_t)((double)(n + kRsHist) / step) + 4;
    GstBuffer* out = gst_buffer_new_allocate(nullptr, cap * kBytesPerFrame, nullptr);
    GstMapInfo om;
    if (!out || !gst_buffer_map(out, &om, GST_MAP_WRITE)) {
        gst_buffer_unmap(in, &im);
        if (out) {
            gst_buffer_unref(out);
        }
        return nullptr;
    }
    float* y = reinterpret_cast<float*>(om.data);
    size_t m = 0;
    double p = m_rsPos;
    float coef[kRsTaps];
    while (m < cap) {
        long i = (long)std::floor(p);
        if (i + kRsHalf > n - 1) {
            break;
        }
        double fph = (p - (double)i) * kRsPhases;
        int ph = std::min((int)fph, kRsPhases - 1);
        float fr = (float)(fph - ph);
        const float* c0 = table + ph * kRsTaps;
        const float* c1 = c0 + kRsTaps;
        long first = i - (kRsHalf - 1);
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
        if (first >= 0) {
            // Both channels four taps at a time: 1.7% of a BBB's core for
            // 44.1 kHz stereo, against 8.7% for the plain loop below (GCC
            // won't vectorize a float sum without -ffast-math).
            static_assert(kChannels == 2 && kRsTaps % 4 == 0, "the NEON path is stereo, four taps a step");
            const float* xs = x + first * kChannels;
            float32x4_t accL = vdupq_n_f32(0.0f);
            float32x4_t accR = vdupq_n_f32(0.0f);
            for (int k = 0; k < kRsTaps; k += 4) {
                float32x4_t a = vld1q_f32(c0 + k);
                float32x4_t co = vmlaq_n_f32(a, vsubq_f32(vld1q_f32(c1 + k), a), fr);
                float32x4x2_t lr = vld2q_f32(xs + 2 * k);
                accL = vmlaq_f32(accL, lr.val[0], co);
                accR = vmlaq_f32(accR, lr.val[1], co);
            }
            float32x2_t l2 = vadd_f32(vget_low_f32(accL), vget_high_f32(accL));
            float32x2_t r2 = vadd_f32(vget_low_f32(accR), vget_high_f32(accR));
            vst1_f32(y + m * kChannels, vpadd_f32(l2, r2));
            m++;
            p += step;
            continue;
        }
#endif
        for (int k = 0; k < kRsTaps; k++) {
            coef[k] = c0[k] + fr * (c1[k] - c0[k]);
        }
        for (int c = 0; c < kChannels; c++) {
            float acc = 0.0f;
            if (first >= 0) {
                const float* xs = x + first * kChannels + c;
                for (int k = 0; k < kRsTaps; k++) {
                    acc += coef[k] * xs[k * kChannels];
                }
            } else {
                for (int k = 0; k < kRsTaps; k++) {
                    acc += coef[k] * at(first + k, c);
                }
            }
            y[m * kChannels + c] = acc;
        }
        m++;
        p += step;
    }
    // At the exit p >= n - kRsHalf, so the next buffer reaches back no
    // further than the history's kRsHist frames.
    m_rsPos = p - (double)n;
    gst_buffer_unmap(out, &om);
    gst_buffer_unmap(in, &im);
    gst_buffer_set_size(out, (gssize)(m * kBytesPerFrame));
    KeepResampleHistoryLocked(in);
    return out;
}

void PipeWireOutputStream::ResetClockSyncLocked() {
    m_syncAnchored = false;
    m_syncWindow.clear();
}

// `trigger` is when the stream was last started (trigger_time), "" if the
// file has none: a restart changes it, hw_ptr wrapping doesn't.
static bool ReadCardPosition(const std::string& path, int64_t& hw, std::string& trigger) {
    std::string status = GetFileContents(path);
    if (status.find("state: RUNNING") == std::string::npos) {
        return false;
    }
    std::size_t p = status.find("hw_ptr");
    if (p == std::string::npos || (p = status.find(':', p)) == std::string::npos) {
        return false;
    }
    hw = strtoll(status.c_str() + p + 1, nullptr, 10);
    trigger.clear();
    std::size_t t = status.find("trigger_time");
    if (t != std::string::npos && (t = status.find(':', t)) != std::string::npos) {
        std::size_t e = status.find('\n', t);
        trigger = status.substr(t + 1, e == std::string::npos ? std::string::npos : e - t - 1);
        TrimWhiteSpace(trigger);
    }
    return true;
}

// Where the card's hw_ptr wraps back to 0 (sw_params "boundary"), 0 if it
// can't be read.  ALSA picks the largest power-of-two multiple of the buffer
// below LONG_MAX: 2^62 frames with a 64-bit kernel, but 2^30 with a 32-bit
// one (a BBB) -- about 6 h at 48 kHz, which a card kept running across songs
// reaches in a long show.
static int64_t ReadCardBoundary(const std::string& statusPath) {
    std::string text = GetFileContents(statusPath.substr(0, statusPath.rfind('/')) + "/sw_params");
    std::size_t p = text.find("boundary");
    if (p == std::string::npos || (p = text.find(':', p)) == std::string::npos) {
        return 0;
    }
    return strtoll(text.c_str() + p + 1, nullptr, 10);
}

bool PipeWireOutputStream::DisciplineClock() {
    std::string path;
    {
        std::unique_lock<std::mutex> lock(m_lock);
        if (!m_clock || m_state != State::Ready || m_cardStatusPath.empty()) {
            ResetClockSyncLocked();
            return false;
        }
        GstClockTime nowInt = gst_clock_get_internal_time(m_clock);
        if (m_syncAnchored && nowInt - m_syncLastInt < (GstClockTime)kClockSampleMs * GST_MSECOND) {
            return false;
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
            return false;
        }
    }
    // The card's position and the clock's internal time, as close together
    // as they can be read: the midpoint of the read.
    GstClockTime before = gst_clock_get_internal_time(m_clock);
    int64_t hw = 0;
    std::string trigger;
    bool running = ReadCardPosition(path, hw, trigger);
    GstClockTime after = gst_clock_get_internal_time(m_clock);
    GstClockTime internal = before + (after - before) / 2;

    std::unique_lock<std::mutex> lock(m_lock);
    if (path != m_cardStatusPath || m_state != State::Ready) {
        return false;
    }
    // The card stopping, or restarting, while a track feeds the stream is an
    // xrun: PipeWire restarted the device, and what the feed delivered
    // meanwhile is now queued behind it (see Flush()).
    //
    // A stop or restart right after one of our own flushes is the flush's
    // doing, not lost time.  Taken for an xrun, it was flushed again, which
    // restarted the card again: on a card that restarts on a flush, one xrun
    // -- or the flush before a track -- had the stream flushed every 250 ms
    // for as long as it played.
    bool lostTime = false;
    bool ownRestart = NowMs() - m_lastFlushMs < kFlushRestartMs;
    if (!running) {
        // The card stopped under a feed: an xrun.  Flushed only once the card
        // runs again.  Seen stopped, the graph usually still is (the stall
        // that caused it hasn't ended), and a flush then dropped only what was
        // queued so far: what the feed handed over for the rest of the stall
        // queued up behind it, and the sound stayed that far behind the lights
        // -- up to the whole stall, by where in it the card was read.
        if (m_syncAnchored && m_feed && !ownRestart) {
            m_xrunPending = true;
        }
        ResetClockSyncLocked();
        return false;
    }
    if (m_xrunPending) {
        m_xrunPending = false;
        lostTime = m_feed != nullptr;
    }
    // A restart resets hw_ptr and changes trigger_time.  hw_ptr also goes back
    // when it wraps at the ALSA boundary, with the card running on -- on a
    // 32-bit kernel every ~6 h -- and taking that for an xrun flushed a song
    // in mid-play.  A wrap is told apart by an unchanged trigger_time and an
    // advance across the boundary no longer than the time since the last
    // sample allows; the position is then carried on past it.
    bool restarted = m_syncAnchored && trigger != m_syncTrigger;
    if (m_syncAnchored && !restarted && hw < m_syncLastHw) {
        int64_t boundary = trigger.empty() ? 0 : ReadCardBoundary(path);
        double sinceS = (double)(internal - m_syncLastInt) / GST_SECOND;
        double advanceS = boundary > m_syncLastHw ? (double)(hw + boundary - m_syncLastHw) / m_syncCardRate : -1.0;
        if (advanceS >= 0.0 && advanceS <= sinceS + 1.0) {
            m_syncWrap += boundary;
            LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: card position wrapped at %lld\n",
                     m_slot, (long long)boundary);
        } else {
            restarted = true;
        }
    }
    m_syncTrigger = trigger;
    int64_t pos = hw + m_syncWrap;

    GstClockTime cInt, cExt, num, den;
    gst_clock_get_calibration(m_clock, &cInt, &cExt, &num, &den);
    GstClockTime external = gst_clock_adjust_with_calibration(m_clock, internal, cInt, cExt, num, den);

    double cardS = m_syncAnchored ? (double)(pos - m_syncHw0) / m_syncCardRate : 0.0;
    double clockS = m_syncAnchored ? (double)(GstClockTimeDiff)(external - m_syncExt0) / GST_SECOND : 0.0;
    double phase = cardS - clockS;  // > 0: the card is ahead of the clock
    if (!m_syncAnchored || restarted || std::fabs(phase) > kClockMaxPhaseError) {
        lostTime = lostTime || (m_syncAnchored && m_feed && (restarted ? !ownRestart : phase < -kClockMaxPhaseError));
        if (m_syncAnchored) {
            LogDebug(VB_MEDIAOUT, "PipeWire output stream %d: card clock re-anchored (phase %.1f ms)\n",
                     m_slot, phase * 1000.0);
        }
        // Start (again) from here, keeping the rate learnt so far.
        m_syncAnchored = true;
        m_syncHw0 = pos;
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
    RecalibrateFeedClockLocked(internal);
    LogExcess(VB_MEDIAOUT, "PipeWire output stream %d: card %+.1f ppm, phase %+.2f ms, clock %+.1f ppm\n",
              m_slot, (rate - 1.0) * 1e6, phase * 1000.0, (applied - 1.0) * 1e6);
    return lostTime;
}

bool PipeWireOutputStream::CardRunning() {
    if (m_cardStatusPath.empty()) {
        return false;
    }
    return GetFileContents(m_cardStatusPath).find("state: RUNNING") != std::string::npos;
}

void PipeWireOutputStream::CloseLocked(const char* why) {
#ifdef FPP_NATIVE_PIPEWIRE
    if (m_pwLoop) {
        if (m_pwStream) {
            // Closing when idle is routine -- a playlist that alternates
            // clips with sound and without closes and reopens the stream
            // at every pass -- so only another reason is worth a line at
            // the default level.
            if (strcmp(why, "idle") == 0) {
                LogDebug(VB_MEDIAOUT, "PipeWire output stream %d closing (%s)\n", m_slot, why);
            } else {
                LogInfo(VB_MEDIAOUT, "PipeWire output stream %d closing (%s)\n", m_slot, why);
            }
        }
        // Stopping the loop joins its thread, so OnProcess() is done with the
        // queue before anything below changes it.
        pw_thread_loop_lock(m_pwLoop);
        if (m_pwStream) {
            pw_stream_destroy(m_pwStream);
        }
        pw_thread_loop_unlock(m_pwLoop);
        pw_thread_loop_stop(m_pwLoop);
        pw_thread_loop_destroy(m_pwLoop);
    }
#endif
    m_pwStream = nullptr;
    m_pwLoop = nullptr;
    m_feedSpans.clear();
    gst_caps_replace(&m_caps, nullptr);
    m_rate = 0;
    EndTimelineLocked();
    m_continuedFirst = false;
    m_flushNow = false;
    m_xrunPending = false;
    // m_feed stays: it belongs to the track using the slot (AttachFeed() /
    // Release()), not to this stream.  Clearing it here orphaned a track
    // whose start triggered a reopen -- a new target -- and the whole track
    // played silent.  Its samples are dropped while there is no stream.
    m_target.clear();
    m_linked = false;
    m_state = State::Closed;
    m_stateChanged.notify_all();
}

#endif
