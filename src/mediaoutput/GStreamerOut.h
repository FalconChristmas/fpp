#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "MediaOutputBase.h"

#if __has_include(<gst/gst.h>)
#define HAS_GSTREAMER

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <array>
#include <atomic>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include "AES67Manager.h"
#include "OpusRTPManager.h"

class PixelOverlayModel;

class GStreamerOutput : public MediaOutputBase {
public:
    GStreamerOutput(const std::string& mediaFilename, MediaOutputStatus* status, const std::string& videoOut = "", int streamSlot = 1);
    virtual ~GStreamerOutput();

    virtual int Start(int msTime = 0) override;
    virtual int Stop(void) override;
    virtual int Process(void) override;
    virtual int Close(void) override;
    virtual int IsPlaying(void) override;
    virtual int AdjustSpeed(float masterPos) override;

    /// Flush-seek to an absolute position in seconds.  Audible/visible glitch,
    /// so this is for repairing a stream that has genuinely left the show
    /// timeline, not for routine correction.  Returns false if the seek failed.
    bool SeekTo(float seconds);
    virtual void SetVolume(int volume) override;

    // Static methods matching SDLOutput interface
    static bool IsOverlayingVideo();
    static bool ProcessVideoOverlay(unsigned int msTimestamp);
    static bool GetAudioSamples(float* samples, int numSamples, int& sampleRate);

    // One-time GStreamer + PipeWire env initialization (safe to call repeatedly)
    static void EnsureGStreamerInit();

    // Resolve connector name (e.g., "HDMI-A-1") to DRM card path, connector ID,
    // and display resolution by scanning sysfs.  Works on all Pi models.
    struct DrmConnectorInfo {
        std::string cardPath;     // e.g. "/dev/dri/card1"
        int connectorId = -1;    // integer ID for kmssink
        bool connected = false;
        int displayWidth = 0;
        int displayHeight = 0;
        int primaryPlaneId = -1; // DRM primary plane for this connector's CRTC
    };
    static DrmConnectorInfo ResolveDrmConnector(const std::string& connectorName);

    // Acquire a shared, refcounted DRM master fd for the given card (e.g.
    // "/dev/dri/card1").  All kmssink elements should use this fd to avoid
    // DRM master contention.  Opens the device and takes DRM master on the
    // first acquire for a given card; subsequent acquires just bump the
    // refcount and return the same fd.  Every successful call MUST be
    // matched by a later ReleaseSharedDrmFd() call for the same cardPath —
    // closing the fd once the refcount drops to zero is the kernel's
    // cleanup backstop that frees any GEM handles/framebuffers a kmssink
    // failed to release explicitly, which is what keeps CMA/GPU memory from
    // growing unbounded across many pipeline build/teardown cycles.
    static int AcquireSharedDrmFd(const std::string& cardPath);

    // Release a reference taken by AcquireSharedDrmFd().  When the refcount
    // for cardPath reaches zero the fd is closed (dropping DRM master and
    // freeing any leaked GEM handles/FBs).  Must only be called after the
    // pipeline holding kmssink(s) on that fd has been fully unreffed —
    // releasing (and possibly closing the fd) while a kmssink still holds
    // it open is undefined behavior.
    static void ReleaseSharedDrmFd(const std::string& cardPath);

    // Find a DRM OVERLAY plane for the CRTC currently bound to a connector.
    // Uses overlay planes (not primary) because fbcon holds the primary planes.
    // Tracks allocated planes to avoid collisions between consumers.
    // Returns -1 if not found.  Requires the shared DRM fd.
    static int FindPrimaryPlaneForConnector(int drmFd, int connectorId);

    // Return a plane previously handed out by FindPrimaryPlaneForConnector to
    // the free pool so it can be reused.  Must be called when the kmssink that
    // owned the plane is torn down, otherwise the allocated-plane set grows
    // until no overlay planes remain and HDMI video silently stops working.
    // A negative id or an id that was never allocated is ignored.
    static void ReleasePlane(int planeId);

    // GStreamer-specific
    void SetLoopCount(int loops) { m_loopCount = loops; }
    void SetVolumeAdjustment(int volAdj);

    // Event callbacks (matching VLCOutput pattern)
    virtual void Starting() {}
    virtual void Playing() {}
    virtual void Stopping() {}
    virtual void Stopped() {}

private:
    GstElement* m_pipeline = nullptr;
    GstElement* m_volume = nullptr;
    GstElement* m_appsink = nullptr;       // audio sample tap
    GstElement* m_videoAppsink = nullptr;   // video frame sink for PixelOverlay
    GstBus* m_bus = nullptr;
    std::string m_videoOut;
    int m_loopCount = 0;
    int m_volumeAdjust = 0;
    std::atomic<bool> m_playing{false};     // written from GStreamer thread (BusSyncHandler), read from main loop
    std::atomic<bool> m_shutdownFlag{false};  // guards callbacks during teardown
    bool m_teardownComplete = false;        // makes Stop() idempotent — the expensive pipeline teardown (silence flush + DRM release sleeps) must run only once even when Stop() is called both directly and via Close()/dtor
    // Set when the supervised pipeline-NULL transition timed out and the
    // pipeline was abandoned to a reaper thread (wedged V4L2 decoder, issue
    // #2695).  Close() must then leak the pipeline's DRM planes and shared
    // DRM fd refs — the still-alive kmssinks own them in the kernel.
    bool m_teardownAbandoned = false;
    // Cancellation token for the detached Start() PLAYING-transition thread.
    // Shared (shared_ptr) so the thread can safely outlive this object: Stop()
    // sets it to abort the volume ramp / deferred attach early, and the thread
    // captures only this token (never `this`), eliminating use-after-free if it
    // is still running when the object is torn down.
    std::shared_ptr<std::atomic<bool>> m_startThreadCancel;
    gulong m_appsinkSignalId = 0;            // audio appsink signal handler ID
    gulong m_videoAppsinkSignalId = 0;       // video appsink signal handler ID

    // Stall watchdog — detects when PipeWire sink stops consuming data
    gint64 m_lastPosition = -1;
    gint64 m_maxDuration = 0;   // highest observed duration (handles VBR fluctuations)
    uint64_t m_stallStartMs = 0;
    uint64_t m_wallStartMs = 0;
    uint64_t m_lastWallLogMs = 0;

    // Audio still queued between pipewiresink's reported position and the DAC.
    // Cached: the depth barely moves while a track plays, and Process() runs at
    // frame rate.
    int64_t AudioSinkLatencyNs();
    int64_t m_sinkLatencyNs = 0;
    uint64_t m_sinkLatencyCheckedMs = 0;
    std::string m_alsaStatusPath;
    std::string m_alsaHwParamsPath;
    int m_alsaRate = 0;
    bool m_loggedSinkLatency = false;
    uint64_t m_playStartMs = 0;   // wall time Start() set m_playing=true — anchors the preroll watchdog
    static constexpr int STALL_TIMEOUT_MS = 5000; // 5 seconds before declaring stall
    // Preroll watchdog: if the pipeline is set to PLAYING but never produces a
    // valid position (never actually starts), it is wedged in preroll/PAUSED —
    // e.g. the Pi's V4L2 hardware decoder can stop producing frames after a very
    // long video-only loop (issue #2695), so the video sink never prerolls and
    // the pipeline stays PAUSED forever.  The regular stall watchdog can't catch
    // this because it only runs once a position is available.  Generous timeout:
    // normal preroll completes in well under 1s even on slow HW decoders.
    static constexpr int PREROLL_TIMEOUT_MS = 15000;

    void ProcessMessages();
    static GstBusSyncReply BusSyncHandler(GstBus* bus, GstMessage* msg, gpointer userData);

    // Audio sample tap for WLED audio-reactive effects
    static constexpr int SAMPLE_BUFFER_SIZE = 4096; // circular buffer size
    static std::array<float, SAMPLE_BUFFER_SIZE> s_sampleBuffer;
    static int s_sampleWritePos;
    static int s_sampleRate;
    static std::mutex s_sampleMutex;
    static GstFlowReturn OnNewSample(GstAppSink* appsink, gpointer userData);

    // Video overlay for PixelOverlayModel (Phase 3)
    PixelOverlayModel* m_videoOverlayModel = nullptr;
    std::mutex m_videoOverlayModelLock;
    int m_videoOverlayWidth = 0;
    int m_videoOverlayHeight = 0;
    bool m_hasVideoStream = false;
    bool m_wasOverlayDisabled = false;

    // Latest video frame buffer — written by OnNewVideoSample, read by ProcessVideoOverlay
    std::vector<uint8_t> m_videoFrameData;
    std::mutex m_videoFrameMutex;
    bool m_videoFrameReady = false;
    uint64_t m_videoFramesReceived = 0;    // diagnostic counter
    uint64_t m_videoFramesDelivered = 0;   // diagnostic counter

    static GstFlowReturn OnNewVideoSample(GstAppSink* appsink, gpointer userData);

    // HDMI/DRM video output via kmssink (Phase 4)
    bool m_wantHDMI = false;               // true when outputting to HDMI via kmssink
    GstElement* m_kmssink = nullptr;       // kmssink element (owned by pipeline bin)
    int m_hdmiConnectorId = -1;            // DRM connector ID from sysfs
    std::string m_hdmiCardPath;            // e.g. "/dev/dri/card1"
    std::vector<int> m_allocatedPlanes;    // overlay planes reserved for this pipeline's kmssinks; released in Close()
    // Cards this pipeline called AcquireSharedDrmFd() on — one entry per
    // acquire call (the same card can be acquired more than once, e.g. one
    // primary kmssink plus several dkms_* consumer branches).  Released with
    // matching ReleaseSharedDrmFd() calls in Close(), strictly after the
    // pipeline (and therefore every kmssink) has been unreffed.
    std::vector<std::string> m_acquiredDrmCards;
    int m_hdmiDisplayWidth = 0;            // display resolution
    int m_hdmiDisplayHeight = 0;

    // PipeWire video routing — when PipeWireVideoSinkName is set, video is
    // tee'd to both kmssink (HDMI) and pipewiresink (PipeWire graph node).
    std::string m_pwVideoSinkName;                  // target-object for video pipewiresink
    bool m_videoPipeWireRouting = false;             // true when video goes through PipeWire
    std::set<int> m_directConnectorIds;                // HDMI connectors driven by direct kmssink on vtee

    // Dynamic pad linking for decodebin (video requires pad-added signal)
    static void OnPadAdded(GstElement* element, GstPad* pad, gpointer userData);
    static void OnNoMorePads(GstElement* element, gpointer userData);
    GstElement* m_audioChain = nullptr;    // audio sub-bin for pad linking
    GstElement* m_videoChain = nullptr;    // video sub-bin for pad linking
    bool m_audioLinked = false;            // true when audio pad was connected
    bool m_videoLinked = false;            // true when video pad was connected

#ifdef HAS_AES67_GSTREAMER
    // Zero-hop AES67 RTP branches attached to the audio tee (Phase 7.9)
    std::vector<AES67Manager::InlineRTPBranch> m_aes67Branches;
    void AttachAES67Branches(GstElement* tee);
    void DetachAES67Branches();
#endif

    // Flush PipeWire filter-chain delay buffers between songs
    static void FlushPipeWireDelayBuffers();

    // ── Phase 5: MultiSync rate adjustment (ported from VLCOut) ──
    bool m_allowSpeedAdjust = true;
    float m_currentRate = 1.0f;

    // Circular buffer of last MAX_DIFFS diff/rate pairs for trend detection
    static constexpr int MAX_DIFFS = 10;
    std::array<std::pair<int, float>, MAX_DIFFS> m_diffs{};
    int m_diffsSize = 0;
    int m_diffIdx = 0;
    int m_diffSum = 0;
    float m_rateSum = 0.0f;

    void pushDiff(int diff, float rate);

    int m_lastDiff = -1;   // init at -1 so speedup/slowdown logic initially assumes slightly behind master
    int m_rateDiff = 0;

    // Running rate average (smooths jitter)
    static constexpr int RATE_AVERAGE_COUNT = 20;
    std::list<float> m_lastRates;
    float m_lastRatesSum = 0.0f;

    // Apply playback rate change via GStreamer instant-rate-change seek
    void ApplyRate(float rate);

    int m_streamSlot = 1;  // stream slot number (1-5), determines PipeWire node name

    static GStreamerOutput* m_currentInstance;

    // Every output currently driving a PixelOverlay model.  The overlay push
    // is per-output, unlike the WLED audio tap, so it cannot hang off
    // m_currentInstance -- slot 1 always claims that, which left a companion
    // stream's model receiving nothing.
    static std::mutex m_overlayOutputsLock;
    static std::vector<GStreamerOutput*> m_overlayOutputs;
    bool PushVideoOverlayFrame();
};

#endif
