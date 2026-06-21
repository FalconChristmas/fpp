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

#include "VideoOutputManager.h"
#include "VideoInputManager.h"
#include "GStreamerOut.h"
#include "common.h"
#include "settings.h"
#include "log.h"
#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"

#include <fstream>
#include <cstring>
#include <thread>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#if __has_include(<gst/app/gstappsink.h>)
#include <gst/app/gstappsink.h>
#endif

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

// SAP (Session Announcement Protocol) constants — RFC 2974
static constexpr const char* SAP_MCAST_ADDR = "239.255.255.255";
static constexpr int SAP_PORT              = 9875;
static constexpr int SAP_TTL               = 255;
static constexpr int SAP_VERSION           = 1;
static constexpr int SAP_INTERVAL_S        = 30;

VideoOutputManager& VideoOutputManager::Instance() {
    static VideoOutputManager instance;
    return instance;
}

VideoOutputManager::~VideoOutputManager() {
    Shutdown();
}

void VideoOutputManager::Init() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized)
        return;

    // Only init if PipeWire backend is active (Simple or Advanced)
    if (!isPipeWireBackend()) {
        std::string backend = getSetting("MediaBackend");
        LogDebug(VB_MEDIAOUT, "VideoOutputManager: Skipping init (MediaBackend=%s, not pipewire)\n", backend.c_str());
        return;
    }

    // Check if video routing is configured
    std::string videoSink = getSetting("PipeWireVideoSinkName");
    if (videoSink.empty()) {
        LogDebug(VB_MEDIAOUT, "VideoOutputManager: No PipeWireVideoSinkName configured, skipping\n");
        return;
    }

    m_initialized = true;

    // Load config but don't start on-demand consumers yet — those start
    // when GStreamerOut calls StartConsumers() after the producer is ready.
    // However, consumers with a sourceNode (targeting a persistent video
    // input source) can start immediately.
    if (LoadConfig()) {
        // Persistent consumers (those with a sourceNode) are NOT started
        // here — they are deferred until NotifyProducerReady() is called
        // by VideoInputManager once the source pipeline has its first
        // video frame.  This avoids the consumer spinning on expensive
        // videoconvert+videoscale of black frames during the HLS buffering
        // window (which can burn a full CPU core at 30fps 1080p).
        int persistentCount = 0;
        for (auto& c : m_consumers) {
            if (!c.sourceNode.empty())
                persistentCount++;
        }
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Initialized with %d consumer(s) "
                "(%d persistent deferred, %d on-demand)\n",
                (int)m_consumers.size(), persistentCount,
                (int)m_consumers.size() - persistentCount);
        // Start SAP multicast announcer for any persistent RTP consumers
        StartSAPAnnouncer();
    }
}

void VideoOutputManager::Reload() {
    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Reloading configuration\n");

    // Stop SAP before taking the lock (SAP thread needs m_mutex)
    StopSAPAnnouncer();

    std::string savedProducer;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        StopAllConsumers();
        m_consumers.clear();
        savedProducer = m_activeProducer;
    }

    // Wait for detached teardown threads to finish releasing DRM/CMA
    // before starting new consumer pipelines.
    WaitForPendingTeardowns(5000);

    // Re-check if video routing is configured
    std::string videoSink = getSetting("PipeWireVideoSinkName");
    if (videoSink.empty()) {
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Video routing disabled (no PipeWireVideoSinkName)\n");
        return;
    }

    bool shouldStartSAP = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_initialized = true;

        if (LoadConfig()) {
            LogInfo(VB_MEDIAOUT, "VideoOutputManager: Reloaded with %d consumer(s)\n", (int)m_consumers.size());
            // If a producer was active, restart consumers targeting it
            if (!savedProducer.empty()) {
                m_activeProducer = savedProducer;
                for (auto& consumer : m_consumers) {
                    if (consumer.type == "rtp")
                        shouldStartSAP = true;
                    StartConsumer(consumer);
                }
            }
        }
    }
    if (shouldStartSAP)
        StartSAPAnnouncer();
}

void VideoOutputManager::Shutdown() {
    // Idempotent: main() calls this at shutdown and ~VideoOutputManager() calls
    // it again at static-destruction time. Run the teardown only once.
    if (m_shutdownDone.exchange(true)) {
        return;
    }
    // Stop SAP announcer first — outside the lock because SAP thread needs m_mutex
    StopSAPAnnouncer();

    std::lock_guard<std::mutex> lock(m_mutex);
    StopAllConsumers();
    m_consumers.clear();
    m_activeProducer.clear();
    m_initialized = false;
    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Shutdown complete\n");
}

void VideoOutputManager::StartConsumers(const std::string& producerNodeName,
                                        int primaryConnectorId,
                                        const std::set<int>& directConnectorIds) {
    bool shouldStartSAP = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized || m_consumers.empty()) {
            return;
        }

        m_activeProducer = producerNodeName;
        m_primaryConnectorId = primaryConnectorId;
        m_directConnectorIds = directConnectorIds;

        // Extract stream slot number from producer node name (e.g. "fppd_video_stream_2" → 2)
        int producerSlot = 0;
        {
            const std::string prefix = "fppd_video_stream_";
            auto pos = producerNodeName.find(prefix);
            if (pos != std::string::npos) {
                std::string slotStr = producerNodeName.substr(pos + prefix.size());
                try { producerSlot = std::stoi(slotStr); } catch (...) {}
            }
        }

        // Only start consumers that don't have a fixed sourceNode — those
        // depend on fppd media playback.  Consumers with sourceNode are
        // already running (started at Init).
        // If the consumer has streamSlots configured, only start it for matching slots.
        int started = 0;
        for (auto& consumer : m_consumers) {
            if (consumer.sourceNode.empty()) {
                // Check stream slot filter
                if (!consumer.streamSlots.empty() && producerSlot > 0) {
                    bool slotMatch = false;
                    for (int s : consumer.streamSlots) {
                        if (s == producerSlot) { slotMatch = true; break; }
                    }
                    if (!slotMatch) continue;
                }
                if (consumer.type == "rtp")
                    shouldStartSAP = true;
                StartConsumer(consumer);
                started++;
            }
        }
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Started %d on-demand consumer(s) targeting producer '%s' (slot %d)\n",
                started, producerNodeName.c_str(), producerSlot);
    }
    // Start SAP announcer outside the lock (SAPAnnounceLoop needs m_mutex)
    if (shouldStartSAP)
        StartSAPAnnouncer();
}

void VideoOutputManager::SuspendPersistentHdmiConsumers() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        int stopped = 0;
        for (auto& consumer : m_consumers) {
            if (!consumer.sourceNode.empty() && consumer.type == "hdmi" && consumer.running) {
                LogInfo(VB_MEDIAOUT, "VideoOutputManager: Suspending persistent HDMI consumer '%s' (connector %s) for DRM master handoff\n",
                        consumer.name.c_str(), consumer.connector.c_str());
                StopConsumer(consumer);
                stopped++;
            }
        }
        if (stopped > 0)
            LogInfo(VB_MEDIAOUT, "VideoOutputManager: Suspended %d persistent HDMI consumer(s)\n", stopped);
    }
    // Wait for DRM/CMA resources to be freed before the main pipeline
    // claims DRM master on the same device.
    WaitForPendingTeardowns(5000);
}

void VideoOutputManager::ResumePersistentHdmiConsumers() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Group persistent HDMI consumers by sourceNode (same logic as Init)
    std::map<std::string, std::vector<size_t>> hdmiSourceGroups;
    for (size_t i = 0; i < m_consumers.size(); i++) {
        auto& c = m_consumers[i];
        if (!c.sourceNode.empty() && c.type == "hdmi" && !c.running) {
            hdmiSourceGroups[c.sourceNode].push_back(i);
        }
    }

    int started = 0;
    std::set<size_t> startedAsGroup;
    for (auto& [sourceNode, indices] : hdmiSourceGroups) {
        if (indices.size() > 1) {
            if (StartHdmiConsumerGroup(indices)) {
                started += indices.size();
                for (auto idx : indices) startedAsGroup.insert(idx);
            }
        }
    }

    for (size_t i = 0; i < m_consumers.size(); i++) {
        auto& c = m_consumers[i];
        if (!c.sourceNode.empty() && c.type == "hdmi" && !c.running
            && startedAsGroup.find(i) == startedAsGroup.end()) {
            LogInfo(VB_MEDIAOUT, "VideoOutputManager: Resuming persistent HDMI consumer '%s' (connector %s)\n",
                    c.name.c_str(), c.connector.c_str());
            StartConsumer(c);
            started++;
        }
    }
    if (started > 0)
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Resumed %d persistent HDMI consumer(s)\n", started);
}

void VideoOutputManager::NotifyProducerReady(const std::string& channelName) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Group persistent HDMI consumers by sourceNode for grouped start.
    std::map<std::string, std::vector<size_t>> hdmiSourceGroups;
    for (size_t i = 0; i < m_consumers.size(); i++) {
        auto& c = m_consumers[i];
        if (c.sourceNode == channelName && !c.running && c.type == "hdmi") {
            hdmiSourceGroups[c.sourceNode].push_back(i);
        }
    }

    int started = 0;
    std::set<size_t> startedAsGroup;
    for (auto& [sourceNode, indices] : hdmiSourceGroups) {
        if (indices.size() > 1) {
            if (StartHdmiConsumerGroup(indices)) {
                started += indices.size();
                for (auto idx : indices) startedAsGroup.insert(idx);
            }
        }
    }

    for (size_t i = 0; i < m_consumers.size(); i++) {
        auto& c = m_consumers[i];
        if (c.sourceNode == channelName && !c.running
            && startedAsGroup.find(i) == startedAsGroup.end()) {
            StartConsumer(c);
            started++;
        }
    }

    if (started > 0)
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Producer '%s' ready — started %d consumer(s)\n",
                channelName.c_str(), started);
}

std::vector<VideoOutputManager::HdmiConsumerInfo> VideoOutputManager::GetHdmiConsumers(
    int streamSlot, const std::set<int>& skipConnectorIds) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<HdmiConsumerInfo> result;
    for (const auto& c : m_consumers) {
        if (c.type != "hdmi" || c.connectorId <= 0)
            continue;
        // Only on-demand consumers (no fixed sourceNode)
        if (!c.sourceNode.empty())
            continue;
        // Stream slot filter
        if (!c.streamSlots.empty() && streamSlot > 0) {
            bool match = false;
            for (int s : c.streamSlots)
                if (s == streamSlot) { match = true; break; }
            if (!match) continue;
        }
        if (skipConnectorIds.count(c.connectorId))
            continue;
        HdmiConsumerInfo info;
        info.connectorId = c.connectorId;
        info.connector = c.connector;
        info.cardPath = c.cardPath;
        info.width = c.width;
        info.height = c.height;
        result.push_back(info);
    }
    return result;
}

void VideoOutputManager::StopConsumers() {
    bool shouldStopSAP = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_initialized) return;

        // Only stop on-demand consumers (those without a fixed sourceNode).
        // Persistent-source consumers keep running independently of media playback.
        int stopped = 0;
        for (auto& consumer : m_consumers) {
            if (consumer.sourceNode.empty() && consumer.running) {
                StopConsumer(consumer);
                stopped++;
            }
        }
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Stopped %d on-demand consumer(s) (producer gone)\n", stopped);
        m_activeProducer.clear();

        // Check if any RTP consumers are still running (persistent-source)
        bool hasRunningRtp = false;
        for (const auto& c : m_consumers) {
            if (c.type == "rtp" && c.running) { hasRunningRtp = true; break; }
        }
        if (!hasRunningRtp)
            shouldStopSAP = true;
    }
    if (shouldStopSAP)
        StopSAPAnnouncer();
}

bool VideoOutputManager::HasConsumers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_consumers.empty();
}

bool VideoOutputManager::HasActiveConsumers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& c : m_consumers) {
        if (c.running)
            return true;
    }
    return false;
}

int VideoOutputManager::ActiveConsumerCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& c : m_consumers) {
        if (c.running)
            count++;
    }
    return count;
}

bool VideoOutputManager::LoadConfig() {
    std::string configPath = FPP_DIR_MEDIA("/config/pipewire-video-consumers.json");

    if (!FileExists(configPath)) {
        LogDebug(VB_MEDIAOUT, "VideoOutputManager: No consumer config at %s\n", configPath.c_str());
        return false;
    }

    std::ifstream ifs(configPath);
    if (!ifs.is_open()) {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Cannot open %s\n", configPath.c_str());
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, ifs, &root, &errors)) {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: JSON parse error: %s\n", errors.c_str());
        return false;
    }

    if (!root.isArray()) {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Config is not an array\n");
        return false;
    }

    for (const auto& entry : root) {
        ConsumerInfo ci;
        ci.id = entry.get("id", 0).asInt();
        ci.name = entry.get("name", "").asString();
        ci.type = entry.get("type", "").asString();
        ci.pipeWireNodeName = entry.get("pipeWireNodeName", "").asString();

        if (ci.type == "hdmi") {
            ci.connector = entry.get("connector", "").asString();
            ci.cardPath = entry.get("cardPath", "").asString();
            ci.connectorId = entry.get("connectorId", -1).asInt();
            ci.width = entry.get("width", 0).asInt();
            ci.height = entry.get("height", 0).asInt();
            ci.scaling = entry.get("scaling", "fit").asString();
        } else if (ci.type == "overlay") {
            ci.overlayModel = entry.get("overlayModel", "").asString();
        } else if (ci.type == "rtp") {
            ci.address = entry.get("address", "239.0.0.1").asString();
            ci.port = entry.get("port", 5004).asInt();
            ci.rtpEncoding = entry.get("encoding", "raw").asString();
        }

        if (ci.type.empty() || ci.pipeWireNodeName.empty()) {
            LogWarn(VB_MEDIAOUT, "VideoOutputManager: Skipping consumer with missing type or node name\n");
            continue;
        }

        ci.sourceNode = entry.get("sourceNode", "").asString();

        if (entry.isMember("streamSlots") && entry["streamSlots"].isArray()) {
            for (const auto& s : entry["streamSlots"]) {
                int slot = s.asInt();
                if (slot >= 1 && slot <= 5)
                    ci.streamSlots.push_back(slot);
            }
        }

        m_consumers.push_back(std::move(ci));
    }

    return !m_consumers.empty();
}

bool VideoOutputManager::StartHdmiConsumerGroup(const std::vector<size_t>& indices) {
#ifdef HAS_GSTREAMER_VIDEO_OUTPUT
    if (indices.size() < 2) return false;

    // The first consumer in the group owns the pipeline
    size_t ownerIdx = indices[0];
    ConsumerInfo& owner = m_consumers[ownerIdx];

    if (owner.running) {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Group owner '%s' already running\n", owner.name.c_str());
        return true;
    }

    setenv("PIPEWIRE_RUNTIME_DIR", "/run/pipewire-fpp", 0);

    // Validate all consumers and check connector connectivity
    std::vector<size_t> validIndices;
    for (size_t idx : indices) {
        auto& c = m_consumers[idx];
        if (c.connectorId <= 0 || c.cardPath.empty()) {
            LogWarn(VB_MEDIAOUT, "VideoOutputManager: Skipping group member '%s' — no valid connector\n", c.name.c_str());
            continue;
        }
        if (!c.connector.empty()) {
            auto drmCheck = GStreamerOutput::ResolveDrmConnector(c.connector);
            if (!drmCheck.connected) {
                LogInfo(VB_MEDIAOUT, "VideoOutputManager: Skipping group member '%s' — connector %s not connected\n",
                        c.name.c_str(), c.connector.c_str());
                continue;
            }
        }
        if (m_primaryConnectorId >= 0 && c.connectorId == m_primaryConnectorId) continue;
        if (m_directConnectorIds.count(c.connectorId)) continue;
        validIndices.push_back(idx);
    }
    if (validIndices.empty()) return false;
    if (validIndices.size() == 1) {
        // Only one valid — fall back to single consumer
        return StartConsumer(m_consumers[validIndices[0]]);
    }

    // Start each consumer as an independent pipeline with its own intervideosrc.
    // This avoids CMA/DRM buffer exhaustion issues when both kmssink elements
    // in a combined pipeline allocate dumb buffers simultaneously on a shared fd.
    bool allOk = true;
    for (size_t i = 0; i < validIndices.size(); i++) {
        if (!StartConsumer(m_consumers[validIndices[i]])) {
            allOk = false;
        }
    }
    return allOk;

#if 0 // Combined pipeline disabled — see comment above
    std::string sourceNodeName = m_consumers[validIndices[0]].sourceNode;
    std::string pipelineDesc = "intervideosrc timeout=5000000000 channel=" + sourceNodeName
        + " ! videoconvert ! tee name=t";
    std::string names; // for logging
    for (size_t i = 0; i < validIndices.size(); i++) {
        auto& c = m_consumers[validIndices[i]];
        std::string sinkName = "sink" + std::to_string(i);
        pipelineDesc += " t. ! queue max-size-buffers=2 leaky=downstream ! videoscale ! ";
        if (c.width > 0 && c.height > 0) {
            pipelineDesc += "video/x-raw,format=BGRx,width=" + std::to_string(c.width)
                         + ",height=" + std::to_string(c.height) + " ! ";
        } else {
            pipelineDesc += "video/x-raw,format=BGRx ! ";
        }
        pipelineDesc += "kmssink name=" + sinkName + " sync=false"
                     " connector-id=" + std::to_string(c.connectorId)
                     + " restore-crtc=true skip-vsync=true";
        if (!names.empty()) names += ", ";
        names += c.name;
    }

    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Starting HDMI group [%s] with combined pipeline: %s\n",
            names.c_str(), pipelineDesc.c_str());

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(pipelineDesc.c_str(), &error);
    if (!pipeline) {
        LogErr(VB_MEDIAOUT, "VideoOutputManager: Group pipeline creation failed: %s\n",
               error ? error->message : "unknown");
        WarningHolder::AddWarning(31, "Video output group pipeline creation failed");
        if (error) g_error_free(error);
        return false;
    }
    if (error) {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Group pipeline warning: %s\n", error->message);
        g_error_free(error);
    }

    // intervideosrc is configured inline (channel name in pipeline desc).\n    // No PipeWire node configuration or manual pw-link needed.

    // Configure each kmssink with shared DRM fd and plane
    for (size_t i = 0; i < validIndices.size(); i++) {
        auto& c = m_consumers[validIndices[i]];
        std::string sinkName = "sink" + std::to_string(i);
        GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), sinkName.c_str());
        if (sink) {
            // Prefer live-resolved card path over config value
            std::string resolvedCard;
            int resolvedConnId = c.connectorId;
            if (!c.connector.empty()) {
                auto drmCheck = GStreamerOutput::ResolveDrmConnector(c.connector);
                resolvedCard = drmCheck.cardPath;
                if (drmCheck.connectorId > 0)
                    resolvedConnId = drmCheck.connectorId;
            }
            std::string cardForFd = !resolvedCard.empty() ? resolvedCard : c.cardPath;
            int drmFd = cardForFd.empty() ? -1 : GStreamerOutput::GetSharedDrmFd(cardForFd);
            if (drmFd >= 0) {
                g_object_set(sink, "fd", drmFd, NULL);
                int planeId = GStreamerOutput::FindPrimaryPlaneForConnector(drmFd, resolvedConnId);
                if (planeId >= 0) {
                    g_object_set(sink, "plane-id", planeId, NULL);
                }
                LogInfo(VB_MEDIAOUT, "VideoOutputManager: Group sink '%s' fd=%d plane=%d connector=%d\n",
                        c.name.c_str(), drmFd, planeId, resolvedConnId);
            } else {
                g_object_set(sink, "driver-name", "vc4", NULL);
            }
            gst_object_unref(sink);
        }
    }

    // Store pipeline in the first valid consumer (the "owner")
    size_t pipelineOwnerIdx = validIndices[0];
    m_consumers[pipelineOwnerIdx].pipeline = pipeline;
    m_consumers[pipelineOwnerIdx].shutdownRequested = false;
    m_consumers[pipelineOwnerIdx].running = true;
    m_consumers[pipelineOwnerIdx].groupPipelineOwnerIdx = -1; // owner

    // Mark other group members as running, pointing to the owner
    for (size_t i = 1; i < validIndices.size(); i++) {
        auto& c = m_consumers[validIndices[i]];
        c.running = true;
        c.shutdownRequested = false;
        c.pipeline = nullptr;
        c.groupPipelineOwnerIdx = (int)pipelineOwnerIdx;
    }

    // Start background thread for pipeline lifecycle
    std::string groupNames = names;
    std::atomic<bool>* shutdownFlag = &m_consumers[pipelineOwnerIdx].shutdownRequested;

    m_consumers[pipelineOwnerIdx].startThread = std::thread(
        [pipeline, groupNames, shutdownFlag]() {
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Group [%s] background thread starting\n", groupNames.c_str());

        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Group [%s] set_state(PLAYING) returned %d\n",
                groupNames.c_str(), (int)ret);

        if (ret == GST_STATE_CHANGE_FAILURE) {
            if (!shutdownFlag->load())
                LogWarn(VB_MEDIAOUT, "VideoOutputManager: Group [%s] pipeline failed to start\n", groupNames.c_str());
            return;
        }

        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Group [%s] pipeline reached PLAYING\n", groupNames.c_str());

        // Bus monitor loop
        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
        int busLoopIter = 0;
        while (!shutdownFlag->load()) {
            GstMessage* msg = gst_bus_timed_pop(bus, 500 * GST_MSECOND);
            busLoopIter++;
            if (busLoopIter % 10 == 1) {
                GstState cur = GST_STATE_VOID_PENDING, pend = GST_STATE_VOID_PENDING;
                gst_element_get_state(pipeline, &cur, &pend, 0);
                if (cur != GST_STATE_PLAYING || pend != GST_STATE_VOID_PENDING) {
                    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Group [%s] state check: current=%d pending=%d\n",
                            groupNames.c_str(), (int)cur, (int)pend);
                }
            }
            if (!msg) continue;
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError* err = nullptr; gchar* debug = nullptr;
                    gst_message_parse_error(msg, &err, &debug);
                    if (!shutdownFlag->load())
                        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Group [%s] error: %s (%s)\n",
                                groupNames.c_str(), err ? err->message : "?", debug ? debug : "");
                    if (err) g_error_free(err); g_free(debug);
                    gst_message_unref(msg); gst_object_unref(bus);
                    return;
                }
                case GST_MESSAGE_EOS:
                    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Group [%s] EOS\n", groupNames.c_str());
                    gst_message_unref(msg); gst_object_unref(bus);
                    return;
                case GST_MESSAGE_ASYNC_DONE:
                    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Group [%s] ASYNC_DONE (preroll complete)\n",
                            groupNames.c_str());
                    break;
                case GST_MESSAGE_STATE_CHANGED:
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                        GstState oldSt, newSt, pendSt;
                        gst_message_parse_state_changed(msg, &oldSt, &newSt, &pendSt);
                        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Group [%s] state %d -> %d (pending %d)\n",
                                groupNames.c_str(), (int)oldSt, (int)newSt, (int)pendSt);
                    }
                    break;
                case GST_MESSAGE_WARNING: {
                    GError* err = nullptr; gchar* debug = nullptr;
                    gst_message_parse_warning(msg, &err, &debug);
                    LogWarn(VB_MEDIAOUT, "VideoOutputManager: Group [%s] warning: %s (%s)\n",
                            groupNames.c_str(), err ? err->message : "?", debug ? debug : "");
                    if (err) g_error_free(err); g_free(debug);
                    break;
                }
                default: break;
            }
            gst_message_unref(msg);
        }
        gst_object_unref(bus);
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Group [%s] background thread exiting\n", groupNames.c_str());
    });

    return true;
#endif // #if 0 -- combined pipeline disabled
#else
    return false;
#endif
}

bool VideoOutputManager::StartConsumer(ConsumerInfo& consumer) {
#ifdef HAS_GSTREAMER_VIDEO_OUTPUT
    if (consumer.running) {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' already running\n", consumer.name.c_str());
        return true;
    }

    // Set PipeWire env (same as GStreamerOut)
    setenv("PIPEWIRE_RUNTIME_DIR", "/run/pipewire-fpp", 0);

    // Build GStreamer consumer pipeline description
    std::string pipelineDesc;

    // Common source: intervideosrc reads from source's intervideosink channel.
    // This uses GStreamer's in-process inter-pipeline communication,
    // bypassing PipeWire for video (PipeWire's video driver scheduling
    // cannot drive video graph cycles at rate=0).
    // timeout=5s — must not overflow gint64 when converted to microseconds
    // and added to g_get_monotonic_time(); UINT64_MAX caused spin-loop.
    pipelineDesc = "intervideosrc timeout=5000000000 channel=" + consumer.sourceNode
                 + " ! videoconvert ! videoscale ! ";

    if (consumer.type == "hdmi") {
        if (consumer.connectorId <= 0 || consumer.cardPath.empty()) {
            LogWarn(VB_MEDIAOUT, "VideoOutputManager: HDMI consumer '%s' has no valid connector\n", consumer.name.c_str());
            return false;
        }

        // Check if the connector is actually connected before launching
        // a consumer pipeline — avoids wasted effort and error log noise.
        if (!consumer.connector.empty()) {
            auto drmCheck = GStreamerOutput::ResolveDrmConnector(consumer.connector);
            if (!drmCheck.connected) {
                LogInfo(VB_MEDIAOUT, "VideoOutputManager: Skipping HDMI consumer '%s' — "
                        "connector %s (id=%d) is not connected\n",
                        consumer.name.c_str(), consumer.connector.c_str(), consumer.connectorId);
                return false;
            }
        }

        // Check for conflict with the primary video output — the main GStreamerOut
        // pipeline may have a direct kmssink on the primary connector.
        // Two kmssinks cannot share the same DRM CRTC/connector.
        // primaryConnectorId is -1 when the primary connector is disconnected
        // (no kmssink in the main pipeline), so no consumer is skipped.
        if (m_primaryConnectorId >= 0 && consumer.connectorId == m_primaryConnectorId) {
            LogWarn(VB_MEDIAOUT, "VideoOutputManager: Skipping HDMI consumer '%s' — "
                    "connector %s (id=%d) is used by the primary video output\n",
                    consumer.name.c_str(), consumer.connector.c_str(), consumer.connectorId);
            return false;
        }
        // Also skip connectors handled by direct kmssink branches on the
        // producer's video tee (avoids DRM CRTC conflicts).
        if (m_directConnectorIds.count(consumer.connectorId)) {
            LogInfo(VB_MEDIAOUT, "VideoOutputManager: Skipping HDMI consumer '%s' — "
                    "connector %s (id=%d) handled by direct kmssink in main pipeline\n",
                    consumer.name.c_str(), consumer.connector.c_str(), consumer.connectorId);
            return false;
        }

        // Check CMA memory before starting HDMI consumer — kmssink needs
        // ~16-24 MB of CMA for dumb buffers.  CmaFree may appear low because
        // the kernel uses CMA pages for general movable allocations; those
        // pages are migrated on demand when a real DMA allocation occurs.
        // If a teardown is in progress, wait briefly for DRM buffers to be
        // released.  Otherwise just warn and proceed — the kernel will
        // attempt CMA migration, and if it truly fails the DRM ioctl
        // (and hence pipeline start) will error out on its own.
        {
            auto readCmaFree = []() -> long {
                FILE* f = fopen("/proc/meminfo", "r");
                if (!f) return -1;
                char line[256];
                long kb = -1;
                while (fgets(line, sizeof(line), f)) {
                    if (sscanf(line, "CmaFree: %ld kB", &kb) == 1) break;
                }
                fclose(f);
                return kb;
            };

            long cmaFreeKB = readCmaFree();
            static constexpr long CMA_WARN_KB = 20480;
            if (cmaFreeKB >= 0 && cmaFreeKB < CMA_WARN_KB) {
                LogInfo(VB_MEDIAOUT, "VideoOutputManager: CMA low (%ld kB) for consumer '%s', "
                        "waiting for pending teardowns then proceeding...\n",
                        cmaFreeKB, consumer.name.c_str());
                WaitForPendingTeardowns(3000);
                cmaFreeKB = readCmaFree();
                if (cmaFreeKB >= 0 && cmaFreeKB < CMA_WARN_KB) {
                    LogWarn(VB_MEDIAOUT, "VideoOutputManager: CMA still low (%ld kB) for '%s' — "
                            "proceeding anyway (kernel will attempt CMA migration)\n",
                            cmaFreeKB, consumer.name.c_str());
                }
            }
        }

        // Add scaling caps for kmssink (vc4 DRM).  Do NOT force BGRx —
        // vc4 display hardware on all Pi models (3/4/5) natively accepts
        // YUV formats (I420, NV12) and performs YUV→RGB conversion in the
        // HVS scanout pipeline for free.  Letting GStreamer auto-negotiate
        // the format keeps the data in YUV end-to-end so videoscale
        // processes 1.5 bytes/pixel instead of 4 (BGRx), cutting CPU by ~60%.
        int fps = VideoInputManager::Instance().GetSourceFramerate(consumer.sourceNode);
        if (fps <= 0) fps = 30;  // fallback for on-demand consumers

        if (consumer.width > 0 && consumer.height > 0) {
            pipelineDesc += "video/x-raw,width=" + std::to_string(consumer.width)
                         + ",height=" + std::to_string(consumer.height)
                         + ",framerate=" + std::to_string(fps) + "/1 ! ";
        } else {
            pipelineDesc += "video/x-raw,framerate=" + std::to_string(fps) + "/1 ! ";
        }

        // Queue absorbs timing jitter between intervideosrc and kmssink;
        // without it GStreamer warns about missing latency compensation.
        pipelineDesc += "queue max-size-buffers=2 ! ";

        // kmssink sync=true paces frame rendering by intervideosrc timestamps
        // (fresh monotonic timestamps, independent of HLS timing).
        pipelineDesc += "kmssink name=sink sync=true"
                     " connector-id=" + std::to_string(consumer.connectorId)
                     + " restore-crtc=true";

    } else if (consumer.type == "overlay") {
        // Resolve the pixel overlay model to get its dimensions
        PixelOverlayModel* model = PixelOverlayManager::INSTANCE.getModel(consumer.overlayModel);
        if (!model) {
            LogWarn(VB_MEDIAOUT, "VideoOutputManager: Overlay model '%s' not found for consumer '%s'\n",
                    consumer.overlayModel.c_str(), consumer.name.c_str());
            return false;
        }

        int overlayW = model->getWidth();
        int overlayH = model->getHeight();
        if (overlayW <= 0 || overlayH <= 0) {
            LogWarn(VB_MEDIAOUT, "VideoOutputManager: Overlay model '%s' has invalid size %dx%d\n",
                    consumer.overlayModel.c_str(), overlayW, overlayH);
            return false;
        }

        // Set up shared overlay state for the appsink callback
        consumer.overlayState = std::make_shared<OverlayState>();
        consumer.overlayState->model = model;
        consumer.overlayState->width = overlayW;
        consumer.overlayState->height = overlayH;
        consumer.overlayState->active = true;

        // Register model listener so we track model replacement/deletion
        std::weak_ptr<OverlayState> weakState = consumer.overlayState;
        PixelOverlayManager::INSTANCE.addModelListener(consumer.overlayModel,
            "VideoOutputManager_" + consumer.pipeWireNodeName,
            [weakState](PixelOverlayModel* m) {
                if (auto state = weakState.lock()) {
                    std::lock_guard<std::mutex> lock(state->mtx);
                    state->model = m;
                    if (m) {
                        state->width = m->getWidth();
                        state->height = m->getHeight();
                    }
                }
            });

        // Build pipeline: pipewiresrc → videoconvert → videoscale → capsfilter(RGB, WxH) → appsink
        pipelineDesc += "video/x-raw,format=RGB,width=" + std::to_string(overlayW)
                     + ",height=" + std::to_string(overlayH)
                     + " ! appsink name=sink emit-signals=true sync=true max-buffers=2 drop=true";

        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Overlay consumer '%s' targeting model '%s' (%dx%d)\n",
                consumer.name.c_str(), consumer.overlayModel.c_str(), overlayW, overlayH);

    } else if (consumer.type == "rtp") {
        // RTP video output — encoding-dependent pipeline
        std::string enc = consumer.rtpEncoding;
        if (enc.empty()) enc = "raw";

        if (enc == "h264") {
            pipelineDesc += "x264enc tune=zerolatency bitrate=4000 speed-preset=ultrafast"
                           " key-int-max=30 bframes=0"
                           " ! video/x-h264,profile=baseline"
                           " ! rtph264pay config-interval=1 pt=96";
        } else if (enc == "h265") {
            pipelineDesc += "x265enc tune=zerolatency bitrate=3000 speed-preset=ultrafast"
                           " key-int-max=30"
                           " ! rtph265pay config-interval=1 pt=96";
        } else if (enc == "mjpeg") {
            pipelineDesc += "jpegenc quality=80 ! rtpjpegpay pt=26";
        } else {
            pipelineDesc += "rtpvrawpay pt=96";
        }

        pipelineDesc += " ! udpsink host=" + consumer.address
                     + " port=" + std::to_string(consumer.port)
                     + " auto-multicast=true";

        // Generate SDP file for receivers (VLC, ffplay, etc.)
        WriteSdpFile(consumer, enc);

    } else {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Unknown consumer type '%s'\n", consumer.type.c_str());
        return false;
    }

    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Starting consumer '%s' (%s): %s\n",
            consumer.name.c_str(), consumer.type.c_str(), pipelineDesc.c_str());

    GError* error = nullptr;
    consumer.pipeline = gst_parse_launch(pipelineDesc.c_str(), &error);
    if (!consumer.pipeline) {
        LogErr(VB_MEDIAOUT, "VideoOutputManager: Failed to create pipeline for '%s': %s\n",
               consumer.name.c_str(), error ? error->message : "unknown error");
        WarningHolder::AddWarning(31, "Video output pipeline creation failed for '" + consumer.name + "'");
        if (error) g_error_free(error);
        return false;
    }
    if (error) {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Pipeline warning for '%s': %s\n",
                consumer.name.c_str(), error->message);
        g_error_free(error);
    }

    // intervideosrc is configured inline — no PipeWire node setup needed.

    // For HDMI consumers, pass the shared DRM fd to kmssink so all
    // pipelines share a single DRM master — avoids contention on card1.
    // fd must be set BEFORE driver-name (kmssink rejects fd if devname is set).
    // When fd is available, driver-name is unnecessary (derived from the fd).
    // When fd is unavailable, fall back to driver-name=vc4.
    if (consumer.type == "hdmi") {
        GstElement* sink = gst_bin_get_by_name(GST_BIN(consumer.pipeline), "sink");
        if (sink) {
            // Prefer live-resolved card path over config value
            std::string resolvedCard;
            int resolvedConnId = consumer.connectorId;
            if (!consumer.connector.empty()) {
                auto drmCheck = GStreamerOutput::ResolveDrmConnector(consumer.connector);
                resolvedCard = drmCheck.cardPath;
                if (drmCheck.connectorId > 0)
                    resolvedConnId = drmCheck.connectorId;
            }
            std::string cardForFd = !resolvedCard.empty() ? resolvedCard : consumer.cardPath;
            int drmFd = cardForFd.empty() ? -1
                : GStreamerOutput::GetSharedDrmFd(cardForFd);
            if (drmFd >= 0) {
                g_object_set(sink, "fd", drmFd, NULL);
                int planeId = GStreamerOutput::FindPrimaryPlaneForConnector(drmFd, resolvedConnId);
                if (planeId >= 0) {
                    g_object_set(sink, "plane-id", planeId, NULL);
                    consumer.assignedPlaneId = planeId;
                }
                LogInfo(VB_MEDIAOUT, "VideoOutputManager: Set shared DRM fd=%d plane=%d on consumer '%s' kmssink\n",
                        drmFd, planeId, consumer.name.c_str());
            } else {
                g_object_set(sink, "driver-name", "vc4", NULL);
                LogInfo(VB_MEDIAOUT, "VideoOutputManager: No shared DRM fd for consumer '%s', using driver-name=vc4\n",
                        consumer.name.c_str());
            }
            gst_object_unref(sink);
        }
    }

    // Wire up appsink callback for overlay consumers
    if (consumer.type == "overlay" && consumer.overlayState) {
        GstElement* sink = gst_bin_get_by_name(GST_BIN(consumer.pipeline), "sink");
        if (sink) {
            GstAppSinkCallbacks callbacks = {};
            callbacks.new_sample = OnOverlaySample;
            // Pass the shared_ptr's raw pointer as user_data — the shared_ptr
            // in ConsumerInfo keeps it alive for the consumer's lifetime.
            gst_app_sink_set_callbacks(GST_APP_SINK(sink),
                &callbacks, consumer.overlayState.get(), nullptr);
            gst_object_unref(sink);
        }
    }

    // Start the pipeline in a background thread.
    // With intervideosrc, set_state(PLAYING) should complete quickly
    // (no PipeWire negotiation needed).
    consumer.shutdownRequested = false;
    consumer.running = true;

    std::string consumerName = consumer.name;
    GstElement* pipeline = consumer.pipeline;
    std::atomic<bool>* shutdownFlag = &consumer.shutdownRequested;

    consumer.startThread = std::thread([pipeline, consumerName, shutdownFlag]() {
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' background thread starting pipeline\n",
                consumerName.c_str());

        // intervideosrc doesn't need PipeWire links — go straight to PLAYING.
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' set_state(PLAYING) returned %d\n",
                consumerName.c_str(), (int)ret);

        if (ret == GST_STATE_CHANGE_FAILURE) {
            // Check if we were asked to shut down — don't log as error if so
            if (!shutdownFlag->load()) {
                LogWarn(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' pipeline failed to start "
                        "(connector may be disconnected)\n", consumerName.c_str());
            }
            return;
        }

        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' pipeline reached PLAYING (ret=%d)\n",
                consumerName.c_str(), ret);

        // Monitor the pipeline bus for errors / EOS while running
        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
        int busLoopIter = 0;
        while (!shutdownFlag->load()) {
            GstMessage* msg = gst_bus_timed_pop(bus, 500 * GST_MSECOND);
            busLoopIter++;

            // Periodic state check every ~5s (10 iterations * 500ms)
            if (busLoopIter % 10 == 1) {
                GstState cur = GST_STATE_VOID_PENDING, pend = GST_STATE_VOID_PENDING;
                gst_element_get_state(pipeline, &cur, &pend, 0);
                if (cur != GST_STATE_PLAYING || pend != GST_STATE_VOID_PENDING) {
                    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' state check: current=%d pending=%d\n",
                            consumerName.c_str(), (int)cur, (int)pend);
                }
            }

            if (!msg) continue;

            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError* err = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_error(msg, &err, &debug);
                    if (!shutdownFlag->load()) {
                        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' error: %s (%s)\n",
                                consumerName.c_str(), err ? err->message : "?", debug ? debug : "");
                    }
                    if (err) g_error_free(err);
                    g_free(debug);
                    gst_message_unref(msg);
                    gst_object_unref(bus);
                    return;
                }
                case GST_MESSAGE_EOS:
                    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' received EOS\n",
                            consumerName.c_str());
                    gst_message_unref(msg);
                    gst_object_unref(bus);
                    return;
                case GST_MESSAGE_ASYNC_DONE:
                    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' ASYNC_DONE (preroll complete)\n",
                            consumerName.c_str());
                    break;
                case GST_MESSAGE_STATE_CHANGED:
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                        GstState oldSt, newSt, pendSt;
                        gst_message_parse_state_changed(msg, &oldSt, &newSt, &pendSt);
                        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' state %d -> %d (pending %d)\n",
                                consumerName.c_str(), (int)oldSt, (int)newSt, (int)pendSt);
                    }
                    break;
                case GST_MESSAGE_WARNING: {
                    GError* err = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_warning(msg, &err, &debug);
                    LogWarn(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' warning: %s (%s)\n",
                            consumerName.c_str(), err ? err->message : "?", debug ? debug : "");
                    if (err) g_error_free(err);
                    g_free(debug);
                    break;
                }
                default:
                    break;
            }
            gst_message_unref(msg);
        }
        gst_object_unref(bus);
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' background thread exiting\n",
                consumerName.c_str());
    });

    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' launched in background thread\n",
            consumer.name.c_str());
    return true;
#else
    LogWarn(VB_MEDIAOUT, "VideoOutputManager: GStreamer not available\n");
    return false;
#endif
}

#ifdef HAS_GSTREAMER_VIDEO_OUTPUT
GstFlowReturn VideoOutputManager::OnOverlaySample(GstAppSink* appsink, gpointer userData) {
    auto* state = static_cast<OverlayState*>(userData);
    if (!state || !state->active.load())
        return GST_FLOW_EOS;

    GstSample* sample = gst_app_sink_pull_sample(appsink);
    if (!sample)
        return GST_FLOW_OK;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        std::lock_guard<std::mutex> lock(state->mtx);
        PixelOverlayModel* model = state->model;
        if (model) {
            int w = state->width;
            int h = state->height;
            int rowBytes = w * 3;
            int expectedSize = rowBytes * h;
            int stride = (h > 0) ? (int)(map.size / h) : rowBytes;

            if (stride == rowBytes && (int)map.size >= expectedSize) {
                // No padding — push directly
                model->setData(map.data);
            } else if (stride > rowBytes) {
                // GStreamer row padding — strip it
                std::vector<uint8_t> stripped(expectedSize);
                for (int y = 0; y < h; y++) {
                    memcpy(&stripped[y * rowBytes], map.data + y * stride, rowBytes);
                }
                model->setData(stripped.data());
            }

            // Auto-enable model if it was disabled
            if (model->getState() == PixelOverlayState::Disabled) {
                state->wasDisabled = true;
                model->setState(PixelOverlayState(PixelOverlayState::Enabled));
            }
        }
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}
#endif

bool VideoOutputManager::WaitForPendingTeardowns(int timeoutMs) {
    std::unique_lock<std::mutex> tl(m_teardownMutex);
    if (m_pendingTeardowns <= 0)
        return true;
    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Waiting for %d pending pipeline teardown(s) to release DRM/CMA...\n",
            m_pendingTeardowns);
    bool ok = m_teardownCV.wait_for(tl, std::chrono::milliseconds(timeoutMs),
        [this] { return m_pendingTeardowns <= 0; });
    if (!ok) {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Timed out waiting for pipeline teardowns "
                "(%d still pending after %d ms)\n", m_pendingTeardowns, timeoutMs);
    } else {
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: All pipeline teardowns completed\n");
    }
    return ok;
}

void VideoOutputManager::StopConsumer(ConsumerInfo& consumer) {
#ifdef HAS_GSTREAMER_VIDEO_OUTPUT
    if (!consumer.running)
        return;

    // If this consumer is part of a grouped pipeline (not the owner),
    // stop the pipeline owner instead — that tears down the whole group.
    if (consumer.groupPipelineOwnerIdx >= 0) {
        int ownerIdx = consumer.groupPipelineOwnerIdx;
        consumer.groupPipelineOwnerIdx = -1;
        consumer.running = false;
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' is grouped — stopping group owner\n",
                consumer.name.c_str());
        if (ownerIdx < (int)m_consumers.size() && m_consumers[ownerIdx].running) {
            StopConsumer(m_consumers[ownerIdx]);
        }
        return;
    }

    // Signal the background thread to exit first
    consumer.shutdownRequested = true;

    // Deactivate overlay state before pipeline teardown to prevent
    // the appsink callback from accessing a model during shutdown
    if (consumer.overlayState) {
        consumer.overlayState->active = false;
    }

    // Detach the background thread — it may be blocked in a GStreamer
    // state change with PipeWire locks held, so joining could hang.
    // The thread checks shutdownRequested and will exit eventually.
    try {
        if (consumer.startThread.joinable())
            consumer.startThread.detach();
    } catch (...) {}

    // Move pipeline NULL transition to a detached background thread.
    // gst_element_set_state(NULL) can block on PipeWire locks held by
    // the pipewiresrc element's internal thread during negotiation.
    // Never call it on the main thread or any critical path.
    // The thread decrements m_pendingTeardowns and notifies m_teardownCV
    // when done so callers (Reload, Resume) can wait for CMA to be freed.
    if (consumer.pipeline) {
        GstElement* p = consumer.pipeline;
        consumer.pipeline = nullptr;
        // Release the reserved overlay plane only after the pipeline reaches
        // NULL (which destroys the kmssink and frees the plane in DRM).
        // Releasing it earlier would let a new consumer claim a plane the old
        // kmssink still holds.  -1 if this consumer never got a plane.
        int planeId = consumer.assignedPlaneId;
        consumer.assignedPlaneId = -1;
        {
            std::lock_guard<std::mutex> tl(m_teardownMutex);
            m_pendingTeardowns++;
        }
        std::thread([this, p, planeId]() {
            gst_element_set_state(p, GST_STATE_NULL);
            gst_object_unref(p);
            if (planeId >= 0)
                GStreamerOutput::ReleasePlane(planeId);
            {
                std::lock_guard<std::mutex> tl(m_teardownMutex);
                m_pendingTeardowns--;
            }
            m_teardownCV.notify_all();
        }).detach();
    }

    // Restore overlay model state and remove listener
    if (consumer.overlayState) {
        std::lock_guard<std::mutex> lock(consumer.overlayState->mtx);
        if (consumer.overlayState->wasDisabled && consumer.overlayState->model) {
            consumer.overlayState->model->setState(PixelOverlayState(PixelOverlayState::Disabled));
            consumer.overlayState->wasDisabled = false;
        }
        consumer.overlayState->model = nullptr;
        PixelOverlayManager::INSTANCE.removeModelListener(consumer.overlayModel,
            "VideoOutputManager_" + consumer.pipeWireNodeName);
        consumer.overlayState.reset();
    }

    consumer.running = false;

    // Also mark any grouped members as stopped
    int myIdx = (int)(&consumer - &m_consumers[0]);
    for (auto& c : m_consumers) {
        if (c.groupPipelineOwnerIdx == myIdx) {
            c.groupPipelineOwnerIdx = -1;
            c.running = false;
            LogInfo(VB_MEDIAOUT, "VideoOutputManager: Grouped consumer '%s' stopped (group owner stopped)\n",
                    c.name.c_str());
        }
    }

    LogInfo(VB_MEDIAOUT, "VideoOutputManager: Consumer '%s' stopped\n", consumer.name.c_str());
#endif
}

void VideoOutputManager::StopAllConsumers() {
    for (auto& consumer : m_consumers) {
        StopConsumer(consumer);
    }
}

std::string VideoOutputManager::BuildSdp(const ConsumerInfo& consumer, const std::string& encoding) {
    // Build SDP content describing the RTP stream.
    // See RFC 4566 (SDP), RFC 3984 (H.264 RTP), RFC 4175 (raw video RTP).
    std::string sdp;
    sdp += "v=0\r\n";
    sdp += "o=FPP 0 0 IN IP4 " + consumer.address + "\r\n";
    sdp += "s=FPP Video: " + consumer.name + "\r\n";
    sdp += "c=IN IP4 " + consumer.address + "/32\r\n";
    sdp += "t=0 0\r\n";

    if (encoding == "h264") {
        sdp += "m=video " + std::to_string(consumer.port) + " RTP/AVP 96\r\n";
        sdp += "a=rtpmap:96 H264/90000\r\n";
        sdp += "a=fmtp:96 packetization-mode=1\r\n";
    } else if (encoding == "h265") {
        sdp += "m=video " + std::to_string(consumer.port) + " RTP/AVP 96\r\n";
        sdp += "a=rtpmap:96 H265/90000\r\n";
    } else if (encoding == "mjpeg") {
        sdp += "m=video " + std::to_string(consumer.port) + " RTP/AVP 26\r\n";
        sdp += "a=rtpmap:26 JPEG/90000\r\n";
    } else {
        // raw video (rtpvrawpay) — RFC 4175
        sdp += "m=video " + std::to_string(consumer.port) + " RTP/AVP 96\r\n";
        sdp += "a=rtpmap:96 raw/90000\r\n";
    }

    return sdp;
}

void VideoOutputManager::WriteSdpFile(const ConsumerInfo& consumer, const std::string& encoding) {
    std::string sdp = BuildSdp(consumer, encoding);

    // Write to web-accessible location: /home/fpp/media/config/
    std::string sdpPath = FPP_DIR_MEDIA("/config/rtp_" + consumer.pipeWireNodeName + ".sdp");
    std::ofstream ofs(sdpPath);
    if (ofs.is_open()) {
        ofs << sdp;
        ofs.close();
        LogInfo(VB_MEDIAOUT, "VideoOutputManager: Wrote SDP file %s\n", sdpPath.c_str());
    } else {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager: Failed to write SDP file %s\n", sdpPath.c_str());
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// SAP (Session Announcement Protocol) — RFC 2974
// ──────────────────────────────────────────────────────────────────────────────

static std::string GetLocalIPAddress() {
    // Return the first non-loopback IPv4 address
    struct ifaddrs* ifas = nullptr;
    if (getifaddrs(&ifas) != 0)
        return "0.0.0.0";

    std::string result = "0.0.0.0";
    for (struct ifaddrs* ifa = ifas; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            continue;
        auto* sin = (struct sockaddr_in*)ifa->ifa_addr;
        uint32_t ip = ntohl(sin->sin_addr.s_addr);
        if ((ip >> 24) == 127)
            continue;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
        result = buf;
        break;
    }
    freeifaddrs(ifas);
    return result;
}

std::vector<uint8_t> VideoOutputManager::BuildSAPPacket(const std::string& sourceIP,
                                                        uint16_t msgIdHash,
                                                        const std::string& sdp,
                                                        bool isDeletion) {
    // RFC 2974 SAP packet:
    //   Byte 0: V=1 (bits 7-5), A=0 (bit 4), R=0 (bit 3), T=isDeletion (bit 2),
    //           E=0 (bit 1), C=0 (bit 0)
    //   Byte 1: Auth length = 0
    //   Bytes 2-3: Message ID hash (network byte order)
    //   Bytes 4-7: Originating source (IPv4)
    //   Payload type: "application/sdp\0"
    //   SDP data
    uint8_t header0 = (SAP_VERSION << 5);  // V=1
    if (isDeletion) {
        header0 |= 0x04;  // T=1 (deletion)
    }

    struct in_addr srcAddr;
    inet_pton(AF_INET, sourceIP.c_str(), &srcAddr);

    const uint8_t* ipBytes = reinterpret_cast<const uint8_t*>(&srcAddr.s_addr);
    const uint8_t header[] = {
        header0,
        0,  // auth length
        static_cast<uint8_t>((msgIdHash >> 8) & 0xFF),
        static_cast<uint8_t>(msgIdHash & 0xFF),
        ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]
    };
    const char payloadType[] = "application/sdp";  // sizeof includes NUL terminator

    std::vector<uint8_t> packet;
    packet.reserve(sizeof(header) + sizeof(payloadType) + sdp.size());
    packet.insert(packet.end(), header, header + sizeof(header));
    packet.insert(packet.end(), payloadType, payloadType + sizeof(payloadType));
    packet.insert(packet.end(), sdp.begin(), sdp.end());

    return packet;
}

void VideoOutputManager::StartSAPAnnouncer() {
    if (m_sapRunning.load())
        return;

    // Only start if we have at least one RTP consumer
    bool hasRtp = false;
    for (const auto& c : m_consumers) {
        if (c.type == "rtp" && c.running) {
            hasRtp = true;
            break;
        }
    }
    if (!hasRtp)
        return;

    m_sapRunning = true;
    m_sapThread = std::thread(&VideoOutputManager::SAPAnnounceLoop, this);
}

void VideoOutputManager::StopSAPAnnouncer() {
    if (!m_sapRunning.load())
        return;

    m_sapRunning = false;
    if (m_sapThread.joinable())
        m_sapThread.join();
}

void VideoOutputManager::SAPAnnounceLoop() {
    LogInfo(VB_MEDIAOUT, "VideoOutputManager SAP announcer thread started\n");

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LogErr(VB_MEDIAOUT, "VideoOutputManager SAP: Failed to create socket: %s\n", strerror(errno));
        return;
    }

    int ttl = SAP_TTL;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

    struct sockaddr_in sapAddr;
    memset(&sapAddr, 0, sizeof(sapAddr));
    sapAddr.sin_family = AF_INET;
    sapAddr.sin_port = htons(SAP_PORT);
    inet_pton(AF_INET, SAP_MCAST_ADDR, &sapAddr.sin_addr);

    std::string localIP = GetLocalIPAddress();

    // Build announce & delete packets for each running RTP consumer
    struct SAPEntry {
        std::vector<uint8_t> announcePacket;
        std::vector<uint8_t> deletePacket;
    };
    std::vector<SAPEntry> entries;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& consumer : m_consumers) {
            if (consumer.type != "rtp" || !consumer.running)
                continue;

            std::string enc = consumer.rtpEncoding.empty() ? "raw" : consumer.rtpEncoding;
            std::string sdp = BuildSdp(consumer, enc);

            // Simple hash from consumer id + port
            uint16_t hash = static_cast<uint16_t>(consumer.id * 31 + consumer.port);

            SAPEntry entry;
            entry.announcePacket = BuildSAPPacket(localIP, hash, sdp, false);
            entry.deletePacket = BuildSAPPacket(localIP, hash, sdp, true);
            entries.push_back(std::move(entry));
        }
    }

    if (entries.empty()) {
        LogWarn(VB_MEDIAOUT, "VideoOutputManager SAP: No RTP consumers to announce\n");
    } else {
        LogInfo(VB_MEDIAOUT, "VideoOutputManager SAP: Announcing %d stream(s) to %s:%d every %ds\n",
                (int)entries.size(), SAP_MCAST_ADDR, SAP_PORT, SAP_INTERVAL_S);
    }

    while (m_sapRunning.load()) {
        for (const auto& entry : entries) {
            ssize_t sent = sendto(sock, entry.announcePacket.data(), entry.announcePacket.size(), 0,
                                  (struct sockaddr*)&sapAddr, sizeof(sapAddr));
            if (sent < 0) {
                LogErr(VB_MEDIAOUT, "VideoOutputManager SAP: sendto failed: %s\n", strerror(errno));
            }
        }

        // Sleep for SAP_INTERVAL_S, checking shutdown flag every second
        for (int i = 0; i < SAP_INTERVAL_S && m_sapRunning.load(); i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // Send deletion packets on shutdown
    for (const auto& entry : entries) {
        sendto(sock, entry.deletePacket.data(), entry.deletePacket.size(), 0,
               (struct sockaddr*)&sapAddr, sizeof(sapAddr));
    }

    close(sock);
    LogInfo(VB_MEDIAOUT, "VideoOutputManager SAP announcer thread stopped (deletion packets sent)\n");
}
