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

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>

#if __has_include(<gst/gst.h>)
#define HAS_GSTREAMER_VIDEO_OUTPUT
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#endif

class PixelOverlayModel;

/**
 * A sub-region of the source frame, as fractions (0.0-1.0) of its width and
 * height.  Fractions rather than pixels so one configuration works for any
 * source resolution — the pixel crop is computed from the negotiated caps at
 * runtime, and re-computed if the source resolution changes mid-stream.
 *
 * Used to split one decoded video across several displays: give each output a
 * different region (e.g. left half / right half of a 3840x1080 file) and each
 * shows its own slice of a single decode.
 */
struct VideoCropRect {
    double x = 0.0;
    double y = 0.0;
    double width = 1.0;
    double height = 1.0;

    /// True when this selects the whole frame, i.e. no crop element is needed.
    bool IsFullFrame() const {
        return x <= 0.0 && y <= 0.0 && width >= 1.0 && height >= 1.0;
    }

    /// True when the values describe a usable region.  A zero/negative extent
    /// or one starting past the right/bottom edge would crop everything away.
    bool IsValid() const {
        return width > 0.0 && height > 0.0 && x >= 0.0 && y >= 0.0 &&
               (x + width) <= 1.0001 && (y + height) <= 1.0001;
    }
};

/**
 * VideoOutputManager — manages PipeWire video consumer pipelines.
 *
 * Each video output destination (HDMI display, pixel overlay, network stream)
 * runs a GStreamer consumer pipeline:
 *   pipewiresrc → videoconvert → videoscale → destination sink
 *
 * Consumer pipelines are started on-demand when a video producer appears
 * (GStreamerOut calls StartConsumers after pipewiresink is attached) and
 * stopped when playback ends (StopConsumers).  The pipewiresrc target-object
 * is set to the producer's node name so PipeWire links them automatically.
 *
 * Config file: $mediaDirectory/config/pipewire-video-consumers.json
 */
class VideoOutputManager {
public:
    static VideoOutputManager& Instance();

    /// Load config at startup.  Does NOT start consumer pipelines.
    void Init();

    /// Reload config (called when Apply is triggered from UI).
    /// If consumers are currently running, restarts them with new config.
    void Reload();

    /// Stop all consumer pipelines and clean up.
    void Shutdown();

    /// Info about an HDMI consumer for direct kmssink attachment.
    struct HdmiConsumerInfo {
        int connectorId = -1;
        std::string connector;   // e.g. "HDMI-A-2"
        std::string cardPath;
        int width = 0;
        int height = 0;
        int primaryPlaneId = -1; // DRM primary plane for this connector's CRTC
        VideoCropRect crop;      // region of the source this display shows
    };

    /// Return HDMI consumer info for consumers that match the given stream
    /// slot but whose connectorId is NOT in skipConnectorIds.
    std::vector<HdmiConsumerInfo> GetHdmiConsumers(
        int streamSlot,
        const std::set<int>& skipConnectorIds = {}) const;

    /// Region configured for the display on `connectorId`, whole frame if none.
    /// Unlike GetHdmiConsumers this does not skip the connector driven by the
    /// primary pipeline — that display is one half of a split too, so its
    /// kmssink needs the crop even though no consumer pipeline is started for
    /// it.  Matches the on-demand consumer entries and the stream-slot filter
    /// exactly as GetHdmiConsumers does.
    VideoCropRect GetHdmiCropForConnector(int streamSlot, int connectorId) const;

#ifdef HAS_GSTREAMER_VIDEO_OUTPUT
    /// Build a videocrop element that resolves `crop` against the negotiated
    /// caps at runtime.  Returns nullptr when the whole frame is selected (no
    /// element needed) or when the crop is invalid/videocrop is unavailable.
    /// The caller owns the floating ref — add it to a bin as usual.
    ///
    /// Place it immediately upstream of kmssink: kmssink advertises
    /// GstVideoCropMeta, so videocrop then crops by metadata (it becomes the
    /// DRM plane's source rectangle, applied by the display hardware during
    /// scanout) instead of copying pixels.  With any other element in between
    /// it silently falls back to a full per-frame copy.
    static GstElement* CreateCropElement(const VideoCropRect& crop,
                                         const std::string& name);
#endif

    /// Start all configured consumers targeting the given producer node.
    /// Called by GStreamerOut after pipewiresink is attached to PipeWire graph.
    /// directConnectorIds: HDMI connectors already driven by direct kmssink
    /// branches in the main pipeline — these will be skipped.
    void StartConsumers(const std::string& producerNodeName,
                        int primaryConnectorId = -1,
                        const std::set<int>& directConnectorIds = {});

    /// Stop all running consumer pipelines.
    /// Called by GStreamerOut when video playback ends.
    void StopConsumers();

    /// Called by VideoInputManager when a producer source pipeline reaches
    /// PLAYING and is about to deliver its first video frames.  Starts any
    /// persistent consumers that target the given intervideo channel.
    void NotifyProducerReady(const std::string& channelName);

    /// Suspend persistent HDMI consumers so GStreamerOut's pipeline can
    /// become DRM master.  Only one pipeline at a time can hold DRM master
    /// on a given DRM device (card1), so persistent consumers must yield.
    void SuspendPersistentHdmiConsumers();

    /// Resume persistent HDMI consumers after GStreamerOut's pipeline stops.
    void ResumePersistentHdmiConsumers();

    /// Check if any consumers are configured.
    bool HasConsumers() const;

    /// Check if any consumers are currently running.
    bool HasActiveConsumers() const;

    /// Get count of running consumer pipelines.
    int ActiveConsumerCount() const;

private:
    VideoOutputManager() = default;
    ~VideoOutputManager();

    VideoOutputManager(const VideoOutputManager&) = delete;
    VideoOutputManager& operator=(const VideoOutputManager&) = delete;

    /// Thread-safe overlay state shared between appsink callback and manager.
    struct OverlayState {
        std::mutex mtx;
        PixelOverlayModel* model = nullptr;
        int width = 0;
        int height = 0;
        bool wasDisabled = false;
        std::atomic<bool> active{false};
    };

    struct ConsumerInfo {
        int id = 0;
        std::string name;
        std::string type;           // "hdmi", "overlay", "rtp"
        std::string pipeWireNodeName;

        // HDMI-specific
        std::string connector;
        std::string cardPath;
        int connectorId = -1;
        int width = 0;
        int height = 0;
        std::string scaling;        // "fit", "fill", "stretch"
        VideoCropRect crop;         // region of the source shown on this output
        int assignedPlaneId = -1;   // DRM overlay plane reserved for this consumer's kmssink; released in StopConsumer
        // DRM cardPath this consumer's kmssink acquired via
        // GStreamerOutput::AcquireSharedDrmFd(); empty if none (non-HDMI
        // consumer, or the acquire failed).  Released with
        // ReleaseSharedDrmFd() in StopConsumer's teardown thread, strictly
        // after the pipeline (and its kmssink) has been unreffed.
        std::string acquiredDrmCard;

        // Overlay-specific
        std::string overlayModel;
        std::shared_ptr<OverlayState> overlayState;

        // RTP-specific
        std::string address;
        int port = 5004;
        std::string rtpEncoding;     // "raw", "h264", "h265", "mjpeg"

        // Source video stream (e.g. "fppd_video_stream_1")
        std::string sourceNode;

        // Stream slots this consumer should receive (empty = all slots)
        std::vector<int> streamSlots;

        // When >= 0, this consumer is part of a grouped pipeline and
        // the GstElement* pipeline is owned by m_consumers[groupPipelineOwnerIdx].
        // -1 means standalone (owns its own pipeline or not grouped).
        int groupPipelineOwnerIdx = -1;

#ifdef HAS_GSTREAMER_VIDEO_OUTPUT
        GstElement* pipeline = nullptr;
#endif
        std::thread startThread;
        // Shared (not raw-pointer-captured) so the bus-monitor thread spawned
        // in StartConsumer can safely outlive this ConsumerInfo: Reload()/
        // Shutdown() clear() m_consumers, which would invalidate a raw
        // `&consumer.shutdownRequested` pointer while the detached thread may
        // still be inside gst_bus_timed_pop().  The thread captures this
        // shared_ptr by value instead.  Re-assigned to a fresh instance each
        // time StartConsumer runs so a still-draining prior thread can never
        // have its flag reset out from under it by a later restart.
        std::shared_ptr<std::atomic<bool>> shutdownRequested = std::make_shared<std::atomic<bool>>(false);
        // Set by the bus-monitor thread when it exits due to
        // GST_MESSAGE_ERROR/EOS (i.e. the pipeline died on its own, not via
        // StopConsumer).  Without this, `running` stays true forever for
        // persistent (sourceNode) consumers and nothing ever restarts them —
        // a zombie that holds its DRM plane and CMA indefinitely.  Same
        // shared_ptr-per-run rationale as shutdownRequested.
        std::shared_ptr<std::atomic<bool>> pipelineDead = std::make_shared<std::atomic<bool>>(false);
        bool running = false;

        ConsumerInfo() = default;
        ConsumerInfo(ConsumerInfo&& o) noexcept
            : id(o.id), name(std::move(o.name)), type(std::move(o.type)),
              pipeWireNodeName(std::move(o.pipeWireNodeName)),
              connector(std::move(o.connector)), cardPath(std::move(o.cardPath)),
              connectorId(o.connectorId), width(o.width), height(o.height),
              scaling(std::move(o.scaling)), crop(o.crop),
              assignedPlaneId(o.assignedPlaneId),
              acquiredDrmCard(std::move(o.acquiredDrmCard)),
              overlayModel(std::move(o.overlayModel)),
              overlayState(std::move(o.overlayState)),
              address(std::move(o.address)), port(o.port),
              rtpEncoding(std::move(o.rtpEncoding)),
              sourceNode(std::move(o.sourceNode)),
              streamSlots(std::move(o.streamSlots)),
              groupPipelineOwnerIdx(o.groupPipelineOwnerIdx),
#ifdef HAS_GSTREAMER_VIDEO_OUTPUT
              pipeline(o.pipeline),
#endif
              startThread(std::move(o.startThread)),
              shutdownRequested(std::move(o.shutdownRequested)),
              pipelineDead(std::move(o.pipelineDead)),
              running(o.running) {
#ifdef HAS_GSTREAMER_VIDEO_OUTPUT
            o.pipeline = nullptr;
#endif
        }
        ConsumerInfo(const ConsumerInfo&) = delete;
        ConsumerInfo& operator=(const ConsumerInfo&) = delete;
    };

    /// Load consumer config from JSON file.
    bool LoadConfig();

    /// Start a single consumer pipeline.
    bool StartConsumer(ConsumerInfo& consumer);

    /// Start a group of HDMI consumers sharing the same PipeWire source
    /// as a single combined pipeline with tee.  PipeWire mode=provide
    /// doesn't fan-out buffers to multiple consumers; this works around
    /// that by using one pipewiresrc with a GStreamer tee.
    bool StartHdmiConsumerGroup(const std::vector<size_t>& indices);

    /// Stop a single consumer pipeline.
    void StopConsumer(ConsumerInfo& consumer);

    /// Stop all consumers.
    void StopAllConsumers();

    /// Write an SDP file for an RTP consumer so receivers (VLC, ffplay) can connect.
    void WriteSdpFile(const ConsumerInfo& consumer, const std::string& encoding);

    /// Build the SDP body for an RTP consumer.
    std::string BuildSdp(const ConsumerInfo& consumer, const std::string& encoding);

    /// Build a raw RFC 2974 SAP packet wrapping the given SDP.
    static std::vector<uint8_t> BuildSAPPacket(const std::string& sourceIP,
                                               uint16_t msgIdHash,
                                               const std::string& sdp,
                                               bool isDeletion = false);

    /// Start/stop the SAP multicast announcer thread.
    void StartSAPAnnouncer();
    void StopSAPAnnouncer();

    /// SAP announcer background loop.
    void SAPAnnounceLoop();

    std::thread m_sapThread;
    std::atomic<bool> m_sapRunning{false};

#ifdef HAS_GSTREAMER_VIDEO_OUTPUT
    /// Appsink callback for overlay consumers (runs on GStreamer streaming thread).
    static GstFlowReturn OnOverlaySample(GstAppSink* appsink, gpointer userData);
#endif

    /// Wait for pending pipeline teardown threads to finish (releases DRM/CMA).
    /// Returns true if all teardowns completed, false on timeout.
    bool WaitForPendingTeardowns(int timeoutMs);

    mutable std::mutex m_mutex;
    std::vector<ConsumerInfo> m_consumers;
    std::set<int> m_directConnectorIds;   // HDMI connectors handled by direct kmssink
    std::string m_activeProducer;  // node name of the current producer, empty if none
    int m_primaryConnectorId = -1; // DRM connector ID used by primary pipeline's kmssink
    bool m_initialized = false;
    // Guards Shutdown() so the destructor doesn't re-run it after main() already
    // did (the "Shutdown complete" was logged twice). Harmless today since it
    // only locks our own mutex, but keeps teardown single-shot and future-proof.
    std::atomic<bool> m_shutdownDone{false};

    // Pipeline teardown tracking — detached threads decrement the counter
    // and notify the CV when done, so callers can wait for CMA to be freed.
    std::mutex m_teardownMutex;
    std::condition_variable m_teardownCV;
    int m_pendingTeardowns = 0;
};
