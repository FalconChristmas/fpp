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
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include "AES67Manager.h"
#include "DrmCards.h"
#include "OpusRTPManager.h"

class PixelOverlayModel;
class PipeWireOutputStream;

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
    // True once the reported position -- already backed up to what is leaving
    // the card -- has moved past the start of the media.
    virtual bool IsAudible(void) override { return m_audible; }

    /// Flush-seek to an absolute position in seconds.  Audible/visible glitch,
    /// so this is for repairing a stream that has genuinely left the show
    /// timeline, not for routine correction.  Returns false if the seek failed.
    bool SeekTo(float seconds);
    virtual void SetVolume(int volume) override;

    /// Ask the output's owner to stop it, from a thread that doesn't own it
    /// (a command, or the slot manager stopping a playlist's background
    /// slots).  Only the owner may Stop()/Close()/delete an output: the base
    /// version flags it for the next Process() on the owner's thread; Play
    /// Media's outputs override it to stop through their own registry.
    virtual void RequestStop() { m_stopRequested = true; }

    /// Whether StreamSlotManager must call Process() for this output.  False
    /// for outputs whose owner already does (a playlist entry, the remote's
    /// synced media); Play Media's outputs have no other pump.
    virtual bool NeedsSlotPump() const { return false; }

    // Static methods matching SDLOutput interface
    static bool IsOverlayingVideo();
    static bool ProcessVideoOverlay(unsigned int msTimestamp);
    static bool GetAudioSamples(float* samples, int numSamples, int& sampleRate);

    // One-time GStreamer + PipeWire env initialization (safe to call repeatedly)
    static void EnsureGStreamerInit();
    // Take `pipeline` to NULL on a reaper thread, waiting at most timeoutMs.
    // False if it wedged: the reaper keeps its own ref and the pipeline (with
    // whatever it holds) is left to it.  The caller's ref is untouched.
    static bool SetPipelineToNullBounded(GstElement* pipeline, int timeoutMs);
    // Set the environment now, on the calling thread, and run gst_init() in the
    // background so the first play doesn't pay for it (~1 s on an AM335x, and
    // ~20 s the very first time, when GStreamer builds its plugin registry).
    static void PreloadAsync();
    // Make every videoconvert in `pipeline` -- including ones added later --
    // read decoded frames from padded system memory rather than straight out
    // of a hardware decoder's buffers.  See the comment at the definition.
    static void GuardVideoConvertInputs(GstElement* pipeline);

    // The PipeWire sink a slot's audio is routed to ("" = PipeWire's default).
    static std::string PipeWireSinkNameForSlot(int slot);
    // The rate the PipeWire graph runs at (48000 if it can't be determined).
    static int PipeWireGraphRate();
    // /proc/asound status file of a card the PipeWire sink `sinkName` (a group
    // or an fpp_alsa_* adapter) plays into, or "" if it reaches no card -- a
    // group of network senders only, or PipeWire's unnamed default sink.
    static std::string AlsaSinkStatusPath(const std::string& sinkName);

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


    // GStreamer-specific
    void SetLoopCount(int loops) { m_loopCount = loops; }
    // Whether this output's position paces the running sequence (the channel
    // output servo).  True for the show's own media -- a playlist entry's, or
    // a remote's synced media -- and false for media played alongside it.
    void SetDrivesShowClock(bool drives) { m_drivesShowClock = drives; }
    void SetVolumeAdjustment(int volAdj);

    // Event callbacks (matching VLCOutput pattern).
    //
    // Playing(), Stopping() and Stopped() are called from Stop() and also from
    // BusSyncHandler, on the GStreamer streaming thread that delivered the
    // message, while that thread still holds the sink's internal locks.  An override must only
    // record state or hand work to the main loop (Timers): anything that
    // seeks, changes pipeline state, stops or closes an output -- including
    // running a command preset, which can do all of those -- deadlocks there.
    virtual void Starting() {}
    virtual void Playing() {}
    virtual void Stopping() {}
    virtual void Stopped() {}

private:
    // Serializes Start(), Stop() and Close() against each other and against
    // Process(), SeekTo() and AdjustSpeed(), which a slot's pump and MultiSync
    // run on the main loop while a command may be stopping the same output on
    // an HTTP thread.  The pump side only ever try-locks and skips a tick when
    // the output is busy, so it never waits here; holders may take
    // StreamSlotManager's lock, never the other way round.
    std::recursive_mutex m_opMutex;
    std::atomic<bool> m_stopRequested{false};

    // An optional video branch -- a secondary display's kmssink, or the
    // PipeWire video tap -- whose sink failed.  Its GST_FLOW_ERROR would
    // otherwise reach the tee and fail the whole pipeline, so the bus sync
    // handler cuts it off at the tee (CutFailedBranch, on the failing sink's
    // streaming thread) and Process() takes it out of the pipeline
    // (RemoveFailedBranches, op lock held).  Holds a ref on every object.
    struct FailedBranch {
        GstElement* tee = nullptr;
        GstPad* teePad = nullptr;
        std::vector<GstElement*> elements;  // sink first, back to the queue
    };
    std::mutex m_failedBranchLock;
    std::vector<FailedBranch> m_failedBranches;
    std::atomic<bool> m_haveFailedBranches{false};
    bool CutFailedBranch(GstElement* sink);
    // The head of an optional branch, between the tee and the branch's
    // queue: turns a sink's GST_FLOW_ERROR into OK, so even a buffer already
    // waiting on the branch's full queue can't carry it back to the tee.
    // nullptr where errorignore isn't installed.
    static GstElement* MakeBranchErrorGuard();
    void RemoveFailedBranches();
    void ReleaseFailedBranches();
    bool m_drivesShowClock = true;
    // Cuts the decodebin and bus callbacks off from this object and waits for
    // any that are running.  See CallbackGuard.
    void DetachCallbacks();
    // This output's id for its PixelOverlayManager model listener.
    std::string OverlayListenerId() const;

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
    // Set while the Start() thread has yet to seek to a non-zero start
    // position.  The pipeline prerolls at 0 first, and Process() must not
    // report that 0 as the media position (see Start()).  Shared for the same
    // reason as m_startThreadCancel.
    std::shared_ptr<std::atomic<bool>> m_startSeekPending;
    gulong m_appsinkSignalId = 0;            // audio appsink signal handler ID
    gulong m_videoAppsinkSignalId = 0;       // video appsink signal handler ID

    // Stall watchdog — detects when PipeWire sink stops consuming data
    gint64 m_lastPosition = -1;
    gint64 m_maxDuration = 0;   // highest observed duration (handles VBR fluctuations)
    uint64_t m_durationQueriedMs = 0;
    uint64_t m_stallStartMs = 0;
    uint64_t m_wallStartMs = 0;
    // A SeekTo() that arrived before the pipeline prerolled, applied by
    // Process() at the first position.  -1 when none is waiting.
    gint64 m_pendingSeekNs = -1;
    uint64_t m_lastWallLogMs = 0;

    // Audio still queued between pipewiresink's reported position and the DAC.
    // Cached: the depth barely moves while a track plays, and Process() runs at
    // frame rate.
    int64_t AudioSinkLatencyNs();
    // One-shot note that the position is going out uncorrected, and why.
    void NoteNoSinkLatency(const std::string& why);
    int64_t m_sinkLatencyNs = 0;
    uint64_t m_sinkLatencyCheckedMs = 0;
    std::string m_alsaStatusPath;
    std::string m_alsaHwParamsPath;
    int m_alsaRate = 0;
    bool m_loggedSinkLatency = false;
    bool m_warnedNoSinkLatency = false;
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
    // Guards the ring above and m_currentInstance, the one output allowed to
    // write it.
    static std::mutex s_sampleMutex;
    // When the tap owner last wrote the ring.  A paused or stalled owner
    // stops writing, and its last half second of audio must not read as
    // live.
    static uint64_t s_sampleLastWriteMs;
    static constexpr int SAMPLE_STALE_MS = 500;
    // Make `o` the tap owner and empty the ring.  s_sampleMutex held.
    static void SetSampleTapOwnerLocked(GStreamerOutput* o);
    static GstFlowReturn OnNewSample(GstAppSink* appsink, gpointer userData);
    // The tap's branch only carries audio while something is reading it.
    static constexpr int SAMPLE_TAP_IDLE_MS = 1000;
    static std::atomic<uint64_t> s_sampleTapReadMs;
    static GstPadProbeReturn GateSampleTap(GstPad* pad, GstPadProbeInfo* info, gpointer userData);

    // Video overlay for PixelOverlayModel (Phase 3)
    PixelOverlayModel* m_videoOverlayModel = nullptr;
    std::mutex m_videoOverlayModelLock;
    int m_videoOverlayWidth = 0;
    int m_videoOverlayHeight = 0;
    bool m_hasVideoStream = false;
    bool m_wasOverlayDisabled = false;

    // Latest video frame buffer — written by OnNewVideoSample, read by ProcessVideoOverlay
    std::vector<uint8_t> m_videoFrameData;
    int m_videoFrameWidth = 0;    // dimensions of m_videoFrameData (packed RGB)
    int m_videoFrameHeight = 0;
    std::mutex m_videoFrameMutex;
    bool m_videoFrameReady = false;
    uint64_t m_videoFramesReceived = 0;    // diagnostic counter
    uint64_t m_videoFramesDelivered = 0;   // diagnostic counter

    static GstFlowReturn OnNewVideoSample(GstAppSink* appsink, gpointer userData);

    // HDMI/DRM video output via kmssink (Phase 4)
    bool m_wantHDMI = false;               // true when outputting to HDMI via kmssink
    bool m_macVideoWindow = false;         // macOS: the "HDMI" output is this slot's video window
    GstElement* m_kmssink = nullptr;       // kmssink element (owned by pipeline bin)
    int m_hdmiConnectorId = -1;            // DRM connector ID from sysfs
    std::string m_hdmiCardPath;            // e.g. "/dev/dri/card1"
    // The DRM lease of each of this pipeline's kmssinks (the primary one and
    // any dkms_* branches).  Released in Close(), strictly after the pipeline
    // (and therefore every kmssink) has been unreffed.
    std::vector<DrmCards::Lease> m_drmLeases;
    // HDMI connectors this pipeline claimed from the video output manager's
    // persistent consumers (ClaimConnectorForMedia); released in Close()
    // after the leases, which brings those consumers back.
    std::vector<int> m_mediaConnectors;
    int m_hdmiDisplayWidth = 0;            // display resolution
    int m_hdmiDisplayHeight = 0;

    // PipeWire video routing — when PipeWireVideoSinkName is set, video is
    // tee'd to both kmssink (HDMI) and pipewiresink (PipeWire graph node).
    std::string m_pwVideoSinkName;                  // target-object for video pipewiresink
    bool m_videoPipeWireRouting = false;             // true when video goes through PipeWire
    std::set<int> m_directConnectorIds;                // HDMI connectors driven by direct kmssink on vtee

    // Dynamic pad linking for decodebin (video requires pad-added signal)
    //
    // decodebin emits these from its own streaming thread, on a pipeline that
    // can outlive this GStreamerOutput: Start() hands an owning pipeline ref to
    // a detached state-change thread, so Close()/~GStreamerOutput can complete
    // while decodebin is still exposing pads.  The callbacks therefore must not
    // dereference `this` directly.  Each Start() allocates a fresh guard whose
    // `self` Close() clears while holding `mtx`; a callback holding a shared_ptr
    // to it either runs to completion before that clear or sees nullptr and
    // returns.  Old pipelines keep their own (already cleared) guard, so a
    // restarted output is never reached through a stale connection.
    //
    // The bus sync handler runs on streaming threads too, and gets the same
    // guard -- but it does not hold `mtx` while it works.  It registers itself
    // in `busInFlight` instead, and Close() waits for that to drain after
    // clearing `self`.  Holding the lock across a handler would let one
    // streaming thread block another inside GStreamer.  `mtx` is recursive
    // because OnNoMorePads changes element state while holding it, and the
    // resulting bus messages reach BusSyncHandler on that same thread.
    struct CallbackGuard {
        std::recursive_mutex mtx;
        GStreamerOutput* self = nullptr;
        int busInFlight = 0;
        std::condition_variable_any busIdle;
    };
    std::shared_ptr<CallbackGuard> m_cbGuard;
    // Connects pad-added (and optionally no-more-pads) on `decoder`, passing a
    // guard reference as the closure data.  Takes no ref on `decoder`.
    void ConnectPadSignals(GstElement* decoder, bool wantNoMorePads);
    static void ReleaseCallbackGuard(gpointer data, GClosure* closure);
    // GDestroyNotify for the guard reference handed to gst_bus_set_sync_handler.
    static void ReleaseBusCallbackGuard(gpointer data);
    // Restarts the output from the top for its next loop.  Called from
    // BusSyncHandler on EOS, it does the seek on the main loop: a flushing
    // seek issued from the thread that delivered the EOS deadlocks, because
    // that thread still holds the sink's preroll lock and the flush needs it.
    static void ScheduleLoopSeek(const std::shared_ptr<CallbackGuard>& guard);
    // Resolves the closure data back to a live output.  Returns nullptr (with
    // `lock` left unlocked) once the owning Close() has run.
    static GStreamerOutput* LockCallbackGuard(gpointer userData,
                                              std::shared_ptr<CallbackGuard>& guard,
                                              std::unique_lock<std::recursive_mutex>& lock);
    // Keeps the output alive for one BusSyncHandler call.  `self` is nullptr
    // once Close() has run; otherwise Close() blocks until this is destroyed.
    struct BusCallbackScope {
        explicit BusCallbackScope(gpointer userData);
        ~BusCallbackScope();
        std::shared_ptr<CallbackGuard> guard;
        GStreamerOutput* self = nullptr;
    };
    static void OnPadAdded(GstElement* element, GstPad* pad, gpointer userData);
    static void OnNoMorePads(GstElement* element, gpointer userData);
    GstElement* m_audioChain = nullptr;    // audio sub-bin for pad linking
    GstElement* m_videoChain = nullptr;    // video sub-bin for pad linking
    bool m_audioLinked = false;            // true when audio pad was connected
    bool m_videoLinked = false;            // true when video pad was connected

    // True when Start() built the audio-only fallback (no video output for this
    // item), which is the one pipeline shape that cannot survive a file with no
    // playable audio -- see the no-audio handling in BusSyncHandler().
    bool m_audioOnlyPipeline = false;
    // Whether decodebin ever exposed a decoded audio pad.  Written on the
    // streaming thread from OnPadAdded, read on the bus thread.
    std::atomic<bool> m_sawAudioPad{false};
    // Set once the "file has no playable audio" case has been reported, so the
    // demuxer's follow-on "Internal data stream error" is not logged as a second,
    // unrelated-looking failure.
    std::atomic<bool> m_reportedNoAudio{false};

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

    // Audio-only PipeWire playback feeds the slot's persistent output stream
    // through this appsink instead of owning a pipewiresink (see
    // PipeWireOutputStream.h).  Both null on every other path.
    PipeWireOutputStream* m_pwStream = nullptr;
    GstElement* m_pwFeed = nullptr;
    void ReleasePipeWireStream();

    std::atomic<bool> m_audible{false};

    // The output whose audio feeds the WLED tap.  Only read or written
    // under s_sampleMutex, and never dereferenced by the tap's reader: the
    // owner can be closing on another thread.
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
