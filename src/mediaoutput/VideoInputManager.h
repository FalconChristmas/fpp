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
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#if __has_include(<gst/gst.h>)
#define HAS_GSTREAMER_VIDEO_INPUT
#include <gst/gst.h>
#endif

/**
 * VideoInputManager — manages persistent PipeWire video source pipelines.
 *
 * Each video input source runs a GStreamer producer pipeline:
 *   <source> → videoconvert → pipewiresink (mode=provide, Video/Source)
 *
 * Sources start at Init() and persist independently of media playback.
 * They appear as Video/Source nodes in the PipeWire graph.  Consumers
 * (VideoOutputManager) target them via the pipewiresrc target-object
 * property using the source's node name.
 *
 * Requires pipewiresink mode=provide support (PipeWire GStreamer plugin
 * >= 1.7.0 or equivalent patched build).
 *
 * Config file: $mediaDirectory/config/pipewire-video-input-sources.json
 */
class VideoInputManager {
public:
    static VideoInputManager& Instance();

    /// Load config and start enabled sources.
    void Init();

    /// Reload config — stops all sources, reloads, restarts enabled ones.
    void Reload();

    /// Stop all source pipelines and clean up.
    void Shutdown();

    /// Check if any sources are configured.
    bool HasSources() const;

    /// Check if any sources are currently running.
    bool HasActiveSources() const;

    /// Get count of running source pipelines.
    int ActiveSourceCount() const;

    /// Get the PipeWire node name for a source by its config ID.
    /// Returns empty string if not found or not running.
    std::string GetSourceNodeName(int sourceId) const;

    /// Get list of all configured source node names (for UI/API).
    std::vector<std::pair<int, std::string>> GetSourceList() const;

    /// Get the framerate of a source by its intervideo channel name.
    /// Returns the detected (or configured) fps, or 0 if not found.
    int GetSourceFramerate(const std::string& channelName) const;

private:
    VideoInputManager() = default;
    ~VideoInputManager();

    VideoInputManager(const VideoInputManager&) = delete;
    VideoInputManager& operator=(const VideoInputManager&) = delete;

    struct SourceInfo {
        int id = 0;
        std::string name;
        std::string type;            // "videotestsrc", "v4l2src", "rtspsrc", "urisrc", "rtpsrc"
        std::string pipeWireNodeName;
        bool enabled = true;

        // videotestsrc settings
        std::string pattern;         // "smpte", "snow", "black", etc.

        // v4l2src settings
        std::string device;          // "/dev/video0"

        // rtspsrc settings
        std::string uri;             // "rtsp://host/path"
        int latency = 200;           // ms

        // urisrc settings (uri field shared with rtspsrc)
        double bufferSec = 3.0;      // seconds of buffering

        // rtpsrc settings
        int port = 5004;             // UDP port
        std::string encoding;        // "H264", "H265", "MP2T", "RAW", "JPEG"
        std::string multicastGroup;  // optional multicast address

        // Common settings
        int width = 320;
        int height = 240;
        int framerate = 10;

        // Audio extraction settings
        bool audioEnabled = false;           // Extract and publish audio from this source
        std::string audioPipeWireNodeName;   // Generated: PipeWire audio source node name

#ifdef HAS_GSTREAMER_VIDEO_INPUT
        GstElement* pipeline = nullptr;
        GstElement* audioPipeline = nullptr;
#endif
        std::thread runThread;
        std::thread audioRunThread;
        std::atomic<bool> shutdownRequested{false};
        std::atomic<bool> audioShutdownRequested{false};
        bool running = false;
        int restartCount = 0;

        SourceInfo() = default;
        SourceInfo(SourceInfo&& o) noexcept
            : id(o.id), name(std::move(o.name)), type(std::move(o.type)),
              pipeWireNodeName(std::move(o.pipeWireNodeName)),
              enabled(o.enabled), pattern(std::move(o.pattern)),
              device(std::move(o.device)),
              uri(std::move(o.uri)), latency(o.latency),
              bufferSec(o.bufferSec),
              port(o.port), encoding(std::move(o.encoding)),
              multicastGroup(std::move(o.multicastGroup)),
              width(o.width), height(o.height), framerate(o.framerate),
              audioEnabled(o.audioEnabled),
              audioPipeWireNodeName(std::move(o.audioPipeWireNodeName)),
#ifdef HAS_GSTREAMER_VIDEO_INPUT
              pipeline(o.pipeline),
              audioPipeline(o.audioPipeline),
#endif
              runThread(std::move(o.runThread)),
              audioRunThread(std::move(o.audioRunThread)),
              shutdownRequested(o.shutdownRequested.load()),
              audioShutdownRequested(o.audioShutdownRequested.load()),
              running(o.running), restartCount(o.restartCount) {
#ifdef HAS_GSTREAMER_VIDEO_INPUT
            o.pipeline = nullptr;
            o.audioPipeline = nullptr;
#endif
        }
        SourceInfo(const SourceInfo&) = delete;
        SourceInfo& operator=(const SourceInfo&) = delete;
    };

    /// Load source config from JSON file.
    bool LoadConfig();

    /// Start a single source pipeline.
    bool StartSource(SourceInfo& source);

    /// Build and start pipeline with separate video + audio streams.
    bool StartSourceWithAudio(SourceInfo& source);

    /// Stop a single source pipeline.
    void StopSource(SourceInfo& source);

    /// Stop all sources.
    void StopAllSources();

    mutable std::mutex m_mutex;
    std::vector<SourceInfo> m_sources;
    bool m_initialized = false;
    // Guards Shutdown() so the destructor doesn't re-run it after main() already
    // did (the "Shutdown complete" was logged twice). Keeps teardown single-shot.
    std::atomic<bool> m_shutdownDone{false};

    static constexpr int MAX_RESTARTS = 3;
};
