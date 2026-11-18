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

#include "VideoDisplay.h"

#ifdef HAS_VIDEO_DISPLAY

#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <chrono>
#include <condition_variable>
#include <map>
#include <thread>
#include <vector>

#include "GStreamerOut.h"
#include "Warnings.h"
#include "common.h"
#include "log.h"

namespace {

// How long a video sink may be late before a frame is dropped: kmssink's own
// default (GstVideoSink), which the feed takes over along with the timing.
constexpr gint64 FEED_MAX_LATENESS_NS = 20 * GST_MSECOND;
// Bounds the display pipeline's NULL transition, as for a media pipeline.
constexpr int TEARDOWN_TIMEOUT_MS = 10000;
// The Pi's vc4 driver needs this long after a kmssink lets go of a connector
// before another can claim it ("general resource error" otherwise).
constexpr int DRM_RELEASE_DELAY_MS = 150;
// How long a display nothing uses stays up before it closes.  A next track
// that was prepared holds it throughout; one that wasn't (more than two audio
// channels, AES67 sending, a pause between entries, a prepare that expired)
// acquires it again only as it starts, and closing at once put the console
// up between them.  Short, so that what the display stands aside for -- a
// Virtual Matrix on the port, the console after the show -- comes back at
// once all the same.
constexpr int IDLE_LINGER_MS = 400;

std::mutex s_mapLock;
std::map<std::string, std::shared_ptr<VideoDisplay>> s_displays;
std::condition_variable s_wake;
bool s_workerStarted = false;
bool s_shutdown = false;
// Displays taken off s_displays and still being torn down on another thread
// (the worker, or a display replaced with other settings), which ShutdownAll()
// waits for: fppd marks every open descriptor close-on-exec once media is shut
// down, and a teardown still closing its lease then raced that.
int s_closing = 0;
std::condition_variable s_closed;

void DoneClosing() {
    {
        std::lock_guard<std::mutex> lock(s_mapLock);
        s_closing--;
    }
    s_closed.notify_all();
}

std::string KeyFor(const VideoDisplay::Config& cfg) {
    return cfg.cardPath + ":" + std::to_string(cfg.connectorId);
}

bool SameCrop(const VideoCropRect& a, const VideoCropRect& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height &&
           a.targetAspect == b.targetAspect;
}

// What an appsink's callback is handed: the display, kept alive for as long as
// the appsink holds the callbacks, and which feed the appsink is.
struct FeedRef {
    std::shared_ptr<VideoDisplay> display;
    uint64_t token;
};

void FreeFeedRef(gpointer data) {
    delete static_cast<FeedRef*>(data);
}

void FreeBusRef(gpointer data) {
    delete static_cast<std::weak_ptr<VideoDisplay>*>(data);
}

} // namespace

bool VideoDisplay::Config::SameOutput(const Config& o) const {
    return cardPath == o.cardPath && connectorId == o.connectorId && width == o.width &&
           height == o.height && SameCrop(crop, o.crop) && scaling == o.scaling;
}

VideoDisplay::VideoDisplay(const Config& cfg) :
    m_cfg(cfg) {
}

VideoDisplay::~VideoDisplay() {
    // Teardown() normally ran on the worker; this only catches a display that
    // never got that far (a failed Build()).
    if (!m_closed) {
        Teardown("destroyed");
    }
}

std::shared_ptr<VideoDisplay> VideoDisplay::Acquire(const Config& cfg) {
    GStreamerOutput::EnsureGStreamerInit();
    std::shared_ptr<VideoDisplay> stale;
    std::shared_ptr<VideoDisplay> display;
    std::shared_ptr<VideoDisplay> shared;
    {
        std::unique_lock<std::mutex> lock(s_mapLock);
        if (s_shutdown) {
            return nullptr;
        }
        if (!s_workerStarted) {
            s_workerStarted = true;
            std::thread(WorkerLoop).detach();
        }
        auto it = s_displays.find(KeyFor(cfg));
        if (it != s_displays.end()) {
            std::shared_ptr<VideoDisplay> cur = it->second;
            std::lock_guard<std::mutex> dl(cur->m_lock);
            if (!cur->m_failed && (cur->m_users > 0 || cur->m_cfg.SameOutput(cfg))) {
                if (!cur->m_cfg.SameOutput(cfg)) {
                    // A second slot on the same display, with a different crop
                    // or scaling.  It shows with the first one's until the
                    // display next closes.
                    LogWarn(VB_MEDIAOUT, "VideoDisplay %s: in use with other crop/scaling settings; keeping them\n",
                            cfg.connectorName.c_str());
                }
                cur->m_users++;
                shared = cur;
            } else {
                // Idle with other settings, or failed: build a new one in its place.
                stale = cur;
                s_displays.erase(it);
                s_closing++;
                // The new display's lease waits for this one's release (the
                // teardown below) rather than finding the connector held.
                DrmCards::RetireLease(stale->m_lease);
            }
        }
        if (!shared) {
            display = std::shared_ptr<VideoDisplay>(new VideoDisplay(cfg));
            display->m_users = 1;
            s_displays[KeyFor(cfg)] = display;
        }
    }
    if (shared) {
        // Another slot is still building it: wait, or its feed would get no
        // caps to keep to (m_feedCaps).
        bool usable;
        {
            std::unique_lock<std::mutex> dl(shared->m_lock);
            shared->m_builtCv.wait_for(dl, std::chrono::milliseconds(TEARDOWN_TIMEOUT_MS),
                                       [&shared]() { return !shared->m_building; });
            usable = !shared->m_building && !shared->m_closed && shared->m_feedCaps;
        }
        if (!usable) {
            shared->Release();
            return nullptr;
        }
        return shared;
    }
    if (stale) {
        // On its own thread: the new lease below waits for it.
        std::thread([stale]() {
            SetThreadName("FPP-VDispClose");
            stale->Teardown("settings changed");
            DoneClosing();
        }).detach();
    }
    bool built = display->Build();
    if (!built) {
        {
            std::lock_guard<std::mutex> lock(s_mapLock);
            auto it = s_displays.find(KeyFor(cfg));
            if (it != s_displays.end() && it->second == display) {
                s_displays.erase(it);
            }
        }
        {
            // Not this caller's any more; anyone waiting to share it (above)
            // finds it closed and lets go too.
            std::lock_guard<std::mutex> dl(display->m_lock);
            display->m_users--;
        }
        display->Teardown("build failed");
    }
    {
        std::lock_guard<std::mutex> dl(display->m_lock);
        display->m_building = false;
    }
    display->m_builtCv.notify_all();
    return built ? display : nullptr;
}

std::shared_ptr<VideoDisplay> VideoDisplay::AcquireRunning(const Config& cfg) {
    std::lock_guard<std::mutex> lock(s_mapLock);
    auto it = s_displays.find(KeyFor(cfg));
    if (s_shutdown || it == s_displays.end()) {
        return nullptr;
    }
    std::shared_ptr<VideoDisplay> cur = it->second;
    std::lock_guard<std::mutex> dl(cur->m_lock);
    if (cur->m_failed || cur->m_closed || cur->m_building || !cur->m_pipeline || !cur->m_cfg.SameOutput(cfg)) {
        return nullptr;
    }
    cur->m_users++;
    return cur;
}

void VideoDisplay::AddUser() {
    std::lock_guard<std::mutex> lock(m_lock);
    m_users++;
}

void VideoDisplay::Release() {
    bool last;
    bool releaseLease = false;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        last = m_users > 0 && --m_users == 0;
        if (last) {
            m_idleSinceMs = GetMonotonicTimeMS();
        }
        if (last && m_releaseLeaseWhenUnused) {
            m_releaseLeaseWhenUnused = false;
            releaseLease = true;
        }
    }
    if (releaseLease) {
        ReleaseLeaseAndConnector();
    }
    if (last) {
        // Under the map lock, which the worker checks under: never between
        // its look and its wait.
        std::lock_guard<std::mutex> lock(s_mapLock);
        s_wake.notify_all();
    }
}

bool VideoDisplay::Build() {
    VideoOutputManager::Instance().ClaimConnectorForMedia(m_cfg.connectorId);
    m_claimedConnector = true;

    GstElement* pipeline = gst_pipeline_new("vdisplay");
    GstElement* src = gst_element_factory_make("appsrc", "vdsrc");
    GstElement* sink = gst_element_factory_make("kmssink", "vdsink");
    if (!pipeline || !src || !sink) {
        LogErr(VB_MEDIAOUT, "VideoDisplay %s: could not create appsrc/kmssink (is gstreamer1.0-plugins-bad installed?)\n",
               m_cfg.connectorName.c_str());
        for (GstElement* e : { pipeline, src, sink }) {
            if (e) {
                gst_object_unref(gst_object_ref_sink(e));
            }
        }
        return false;
    }

    // Live, so the pipeline runs without waiting for a first frame, and never
    // blocking the track that pushes into it: if the display falls behind, the
    // older of two queued frames goes.
    g_object_set(src, "is-live", TRUE, "format", GST_FORMAT_TIME, "do-timestamp", FALSE,
                 "block", FALSE, "max-buffers", (guint64)2, "max-bytes", (guint64)0, NULL);
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(src), "leaky-type")) {
        g_object_set(src, "leaky-type", 2 /* GST_APP_LEAKY_TYPE_DOWNSTREAM */, NULL);
    }

    // The feed has already timed each frame: show it as it arrives.
    g_object_set(sink, "sync", FALSE, "async", FALSE, "qos", FALSE,
                 "enable-last-sample", FALSE, NULL);
    m_lease = DrmCards::AcquireLease(m_cfg.cardPath, m_cfg.connectorId);
    if (m_lease.fd >= 0) {
        g_object_set(sink, "fd", m_lease.fd, "connector-id", m_cfg.connectorId,
                     "restore-crtc", TRUE, "skip-vsync", TRUE, NULL);
        if (m_lease.planeId >= 0) {
            g_object_set(sink, "plane-id", m_lease.planeId, NULL);
        }
    } else {
        g_object_set(sink, "driver-name", "vc4", "connector-id", m_cfg.connectorId,
                     "restore-crtc", TRUE, "skip-vsync", TRUE, NULL);
    }

    // Crop and scaling, between the source and the sink: kmssink advertises
    // GstVideoCropMeta, so a crop right ahead of it costs nothing (see
    // VideoOutputManager::CreateCropElement()).
    std::vector<GstElement*> chain = { src };
    if (GstElement* crop = VideoOutputManager::CreateCropElement(m_cfg.crop, "vd_crop")) {
        chain.push_back(crop);
    }
    for (GstElement* e : VideoOutputManager::CreateScaleElements(m_cfg.scaling, m_cfg.width, m_cfg.height, "vd")) {
        chain.push_back(e);
    }
    chain.push_back(sink);
    bool ok = true;
    for (GstElement* e : chain) {
        ok = ok && gst_bin_add(GST_BIN(pipeline), e);
    }
    for (size_t i = 1; ok && i < chain.size(); i++) {
        ok = gst_element_link(chain[i - 1], chain[i]);
    }
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_pipeline = pipeline;
        m_src = GST_ELEMENT(gst_object_ref(src));
    }
    if (!ok) {
        LogErr(VB_MEDIAOUT, "VideoDisplay %s: could not link the display pipeline\n", m_cfg.connectorName.c_str());
        return false;
    }

    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_set_sync_handler(bus, OnBusMessage, new std::weak_ptr<VideoDisplay>(weak_from_this()), FreeBusRef);
    gst_object_unref(bus);

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        LogErr(VB_MEDIAOUT, "VideoDisplay %s: kmssink would not start\n", m_cfg.connectorName.c_str());
        return false;
    }

    // What the feeds must deliver: whatever the crop/scale chain and the
    // display's planes accept, now that kmssink has the display open.
    GstPad* srcPad = gst_element_get_static_pad(src, "src");
    GstCaps* caps = srcPad ? gst_pad_peer_query_caps(srcPad, nullptr) : nullptr;
    if (srcPad) {
        gst_object_unref(srcPad);
    }
    if (!caps || gst_caps_is_empty(caps)) {
        LogErr(VB_MEDIAOUT, "VideoDisplay %s: the display accepts no video format\n", m_cfg.connectorName.c_str());
        if (caps) {
            gst_caps_unref(caps);
        }
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_feedCaps = caps;
    }
    LogDebug(VB_MEDIAOUT, "VideoDisplay %s: running on %s (lease fd=%d plane=%d)\n", m_cfg.connectorName.c_str(),
             m_cfg.cardPath.c_str(), m_lease.fd, m_lease.planeId);
    return true;
}

void VideoDisplay::Teardown(const char* why) {
    GstElement* pipeline = nullptr;
    GstElement* src = nullptr;
    uint64_t shown = 0;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        if (m_closed) {
            return;
        }
        m_closed = true;
        m_feedToken = 0;
        pipeline = m_pipeline;
        src = m_src;
        m_pipeline = nullptr;
        m_src = nullptr;
        shown = m_framesShown;
        if (m_feedCaps) {
            gst_caps_unref(m_feedCaps);
            m_feedCaps = nullptr;
        }
        if (m_srcCaps) {
            gst_caps_unref(m_srcCaps);
            m_srcCaps = nullptr;
        }
    }
    LogDebug(VB_MEDIAOUT, "VideoDisplay %s: closing (%s) after %llu frames\n", m_cfg.connectorName.c_str(), why,
             (unsigned long long)shown);
    bool released = true;
    if (pipeline) {
        GstBus* bus = gst_element_get_bus(pipeline);
        gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
        gst_object_unref(bus);
        released = GStreamerOutput::SetPipelineToNullBounded(pipeline, TEARDOWN_TIMEOUT_MS);
        if (released) {
            std::this_thread::sleep_for(std::chrono::milliseconds(DRM_RELEASE_DELAY_MS));
        } else {
            LogErr(VB_MEDIAOUT, "VideoDisplay %s: kmssink would not stop; abandoning it with its lease\n",
                   m_cfg.connectorName.c_str());
        }
    }
    if (src) {
        gst_object_unref(src);
    }
    if (pipeline) {
        gst_object_unref(pipeline);
    }
    // After the kmssink is gone (closing a lease under a live one is
    // undefined), unless it never stopped: it still uses the lease then.
    if (released) {
        bool inUse;
        {
            std::lock_guard<std::mutex> lock(m_lock);
            inUse = m_users > 0;
            m_releaseLeaseWhenUnused = inUse;
        }
        if (inUse) {
            LogDebug(VB_MEDIAOUT, "VideoDisplay %s: keeping its lease until the tracks on it let go\n",
                     m_cfg.connectorName.c_str());
        } else {
            ReleaseLeaseAndConnector();
        }
    } else if (m_claimedConnector) {
        m_claimedConnector = false;
        VideoOutputManager::Instance().ReleaseConnectorFromMedia(m_cfg.connectorId);
    }
}

void VideoDisplay::ReleaseLeaseAndConnector() {
    DrmCards::ReleaseLease(m_lease);
    if (m_claimedConnector) {
        m_claimedConnector = false;
        VideoOutputManager::Instance().ReleaseConnectorFromMedia(m_cfg.connectorId);
    }
}

uint64_t VideoDisplay::AttachFeed(GstElement* appsink, bool activate) {
    uint64_t token;
    GstCaps* caps = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        token = ++m_lastToken;
        if (activate) {
            m_feedToken = token;
        }
        if (m_feedCaps) {
            caps = gst_caps_ref(m_feedCaps);
        }
    }
    // The timing kmssink would have applied, now applied here: sync on the
    // track's clock, drop what is too late, and report it upstream (QoS) so
    // the decoder can skip ahead.
    g_object_set(appsink, "sync", TRUE, "qos", TRUE, "max-lateness", FEED_MAX_LATENESS_NS,
                 "emit-signals", FALSE, "enable-last-sample", FALSE, "max-buffers", (guint)1, NULL);
    if (caps) {
        g_object_set(appsink, "caps", caps, NULL);
        gst_caps_unref(caps);
    }
    GstAppSinkCallbacks callbacks = {};
    callbacks.new_sample = [](GstAppSink* sink, gpointer data) -> GstFlowReturn {
        return OnFeedSample(GST_ELEMENT(sink), data);
    };
    gst_app_sink_set_callbacks(GST_APP_SINK(appsink), &callbacks,
                               new FeedRef{ shared_from_this(), token }, FreeFeedRef);
    // The decoder asks downstream how to allocate its frames.  The appsink
    // has no answer, so a hardware decoder allocated its minimum and, with
    // the display holding a few of them, ran short and fell back to copying
    // every frame into system memory -- which kmssink then copied again:
    // half a Pi 4 core and dropped frames for a 1080p30 H.264 video.  The
    // kmssink answers, as it did when it was in the track's own pipeline.
    if (GstPad* sinkPad = gst_element_get_static_pad(appsink, "sink")) {
        gst_pad_add_probe(sinkPad, GST_PAD_PROBE_TYPE_QUERY_DOWNSTREAM, OnFeedQuery,
                          new FeedRef{ shared_from_this(), token }, FreeFeedRef);
        gst_object_unref(sinkPad);
    }
    return token;
}

GstPadProbeReturn VideoDisplay::OnFeedQuery(GstPad*, GstPadProbeInfo* info, gpointer userData) {
    GstQuery* query = GST_PAD_PROBE_INFO_QUERY(info);
    if (!query || GST_QUERY_TYPE(query) != GST_QUERY_ALLOCATION) {
        return GST_PAD_PROBE_OK;
    }
    return static_cast<FeedRef*>(userData)->display->AnswerAllocation(query) ? GST_PAD_PROBE_HANDLED
                                                                             : GST_PAD_PROBE_OK;
}

bool VideoDisplay::AnswerAllocation(GstQuery* query) {
    GstElement* src = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        if (!m_src || m_failed) {
            return false;
        }
        src = GST_ELEMENT(gst_object_ref(m_src));
    }
    bool answered = false;
    if (GstPad* srcPad = gst_element_get_static_pad(src, "src")) {
        answered = gst_pad_peer_query(srcPad, query);
        gst_object_unref(srcPad);
    }
    gst_object_unref(src);
    return answered;
}

void VideoDisplay::ActivateFeed(uint64_t token) {
    std::lock_guard<std::mutex> lock(m_lock);
    m_feedToken = token;
}

void VideoDisplay::DetachFeed(uint64_t token) {
    std::lock_guard<std::mutex> lock(m_lock);
    if (m_feedToken == token) {
        m_feedToken = 0;
    }
}

GstFlowReturn VideoDisplay::OnFeedSample(GstElement* appsink, gpointer userData) {
    FeedRef* ref = static_cast<FeedRef*>(userData);
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
    if (sample) {
        ref->display->Push(ref->token, sample);
        gst_sample_unref(sample);
    }
    // Never an error: a display that can't show the frame must not end the
    // track, whose sound and lights go on.
    return GST_FLOW_OK;
}

void VideoDisplay::Push(uint64_t token, GstSample* sample) {
    GstBuffer* buf = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);
    if (!buf) {
        return;
    }
    GstElement* src = nullptr;
    GstCaps* newCaps = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_lock);
        if (token != m_feedToken || !m_src || m_failed) {
            return;
        }
        src = GST_ELEMENT(gst_object_ref(m_src));
        if (caps && (!m_srcCaps || !gst_caps_is_equal(caps, m_srcCaps))) {
            if (m_srcCaps) {
                gst_caps_unref(m_srcCaps);
            }
            m_srcCaps = gst_caps_ref(caps);
            newCaps = gst_caps_ref(caps);
        }
        m_framesShown++;
    }
    if (newCaps) {
        if (WillLog(LOG_DEBUG, VB_MEDIAOUT)) {
            gchar* str = gst_caps_to_string(newCaps);
            LogDebug(VB_MEDIAOUT, "VideoDisplay %s: frames now %s\n", m_cfg.connectorName.c_str(), str);
            g_free(str);
        }
        gst_app_src_set_caps(GST_APP_SRC(src), newCaps);
        gst_caps_unref(newCaps);
    }
    // The decoder's own buffer, held as long as the sink shows it -- as it
    // was when the sink was in the track's pipeline.  A copy (same memory,
    // new metadata) let the original go back to its pool at once: a hardware
    // decoder's pool could then hand its memory to the driver again while it
    // was on screen, and a pool that discards buffers whose memory is shared
    // allocated a new one for every frame.  The sink doesn't sync (it shows a
    // frame as it comes), so the track's own timestamps do no harm.
    gst_app_src_push_buffer(GST_APP_SRC(src), gst_buffer_ref(buf));
    gst_object_unref(src);
}

GstBusSyncReply VideoDisplay::OnBusMessage(GstBus*, GstMessage* msg, gpointer userData) {
    if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_ERROR) {
        return GST_BUS_DROP;
    }
    std::shared_ptr<VideoDisplay> self = static_cast<std::weak_ptr<VideoDisplay>*>(userData)->lock();
    if (!self) {
        return GST_BUS_DROP;
    }
    GError* err = nullptr;
    gchar* debug = nullptr;
    gst_message_parse_error(msg, &err, &debug);
    std::string reason = err ? GStreamerOutput::ErrorReason(err, debug) : "unknown error";
    LogErr(VB_MEDIAOUT, "VideoDisplay %s: %s\n", self->m_cfg.connectorName.c_str(), reason.c_str());
    LogDebug(VB_MEDIAOUT, "VideoDisplay %s debug: %s\n", self->m_cfg.connectorName.c_str(), debug ? debug : "(none)");
    // The same banner the track's own kmssink raised (ReportVideoOutputFailure()).
    std::string warning = VideoOutputManager::AspectMismatchWarning(self->m_cfg.connectorName, self->m_cfg.cardPath,
                                                                    self->m_cfg.connectorId, reason);
    if (warning.empty()) {
        warning = "Video output to " + self->m_cfg.connectorName + " failed (" + reason + ")";
    }
    WarningHolder::AddWarningTimeout(300, 74, warning); // Display Refused Video
    if (err) {
        g_error_free(err);
    }
    g_free(debug);
    {
        std::lock_guard<std::mutex> lock(self->m_lock);
        self->m_failed = true;
    }
    s_wake.notify_all();
    return GST_BUS_DROP;
}

void VideoDisplay::WorkerLoop() {
    SetThreadName("FPP-VDisplay");
    std::unique_lock<std::mutex> lock(s_mapLock);
    while (!s_shutdown) {
        std::vector<std::shared_ptr<VideoDisplay>> closing;
        uint64_t now = GetMonotonicTimeMS();
        uint64_t nextCheckMs = 1000;
        for (auto it = s_displays.begin(); it != s_displays.end();) {
            std::shared_ptr<VideoDisplay> d = it->second;
            std::lock_guard<std::mutex> dl(d->m_lock);
            bool idle = d->m_users == 0 && !d->m_building;
            if (idle && !d->m_failed && now - d->m_idleSinceMs < (uint64_t)IDLE_LINGER_MS) {
                // Not yet: a track starting next may still take it up.
                nextCheckMs = std::min<uint64_t>(nextCheckMs, IDLE_LINGER_MS - (now - d->m_idleSinceMs));
                ++it;
                continue;
            }
            if (idle || d->m_failed) {
                DrmCards::RetireLease(d->m_lease);
                closing.push_back(d);
                it = s_displays.erase(it);
                s_closing++;
                continue;
            }
            ++it;
        }
        if (!closing.empty()) {
            lock.unlock();
            for (auto& d : closing) {
                d->Teardown(d->m_failed ? "failed" : "idle");
                DoneClosing();
            }
            closing.clear();
            lock.lock();
            continue;
        }
        s_wake.wait_for(lock, std::chrono::milliseconds(nextCheckMs));
    }
}

void VideoDisplay::ShutdownAll() {
    std::vector<std::shared_ptr<VideoDisplay>> closing;
    {
        std::lock_guard<std::mutex> lock(s_mapLock);
        s_shutdown = true;
        for (auto& [key, d] : s_displays) {
            closing.push_back(d);
        }
        s_displays.clear();
    }
    s_wake.notify_all();
    for (auto& d : closing) {
        d->Teardown("shutdown");
    }
    // And any that were already closing.  Each teardown is itself bounded.
    std::unique_lock<std::mutex> lock(s_mapLock);
    if (!s_closed.wait_for(lock, std::chrono::milliseconds(TEARDOWN_TIMEOUT_MS + DRM_RELEASE_DELAY_MS + 1000),
                           [] { return s_closing == 0; })) {
        LogWarn(VB_MEDIAOUT, "VideoDisplay: %d display(s) still closing at shutdown\n", s_closing);
    }
}

#endif
