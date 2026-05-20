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

// OpusRTPManager -- GStreamer-based Opus RTP audio send/receive
//
// Provides WiFi-friendly compressed audio streaming using the Opus codec
// over RTP, as an alternative to AES67 (uncompressed L24) which requires
// wired Ethernet.  Supports both unicast and multicast destinations.
//
// Features:
//   - Send: pipewiresrc -> audioconvert -> opusenc -> rtpopuspay -> udpsink
//   - Receive: udpsrc -> rtpjitterbuffer -> rtpopusdepay -> opusdec -> audioconvert -> pipewiresink
//   - Configurable bitrate, DTX, in-band FEC
//   - Unicast or multicast destination
//   - Config format: pipewire-opus-rtp-instances.json

#if __has_include(<gst/gst.h>)
#define HAS_OPUS_RTP_GSTREAMER

#include <gst/gst.h>

#include "fpphttp.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// Opus RTP Protocol Constants
// ──────────────────────────────────────────────────────────────────────────────
namespace OpusRTP {

constexpr int RTP_PAYLOAD_TYPE      = 96;
constexpr int AUDIO_RATE            = 48000;     // Opus native rate
constexpr int DEFAULT_PORT          = 5005;      // avoid AES67's 5004
constexpr int DEFAULT_CHANNELS      = 2;
constexpr int DEFAULT_LATENCY_MS    = 50;        // higher default for WiFi
constexpr int DEFAULT_BITRATE       = 128000;    // 128 kbps
constexpr int DEFAULT_PACKET_LOSS   = 5;         // 5% expected loss
constexpr int MULTICAST_TTL         = 4;

constexpr const char* DEFAULT_DEST_IP = "239.69.1.1";

// Valid bitrates (bps)
inline bool IsValidBitrate(int br) {
    return br >= 6000 && br <= 512000;
}

} // namespace OpusRTP

// ──────────────────────────────────────────────────────────────────────────────
// Instance configuration -- mirrors the JSON schema
// ──────────────────────────────────────────────────────────────────────────────
struct OpusRTPInstance {
    int id = 0;
    std::string name;
    bool enabled = true;
    std::string mode = "send";               // "send" | "receive" | "both"
    std::string destIP = OpusRTP::DEFAULT_DEST_IP;   // unicast or multicast
    int port = OpusRTP::DEFAULT_PORT;
    int channels = OpusRTP::DEFAULT_CHANNELS;
    std::string interface;                   // network interface (e.g., "wlan0")
    int latency = OpusRTP::DEFAULT_LATENCY_MS;
    int bitrate = OpusRTP::DEFAULT_BITRATE;
    bool dtx = false;                        // Discontinuous Transmission
    bool fec = true;                         // In-band Forward Error Correction
    int packetLoss = OpusRTP::DEFAULT_PACKET_LOSS;  // expected packet loss %
};

struct OpusRTPConfig {
    std::vector<OpusRTPInstance> instances;
};

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline wrapper -- one per send or receive direction of an instance
// ──────────────────────────────────────────────────────────────────────────────
struct OpusRTPPipeline {
    int instanceId = 0;
    bool isSend = true;
    GstElement* pipeline = nullptr;
    GstBus* bus = nullptr;

    bool running = false;
    std::string errorMessage;

    // Buffer-drop probe for flush between tracks
    std::atomic<int> dropCounter = 0;
    GstPad* probePad = nullptr;
    gulong probeId = 0;

    // Zombie detection
    uint64_t lastByteCount = 0;
    int stallCount = 0;
};

// ──────────────────────────────────────────────────────────────────────────────
// OpusRTPManager -- singleton managing all Opus RTP GStreamer pipelines
// ──────────────────────────────────────────────────────────────────────────────
class OpusRTPManager {
public:
    static OpusRTPManager& INSTANCE;

    // HTTP API endpoint (registered at /opusrtp)
    HttpResponsePtr render_GET(const HttpRequestPtr& req);

    // Lifecycle
    bool Init();
    void Shutdown();

    // Apply configuration -- reads JSON and rebuilds pipelines
    bool ApplyConfig();

    // Cleanup -- stop all pipelines
    void Cleanup();

    // Status query
    struct Status {
        struct PipelineStatus {
            int instanceId;
            std::string name;
            std::string mode;
            bool running;
            std::string error;
        };
        std::vector<PipelineStatus> pipelines;
    };
    Status GetStatus();

    // Signal from fppd that PipeWire is ready
    void OnPipeWireReady();

    // Check if manager is active
    bool IsActive() const { return m_active.load(); }

    // Check for active send instances
    bool HasActiveSendInstances();

    // Pause/resume/flush send pipelines (media playback integration)
    void PauseSendPipelines();
    void ResumeSendPipelines();
    void FlushSendPipelines();

    OpusRTPManager();
    virtual ~OpusRTPManager();

private:
    // Config
    OpusRTPConfig m_config;
    std::string m_configPath;
    bool LoadConfig();

    // Pipeline watchdog -- polls bus messages and recovers crashed pipelines
    bool PollPipelinesWatchdog();

    // Watchdog thread
    std::thread m_watchdogThread;
    std::atomic<bool> m_watchdogRunning{false};
    void WatchdogLoop();

    // Pipeline management
    std::map<int, OpusRTPPipeline> m_sendPipelines;
    std::map<int, OpusRTPPipeline> m_recvPipelines;
    std::mutex m_pipelineMutex;

    bool CreateSendPipeline(const OpusRTPInstance& inst);
    bool CreateRecvPipeline(const OpusRTPInstance& inst);
    void StopPipeline(OpusRTPPipeline& p);
    void StopAllPipelines();

    // Get source IP for the network interface
    std::string GetInterfaceIP(const std::string& iface);

    // Check if destination IP is multicast
    static bool IsMulticast(const std::string& ip);

    // Safe node name
    static std::string SafeNodeName(const std::string& name);

    // State
    std::atomic<bool> m_active{false};
    std::atomic<bool> m_initialized{false};
};

#endif // HAS_OPUS_RTP_GSTREAMER
