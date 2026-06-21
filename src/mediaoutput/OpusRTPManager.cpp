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

#include "fpp-pch.h"

#include "OpusRTPManager.h"

#ifdef HAS_OPUS_RTP_GSTREAMER

#include <gst/gst.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <sstream>

#include "common.h"
#include "log.h"
#include "settings.h"

// ──────────────────────────────────────────────────────────────────────────────
// Singleton instance
// ──────────────────────────────────────────────────────────────────────────────
static OpusRTPManager s_opusRTPManager;
OpusRTPManager& OpusRTPManager::INSTANCE = s_opusRTPManager;

OpusRTPManager::OpusRTPManager() {
    m_configPath = getFPPMediaDir("/config/pipewire-opus-rtp-instances.json");
}

OpusRTPManager::~OpusRTPManager() {
    Shutdown();
}

// ──────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────────────────────────────────────
bool OpusRTPManager::Init() {
    if (m_initialized.load()) {
        return true;
    }

    // Opus RTP uses pipewiresrc/pipewiresink which require the full PipeWire graph.
    // Only proceed when MediaBackend is "pipewire" (the advanced PipeWire mode).
    // - ALSA / no backend: PipeWire daemon is not running; gst_parse_launch with
    //   pipewiresrc crashes in gst_value_deserialize.
    // - pipewire-simple: the graph lacks the node connections needed for audio
    //   format negotiation, causing the state change to block indefinitely.
    std::string mediaBackend = toLowerCopy(getSetting("MediaBackend"));
    if (mediaBackend != "pipewire") {
        LogDebug(VB_MEDIAOUT, "OpusRTPManager: MediaBackend='%s' (need 'pipewire'), skipping init\n",
                 mediaBackend.c_str());
        return true;
    }

    if (!FileExists(m_configPath)) {
        LogDebug(VB_MEDIAOUT, "OpusRTPManager: No config file at %s, skipping init\n",
                 m_configPath.c_str());
        return true;
    }

    if (!gst_is_initialized()) {
        gst_init(nullptr, nullptr);
    }

    // Set PipeWire env vars
    setenv("PIPEWIRE_RUNTIME_DIR", "/run/pipewire-fpp", 0);
    setenv("XDG_RUNTIME_DIR", "/run/pipewire-fpp", 0);
    setenv("PULSE_RUNTIME_PATH", "/run/pipewire-fpp/pulse", 0);

    m_initialized.store(true);
    LogInfo(VB_MEDIAOUT, "OpusRTPManager: Initialized\n");
    return true;
}

void OpusRTPManager::Shutdown() {
    if (!m_initialized.load()) {
        return;
    }

    LogInfo(VB_MEDIAOUT, "OpusRTPManager: Shutting down\n");

    m_watchdogRunning.store(false);
    if (m_watchdogThread.joinable()) {
        m_watchdogThread.join();
    }

    StopAllPipelines();

    m_active.store(false);
    m_initialized.store(false);
    LogInfo(VB_MEDIAOUT, "OpusRTPManager: Shutdown complete\n");
}

// ──────────────────────────────────────────────────────────────────────────────
// Config loading
// ──────────────────────────────────────────────────────────────────────────────
bool OpusRTPManager::LoadConfig() {
    Json::Value root;
    if (!LoadJsonFromFile(m_configPath, root)) {
        LogWarn(VB_MEDIAOUT, "OpusRTPManager: Failed to load config from %s\n",
                m_configPath.c_str());
        return false;
    }

    m_config.instances.clear();

    if (root.isMember("instances") && root["instances"].isArray()) {
        for (const auto& instJson : root["instances"]) {
            OpusRTPInstance inst;
            inst.id = instJson.get("id", 0).asInt();
            inst.name = instJson.get("name", "Opus RTP").asString();
            inst.enabled = instJson.get("enabled", true).asBool();
            inst.mode = instJson.get("mode", "send").asString();
            inst.destIP = instJson.get("destIP", OpusRTP::DEFAULT_DEST_IP).asString();
            inst.port = instJson.get("port", OpusRTP::DEFAULT_PORT).asInt();
            inst.channels = instJson.get("channels", OpusRTP::DEFAULT_CHANNELS).asInt();
            inst.interface = instJson.get("interface", "").asString();
            inst.latency = instJson.get("latency", OpusRTP::DEFAULT_LATENCY_MS).asInt();
            inst.bitrate = instJson.get("bitrate", OpusRTP::DEFAULT_BITRATE).asInt();
            inst.dtx = instJson.get("dtx", false).asBool();
            inst.fec = instJson.get("fec", true).asBool();
            inst.packetLoss = instJson.get("packetLoss", OpusRTP::DEFAULT_PACKET_LOSS).asInt();

            if (!OpusRTP::IsValidBitrate(inst.bitrate)) {
                inst.bitrate = OpusRTP::DEFAULT_BITRATE;
            }

            m_config.instances.push_back(inst);
        }
    }

    LogInfo(VB_MEDIAOUT, "OpusRTPManager: Loaded config with %d instances\n",
            (int)m_config.instances.size());
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// ApplyConfig
// ──────────────────────────────────────────────────────────────────────────────
bool OpusRTPManager::ApplyConfig() {
    if (!m_initialized.load()) {
        if (!Init()) {
            return false;
        }
        // Init() may return true but skip initialization (e.g. wrong backend).
        // If m_initialized is still false, there's nothing to do.
        if (!m_initialized.load()) {
            return true;
        }
    }

    // Stop existing watchdog and pipelines
    m_watchdogRunning.store(false);
    if (m_watchdogThread.joinable()) {
        m_watchdogThread.join();
    }
    StopAllPipelines();

    if (!FileExists(m_configPath)) {
        LogInfo(VB_MEDIAOUT, "OpusRTPManager: No config file, nothing to apply\n");
        m_active.store(false);
        return true;
    }

    if (!LoadConfig()) {
        return false;
    }

    int enabledCount = 0;
    for (const auto& inst : m_config.instances) {
        if (inst.enabled) enabledCount++;
    }

    if (enabledCount == 0) {
        LogInfo(VB_MEDIAOUT, "OpusRTPManager: No enabled instances\n");
        m_active.store(false);
        return true;
    }

    // Create pipelines for each enabled instance
    for (const auto& inst : m_config.instances) {
        if (!inst.enabled) continue;

        bool wantSend = (inst.mode == "send" || inst.mode == "both");
        bool wantRecv = (inst.mode == "receive" || inst.mode == "both");

        if (wantSend) {
            CreateSendPipeline(inst);
        }

        if (wantRecv) {
            CreateRecvPipeline(inst);
        }
    }

    // Start watchdog thread
    m_watchdogRunning.store(true);
    m_watchdogThread = std::thread(&OpusRTPManager::WatchdogLoop, this);

    m_active.store(true);
    LogInfo(VB_MEDIAOUT, "OpusRTPManager: Applied config -- %d send, %d receive pipelines\n",
            (int)m_sendPipelines.size(), (int)m_recvPipelines.size());
    return true;
}

void OpusRTPManager::Cleanup() {
    LogInfo(VB_MEDIAOUT, "OpusRTPManager: Cleanup\n");

    m_watchdogRunning.store(false);
    if (m_watchdogThread.joinable()) {
        m_watchdogThread.join();
    }

    StopAllPipelines();
    m_active.store(false);
}

void OpusRTPManager::OnPipeWireReady() {
    if (FileExists(m_configPath)) {
        LogInfo(VB_MEDIAOUT, "OpusRTPManager: PipeWire ready, applying config\n");
        ApplyConfig();
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────────────────
bool OpusRTPManager::IsMulticast(const std::string& ip) {
    struct in_addr addr;
    if (inet_pton(AF_INET, ip.c_str(), &addr) != 1) {
        return false;
    }
    // Multicast range: 224.0.0.0 - 239.255.255.255
    uint32_t hostAddr = ntohl(addr.s_addr);
    return (hostAddr >= 0xE0000000 && hostAddr <= 0xEFFFFFFF);
}

std::string OpusRTPManager::SafeNodeName(const std::string& name) {
    std::string result = "opusrtp_";
    for (char c : name) {
        if (std::isalnum(c) || c == '_') {
            result += std::tolower(c);
        } else {
            result += '_';
        }
    }
    return result;
}

std::string OpusRTPManager::GetInterfaceIP(const std::string& iface) {
    struct ifaddrs* addrs = nullptr;
    std::string result = "0.0.0.0";

    if (getifaddrs(&addrs) != 0) {
        return result;
    }

    for (struct ifaddrs* ifa = addrs; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
        if (iface.empty() || iface == ifa->ifa_name) {
            if (std::string(ifa->ifa_name) == "lo") continue;
            struct sockaddr_in* addr = (struct sockaddr_in*)ifa->ifa_addr;
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
            result = buf;
            if (!iface.empty()) break;
        }
    }

    freeifaddrs(addrs);
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline creation -- Send
// ──────────────────────────────────────────────────────────────────────────────
bool OpusRTPManager::CreateSendPipeline(const OpusRTPInstance& inst) {
    std::string nodeName = SafeNodeName(inst.name) + "_send";
    bool multicast = IsMulticast(inst.destIP);

    // Pipeline:
    //   pipewiresrc stream-properties="props,node.name=<node>,node.autoconnect=false"
    //   ! audioconvert
    //   ! audio/x-raw,rate=48000,channels=N
    //   ! opusenc bitrate=<bps> dtx=<bool> inband-fec=<bool> packet-loss-percentage=<int>
    //   ! rtpopuspay pt=96
    //   ! udpsink host=<ip> port=<port> [multicast options]

    std::ostringstream oss;
    oss << "pipewiresrc name=pwsrc min-buffers=2 do-timestamp=true "
        << "! queue max-size-buffers=0 max-size-bytes=0 max-size-time=200000000 leaky=downstream "
        << "! audioconvert "
        << "! audiorate "
        << "! audio/x-raw,rate=" << OpusRTP::AUDIO_RATE
        << ",channels=" << inst.channels << " "
        << "! opusenc bitrate=" << inst.bitrate
        << " frame-size=20"
        << " dtx=" << (inst.dtx ? "true" : "false")
        << " inband-fec=" << (inst.fec ? "true" : "false")
        << " packet-loss-percentage=" << inst.packetLoss << " "
        << "! rtpopuspay pt=" << OpusRTP::RTP_PAYLOAD_TYPE << " "
        << "! udpsink name=usink host=" << inst.destIP
        << " port=" << inst.port
        << " sync=false";

    if (multicast) {
        oss << " ttl-mc=" << OpusRTP::MULTICAST_TTL
            << " auto-multicast=true";
        if (!inst.interface.empty()) {
            oss << " multicast-iface=" << inst.interface;
        }
    }

    std::string pipelineStr = oss.str();
    LogInfo(VB_MEDIAOUT, "Opus RTP send pipeline [%d] %s: %s\n",
            inst.id, inst.name.c_str(), pipelineStr.c_str());

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
    if (error) {
        LogErr(VB_MEDIAOUT, "Opus RTP send pipeline error [%d]: %s\n",
               inst.id, error->message);
        g_error_free(error);
        return false;
    }

    // Set stream-properties post-launch — inline GstStructure values
    // containing '/' (e.g. node.latency=960/48000) crash gst_value_deserialize.
    GstElement* pwsrc = gst_bin_get_by_name(GST_BIN(pipeline), "pwsrc");
    if (pwsrc) {
        GstStructure* props = gst_structure_new("props",
            "node.name", G_TYPE_STRING, nodeName.c_str(),
            "node.autoconnect", G_TYPE_BOOLEAN, FALSE,
            "node.latency", G_TYPE_STRING, "960/48000",
            NULL);
        g_object_set(pwsrc, "stream-properties", props, NULL);
        gst_structure_free(props);
        gst_object_unref(pwsrc);
    }

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LogErr(VB_MEDIAOUT, "Opus RTP send pipeline [%d] failed to start\n", inst.id);
        WarningHolder::AddWarning(44, "Opus RTP: audio send stream failed to start");
        gst_object_unref(bus);
        gst_object_unref(pipeline);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_pipelineMutex);
        m_sendPipelines.erase(inst.id);
        auto [it, ok] = m_sendPipelines.try_emplace(inst.id);
        it->second.instanceId = inst.id;
        it->second.isSend = true;
        it->second.pipeline = pipeline;
        it->second.bus = bus;
        it->second.running = true;
    }

    LogInfo(VB_MEDIAOUT, "Opus RTP send pipeline [%d] %s started -> %s:%d (%dch, %d kbps%s)\n",
            inst.id, inst.name.c_str(), inst.destIP.c_str(), inst.port,
            inst.channels, inst.bitrate / 1000,
            multicast ? ", multicast" : ", unicast");
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline creation -- Receive
// ──────────────────────────────────────────────────────────────────────────────
bool OpusRTPManager::CreateRecvPipeline(const OpusRTPInstance& inst) {
    std::string nodeName = SafeNodeName(inst.name) + "_recv";
    bool multicast = IsMulticast(inst.destIP);

    // Pipeline:
    //   udpsrc [multicast-group=<ip>] port=<port> [auto-multicast=true] [multicast-iface=<iface>]
    //   ! application/x-rtp,media=audio,encoding-name=OPUS,payload=96
    //   ! rtpjitterbuffer latency=<ms>
    //   ! rtpopusdepay
    //   ! opusdec
    //   ! audioconvert
    //   ! pipewiresink stream-properties="props,media.class=Audio/Source,node.name=<node>"

    std::ostringstream oss;
    oss << "udpsrc";

    if (multicast) {
        oss << " multicast-group=" << inst.destIP;
    }

    oss << " port=" << inst.port;

    if (multicast) {
        oss << " auto-multicast=true";
        if (!inst.interface.empty()) {
            oss << " multicast-iface=" << inst.interface;
        }
    }

    oss << " ! application/x-rtp,media=audio,encoding-name=OPUS"
        << ",payload=" << OpusRTP::RTP_PAYLOAD_TYPE << " "
        << "! rtpjitterbuffer latency=" << inst.latency << " "
        << "! rtpopusdepay "
        << "! opusdec plc=true "
        << "! audioconvert "
        << "! audio/x-raw,rate=" << OpusRTP::AUDIO_RATE << ",format=F32LE "
        << "! pipewiresink name=pwsink";

    std::string pipelineStr = oss.str();
    LogInfo(VB_MEDIAOUT, "Opus RTP recv pipeline [%d] %s: %s\n",
            inst.id, inst.name.c_str(), pipelineStr.c_str());

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
    if (error) {
        LogErr(VB_MEDIAOUT, "Opus RTP recv pipeline error [%d]: %s\n",
               inst.id, error->message);
        g_error_free(error);
        return false;
    }

    // Set stream-properties post-launch — gst_parse_launch cannot
    // deserialize GstStructure values containing '/' (e.g. Audio/Source)
    // which crashes gst_value_deserialize.
    std::string recvNodeDesc = SafeNodeName(inst.name) + "_recv";
    GstElement* pwsink = gst_bin_get_by_name(GST_BIN(pipeline), "pwsink");
    if (pwsink) {
        GstStructure* props = gst_structure_new("props",
            "media.class", G_TYPE_STRING, "Audio/Source",
            "node.name", G_TYPE_STRING, nodeName.c_str(),
            "node.description", G_TYPE_STRING, recvNodeDesc.c_str(),
            NULL);
        g_object_set(pwsink, "stream-properties", props, NULL);
        gst_structure_free(props);
        gst_object_unref(pwsink);
    }

    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));

    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LogErr(VB_MEDIAOUT, "Opus RTP recv pipeline [%d] failed to start\n", inst.id);
        WarningHolder::AddWarning(44, "Opus RTP: audio receive stream failed to start");
        gst_object_unref(bus);
        gst_object_unref(pipeline);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_pipelineMutex);
        m_recvPipelines.erase(inst.id);
        auto [it, ok] = m_recvPipelines.try_emplace(inst.id);
        it->second.instanceId = inst.id;
        it->second.isSend = false;
        it->second.pipeline = pipeline;
        it->second.bus = bus;
        it->second.running = true;
    }

    LogInfo(VB_MEDIAOUT, "Opus RTP recv pipeline [%d] %s started <- %s:%d (%dch, %dms latency)\n",
            inst.id, inst.name.c_str(), inst.destIP.c_str(), inst.port,
            inst.channels, inst.latency);
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline management
// ──────────────────────────────────────────────────────────────────────────────
void OpusRTPManager::StopPipeline(OpusRTPPipeline& p) {
    if (p.pipeline) {
        gst_element_set_state(p.pipeline, GST_STATE_NULL);
        if (p.bus) {
            gst_object_unref(p.bus);
            p.bus = nullptr;
        }
        gst_object_unref(p.pipeline);
        p.pipeline = nullptr;
        p.running = false;
    }
}

void OpusRTPManager::StopAllPipelines() {
    std::map<int, OpusRTPPipeline> sendCopy, recvCopy;
    {
        std::lock_guard<std::mutex> lock(m_pipelineMutex);
        sendCopy.swap(m_sendPipelines);
        recvCopy.swap(m_recvPipelines);
    }

    for (auto& [id, p] : sendCopy) {
        LogDebug(VB_MEDIAOUT, "OpusRTPManager: Stopping send pipeline [%d]\n", id);
        StopPipeline(p);
    }

    for (auto& [id, p] : recvCopy) {
        LogDebug(VB_MEDIAOUT, "OpusRTPManager: Stopping recv pipeline [%d]\n", id);
        StopPipeline(p);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Pause / Resume / Flush
// ──────────────────────────────────────────────────────────────────────────────
void OpusRTPManager::PauseSendPipelines() {
    // No-op: with node.autoconnect=false the send pipeline always outputs.
    // When no media is playing, the filter-chain delivers clean silence.
}

void OpusRTPManager::ResumeSendPipelines() {
    // No-op: pipeline is always sending.
}

// Pad probe callback: drops buffers while dropCounter > 0
static GstPadProbeReturn OpusDropBufferProbe(GstPad* pad, GstPadProbeInfo* info, gpointer userData) {
    std::atomic<int>* counter = static_cast<std::atomic<int>*>(userData);
    if (!(info->type & GST_PAD_PROBE_TYPE_BUFFER))
        return GST_PAD_PROBE_OK;
    if (*counter <= 0)
        return GST_PAD_PROBE_OK;
    (*counter)--;
    return GST_PAD_PROBE_DROP;
}

void OpusRTPManager::FlushSendPipelines() {
    std::lock_guard<std::mutex> lock(m_pipelineMutex);
    for (auto& kv : m_sendPipelines) {
        OpusRTPPipeline& p = kv.second;
        if (!p.pipeline || !p.running)
            continue;

        constexpr int DROP_COUNT = 10;
        LogInfo(VB_MEDIAOUT, "Opus RTP send pipeline [%d]: dropping next %d buffers\n",
                p.instanceId, DROP_COUNT);

        p.dropCounter = DROP_COUNT;

        if (p.probeId != 0)
            continue;

        GstIterator* it = gst_bin_iterate_sources(GST_BIN(p.pipeline));
        GValue val = G_VALUE_INIT;
        GstElement* srcElem = nullptr;
        if (gst_iterator_next(it, &val) == GST_ITERATOR_OK) {
            srcElem = GST_ELEMENT(g_value_get_object(&val));
            gst_object_ref(srcElem);
            g_value_unset(&val);
        }
        gst_iterator_free(it);

        if (srcElem) {
            GstPad* srcpad = gst_element_get_static_pad(srcElem, "src");
            if (srcpad) {
                p.probePad = srcpad;
                p.probeId = gst_pad_add_probe(
                    srcpad,
                    GST_PAD_PROBE_TYPE_BUFFER,
                    OpusDropBufferProbe,
                    &p.dropCounter,
                    nullptr);
            }
            gst_object_unref(srcElem);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Watchdog
// ──────────────────────────────────────────────────────────────────────────────
void OpusRTPManager::WatchdogLoop() {
    constexpr int WATCHDOG_INTERVAL_S = 30;

    while (m_watchdogRunning.load()) {
        // Sleep in small increments so we can exit promptly
        for (int i = 0; i < WATCHDOG_INTERVAL_S * 10 && m_watchdogRunning.load(); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!m_watchdogRunning.load()) break;

        if (PollPipelinesWatchdog()) {
            LogWarn(VB_MEDIAOUT, "OpusRTPManager: Watchdog detected failed pipelines, rebuilding\n");
            ApplyConfig();
        }
    }
}

bool OpusRTPManager::PollPipelinesWatchdog() {
    bool needsRebuild = false;

    std::lock_guard<std::mutex> lock(m_pipelineMutex);

    auto checkPipelines = [this, &needsRebuild](std::map<int, OpusRTPPipeline>& pipelines, const char* direction) {
        for (auto& kv : pipelines) {
            OpusRTPPipeline& p = kv.second;
            if (!p.pipeline || !p.running) continue;

            // Drain bus messages
            if (p.bus) {
                GstMessage* msg;
                while ((msg = gst_bus_pop(p.bus)) != nullptr) {
                    switch (GST_MESSAGE_TYPE(msg)) {
                        case GST_MESSAGE_ERROR: {
                            GError* err = nullptr; gchar* dbg = nullptr;
                            gst_message_parse_error(msg, &err, &dbg);
                            LogErr(VB_MEDIAOUT,
                                   "Opus RTP %s pipeline [%d] bus error: %s\n",
                                   direction, p.instanceId, err->message);
                            g_error_free(err); g_free(dbg);
                            p.errorMessage = "GStreamer error";
                            break;
                        }
                        case GST_MESSAGE_WARNING: {
                            GError* err = nullptr; gchar* dbg = nullptr;
                            gst_message_parse_warning(msg, &err, &dbg);
                            LogWarn(VB_MEDIAOUT,
                                    "Opus RTP %s pipeline [%d] bus warning: %s\n",
                                    direction, p.instanceId, err->message);
                            g_error_free(err); g_free(dbg);
                            break;
                        }
                        default: break;
                    }
                    gst_message_unref(msg);
                }
            }

            // Check current pipeline state
            GstState curState = GST_STATE_NULL, pendingState = GST_STATE_VOID_PENDING;
            gst_element_get_state(p.pipeline, &curState, &pendingState, 0);

            if (curState != GST_STATE_PLAYING && pendingState != GST_STATE_PLAYING) {
                LogWarn(VB_MEDIAOUT,
                        "Opus RTP %s pipeline [%d] is in %s state -- flagging for rebuild\n",
                        direction, p.instanceId,
                        gst_element_state_get_name(curState));
                p.running = false;
                p.errorMessage = "Watchdog: not in PLAYING state";
                needsRebuild = true;
            } else if (p.isSend) {
                // Zombie detection for send pipelines: check udpsink byte count
                GstElement* usink = gst_bin_get_by_name(GST_BIN(p.pipeline), "usink");
                if (usink) {
                    guint64 bytesSent = 0;
                    g_object_get(usink, "bytes-served", &bytesSent, nullptr);
                    gst_object_unref(usink);

                    if (bytesSent == p.lastByteCount) {
                        p.stallCount++;
                        if (p.stallCount >= 2) {
                            LogWarn(VB_MEDIAOUT,
                                    "Opus RTP send pipeline [%d] stalled (no bytes for %d checks)\n",
                                    p.instanceId, p.stallCount);
                            p.running = false;
                            p.errorMessage = "Watchdog: send pipeline stalled";
                            needsRebuild = true;
                        }
                    } else {
                        p.stallCount = 0;
                    }
                    p.lastByteCount = bytesSent;
                }
            }
        }
    };

    checkPipelines(m_sendPipelines, "send");
    checkPipelines(m_recvPipelines, "recv");

    return needsRebuild;
}

// ──────────────────────────────────────────────────────────────────────────────
// Status
// ──────────────────────────────────────────────────────────────────────────────
bool OpusRTPManager::HasActiveSendInstances() {
    if (!m_active.load()) return false;
    std::lock_guard<std::mutex> lock(m_pipelineMutex);
    return !m_sendPipelines.empty();
}

OpusRTPManager::Status OpusRTPManager::GetStatus() {
    Status status;

    std::unique_lock<std::mutex> lock(m_pipelineMutex, std::try_to_lock);
    if (lock.owns_lock()) {
        for (const auto& [id, p] : m_sendPipelines) {
            Status::PipelineStatus ps;
            ps.instanceId = id;
            ps.mode = "send";
            ps.running = p.running;
            ps.error = p.errorMessage;

            for (const auto& inst : m_config.instances) {
                if (inst.id == id) {
                    ps.name = inst.name;
                    break;
                }
            }
            status.pipelines.push_back(ps);
        }

        for (const auto& [id, p] : m_recvPipelines) {
            Status::PipelineStatus ps;
            ps.instanceId = id;
            ps.mode = "receive";
            ps.running = p.running;
            ps.error = p.errorMessage;

            for (const auto& inst : m_config.instances) {
                if (inst.id == id) {
                    ps.name = inst.name;
                    break;
                }
            }
            status.pipelines.push_back(ps);
        }
    } else {
        Status::PipelineStatus ps;
        ps.instanceId = -1;
        ps.name = "(status unavailable -- pipeline operation in progress)";
        ps.mode = "unknown";
        ps.running = false;
        status.pipelines.push_back(ps);
    }

    return status;
}

// ──────────────────────────────────────────────────────────────────────────────
// HTTP API
// ──────────────────────────────────────────────────────────────────────────────
HttpResponsePtr OpusRTPManager::render_GET(const HttpRequestPtr& req) {
    std::string url(req->path());

    if (url.find("/opusrtp/") == 0) {
        url = url.substr(9);
    } else if (url == "/opusrtp") {
        url = "status";
    }

    if (url == "status") {
        Status st = GetStatus();
        Json::Value result;

        Json::Value pipelines(Json::arrayValue);
        for (const auto& p : st.pipelines) {
            Json::Value pj;
            pj["instanceId"] = p.instanceId;
            pj["name"] = p.name;
            pj["mode"] = p.mode;
            pj["running"] = p.running;
            if (!p.error.empty()) {
                pj["error"] = p.error;
            }
            pipelines.append(pj);
        }
        result["pipelines"] = pipelines;
        result["active"] = m_active.load();

        Json::StreamWriterBuilder wbuilder;
        wbuilder["indentation"] = "";
        std::string resultStr = Json::writeString(wbuilder, result);

        return makeStringResponse(resultStr, 200, "application/json");
    }

    return makeStringResponse("{\"error\":\"unknown endpoint\"}", 404, "application/json");
}

#endif // HAS_OPUS_RTP_GSTREAMER
