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

// RTSPOutputManager -- serves FPP's video (and, optionally, its audio) as
// muxed RTSP streams that a receiver can open by URL.
//
// This is the one place FPP deliberately recombines the two planes it
// otherwise keeps apart.  Internally video travels over intervideo channels
// and audio over the PipeWire graph, because show control is audio-clocked and
// the audio graph has to be a many-to-many mixer while video is one-to-many
// frame fanout.  Neither of those facts changes here: the planes are only
// brought back together at the very edge, where the wire format demands one
// container.  Everything upstream stays exactly as separate as it was.
//
// The existing RTP video output (VideoOutputManager, type "rtp") sends video
// alone to a fixed address and leaves the receiver to find a hand-written SDP
// file before it can decode anything.  RTSP replaces that with negotiation:
// point VLC, ffplay, OBS or a video wall at rtsp://<host>:<port><mount> and
// the server describes its own streams.  Because both branches live in one
// GstRTSPMedia they share a clock, so lip sync costs nothing extra.
//
// Config: $mediaDirectory/config/pipewire-rtsp-outputs.json

#if __has_include(<gst/rtsp-server/rtsp-server.h>)
#define HAS_RTSP_OUTPUT_GSTREAMER

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include "fpphttp_types.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace RTSPOutput
{

    constexpr int DEFAULT_PORT = 8554; // the IANA-registered RTSP alt port
    // gst-rtsp-server's own default is 200ms of buffering inside each media.
    // Our sources are live and already on this machine -- a camera, or a
    // sequence being played -- so that much slack buys nothing and is felt
    // directly as lag by whoever is watching. 40ms is a little over one frame
    // at 25fps: enough to absorb jitter between the capture and encode threads
    // without stacking up.
    constexpr int DEFAULT_LATENCY_MS = 40;
    constexpr int DEFAULT_WIDTH = 1280;
    constexpr int DEFAULT_HEIGHT = 720;
    constexpr int DEFAULT_FRAMERATE = 30;
    constexpr int DEFAULT_VIDEO_BITRATE = 4000;   // kbps, x264enc's unit
    constexpr int DEFAULT_AUDIO_BITRATE = 128000; // bps, opusenc's unit
    constexpr int AUDIO_RATE = 48000;             // Opus native rate
    constexpr int AUDIO_CHANNELS = 2;

    constexpr int VIDEO_PAYLOAD_TYPE = 96;
    constexpr int AUDIO_PAYLOAD_TYPE = 97;

} // namespace RTSPOutput

// One mount point on the server: rtsp://<host>:<port><mountPoint>
//
// Built from a Video Output Group member, not configured here. A group owns
// the video source and fans it out to its members, so an RTSP stream picks up
// whatever its group is showing -- the same source that group may also be
// putting on HDMI. Only the settings specific to serving it live on the
// member itself, exactly as the RTP member type carries address/port/encoding.
struct RTSPOutputMount {
    std::string name;
    std::string mountPoint = "/live";

    // intervideosrc channel to read. Filled from the group's videoSource, or
    // resolved from its stream slot -- see ResolveSourceNode().
    std::string sourceNode;
    std::vector<int> streamSlots;

    int width = RTSPOutput::DEFAULT_WIDTH;
    int height = RTSPOutput::DEFAULT_HEIGHT;
    int framerate = RTSPOutput::DEFAULT_FRAMERATE;
    std::string videoEncoding = "h264"; // "h264" | "h265" | "mjpeg"
    int videoBitrate = RTSPOutput::DEFAULT_VIDEO_BITRATE;

    bool audioEnabled = false;
    // PipeWire node.name the audio bridge publishes, so an Audio Output Group
    // can be pointed at it. Generated per group member by the web UI.
    std::string audioNodeName;
    int audioBitrate = RTSPOutput::DEFAULT_AUDIO_BITRATE;

    // interaudio channel carrying this mount's audio from the bridge pipeline
    // to the RTSP media. Derived from audioNodeName, never configured.
    std::string AudioChannel() const;

    // The channel to actually read. A group targeting a playback stream slot
    // rather than a named input source has no videoSource, and its slot maps
    // to a fixed intervideo channel (StreamSlotManager::GetVideoNodeName).
    // Returns "" when neither is set, i.e. nothing to serve.
    std::string ResolveSourceNode() const;
};

// Server-wide settings. The streams themselves are not here: they come from
// the Video Output Groups, so that a source is chosen in exactly one place.
struct RTSPOutputConfig {
    bool enabled = false;
    int port = RTSPOutput::DEFAULT_PORT;
    // Buffering inside each RTSP media, in milliseconds.  Lower means less
    // delay for the viewer; too low and a jittery source drops frames.
    int latencyMs = RTSPOutput::DEFAULT_LATENCY_MS;

    // Hold an audio bridge idle until something in the graph feeds it, for
    // exactly the reason OpusRTPConfig::requireGroupSource documents: the
    // bridge's pipewiresrc is created with node.autoconnect=false, so with
    // nothing linked in it cannot preroll and starting it burns 30 seconds
    // before failing. A mount whose audio is waiting still serves video.
    bool requireGroupSource = true;

    // Populated from pipewire-video-consumers.json, not from the server config.
    std::vector<RTSPOutputMount> mounts;
};

class RTSPOutputManager {
public:
    static RTSPOutputManager& INSTANCE;

    // HTTP API endpoint (registered at /rtspoutput)
    HttpResponsePtr render_GET(const HttpRequestPtr& req);

    bool Init();
    void Shutdown();

    /// Read the config and rebuild the server, its mounts and audio bridges.
    bool ApplyConfig();

    struct Status {
        struct MountStatus {
            std::string name;
            std::string mountPoint;
            std::string url;
            bool enabled = false;
            bool audioEnabled = false;
            // Audio is configured but its bridge was not started because
            // nothing in the graph feeds it -- guidance, not a failure.
            bool audioWaitingForSource = false;
            std::string audioNodeName;
            std::string note;
        };
        bool serverRunning = false;
        int port = 0;
        int latencyMs = 0;
        std::vector<MountStatus> mounts;
    };
    Status GetStatus();

    bool IsActive() const { return m_active.load(); }

    RTSPOutputManager();
    virtual ~RTSPOutputManager();

private:
    RTSPOutputConfig m_config;
    // Server settings (enabled/port) only.
    std::string m_configPath;
    // Where the streams come from: the flattened Video Output Group members
    // that VideoOutputManager also reads, filtered to type "rtsp".
    std::string m_consumersPath;
    bool LoadConfig();
    /// Fill cfg.mounts from the Video Output Group members (type "rtsp").
    void LoadMounts(RTSPOutputConfig& cfg);

    /// gst-launch description for one mount's GstRTSPMediaFactory.
    std::string BuildFactoryLaunch(const RTSPOutputMount& mount) const;

    /// Start/stop the always-present pipewiresrc -> interaudiosink bridge.
    bool StartAudioBridge(const RTSPOutputMount& mount);
    void StopAudioBridges();

    bool StartServer();
    void StopServer();

    // Guards m_config and m_mountStatus.  GetStatus() runs on an HTTP thread
    // while ApplyConfig() can be rebuilding both, the same race
    // OpusRTPManager::m_configMutex exists for.
    mutable std::mutex m_configMutex;
    // Serialises whole ApplyConfig()/Shutdown() cycles against each other.
    std::mutex m_applyMutex;

    std::atomic<bool> m_active{ false };

    GstRTSPServer* m_server = nullptr;
    guint m_serverSourceId = 0;
    GMainContext* m_mainContext = nullptr;
    GMainLoop* m_mainLoop = nullptr;
    std::thread m_loopThread;

    struct AudioBridge {
        std::string mountPoint;
        std::string nodeName;
        GstElement* pipeline = nullptr;
        bool waitingForSource = false;
    };
    std::vector<AudioBridge> m_audioBridges;
};

#endif // __has_include(<gst/rtsp-server/rtsp-server.h>)
