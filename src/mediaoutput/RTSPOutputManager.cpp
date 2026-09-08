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

#include "fpp-json.h"
#include "fpphttp.h" // drogon/HTTP helpers used here; see fpphttp_types.h

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include "GStreamerOut.h"
#include "PipeWireGraphConfig.h"
#include "RTSPOutputManager.h"

#ifdef HAS_RTSP_OUTPUT_GSTREAMER

#include <fstream>
#include <sstream>

#include "common.h"
#include "log.h"
#include "settings.h"

// ──────────────────────────────────────────────────────────────────────────────
// Singleton instance
// ──────────────────────────────────────────────────────────────────────────────
static RTSPOutputManager s_rtspOutputManager;
RTSPOutputManager& RTSPOutputManager::INSTANCE = s_rtspOutputManager;

RTSPOutputManager::RTSPOutputManager() {
    m_configPath = FPP_DIR_MEDIA("/config/pipewire-rtsp-outputs.json");
}

RTSPOutputManager::~RTSPOutputManager() {
    Shutdown();
}

std::string RTSPOutputMount::AudioChannel() const {
    return "fpp_rtsp_a_" + std::to_string(id);
}

// ──────────────────────────────────────────────────────────────────────────────
// Helpers
// ──────────────────────────────────────────────────────────────────────────────

// Quote a config-supplied value for a gst_parse_launch description.
//
// Same hazard GstQuote() covers in VideoInputManager: these strings come from
// a JSON file the web UI writes, and a bare space or quote in one of them does
// not fail cleanly -- it re-parses as extra pipeline syntax.
static std::string RtspGstQuote(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        if (c == '\\' || c == '"') {
            out += '\\';
        }
        out += c;
    }
    out += '"';
    return out;
}

// A mount point has to start with '/' and carry nothing that would change the
// meaning of the URL.  An empty or malformed one is replaced rather than
// rejected, so a half-finished config still serves something addressable.
static std::string NormaliseMountPoint(const std::string& raw, int id) {
    std::string mp;
    for (char c : raw) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '_' || c == '-') {
            mp += c;
        }
    }
    if (mp.empty() || mp == "/") {
        mp = "/stream" + std::to_string(id);
    }
    if (mp[0] != '/') {
        mp = "/" + mp;
    }
    return mp;
}

// ──────────────────────────────────────────────────────────────────────────────
// Config
// ──────────────────────────────────────────────────────────────────────────────
bool RTSPOutputManager::LoadConfig() {
    RTSPOutputConfig fresh;

    if (!FileExists(m_configPath)) {
        LogDebug(VB_MEDIAOUT, "RTSPOutputManager: No config at %s\n", m_configPath.c_str());
        std::unique_lock<std::mutex> lock(m_configMutex);
        m_config = std::move(fresh);
        return false;
    }

    std::ifstream ifs(m_configPath);
    if (!ifs.is_open()) {
        LogWarn(VB_MEDIAOUT, "RTSPOutputManager: Cannot open %s\n", m_configPath.c_str());
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, ifs, &root, &errors)) {
        LogWarn(VB_MEDIAOUT, "RTSPOutputManager: JSON parse error: %s\n", errors.c_str());
        return false;
    }

    fresh.enabled = root.get("enabled", false).asBool();
    fresh.port = root.get("port", RTSPOutput::DEFAULT_PORT).asInt();
    if (fresh.port < 1024 || fresh.port > 65535) {
        LogWarn(VB_MEDIAOUT, "RTSPOutputManager: port %d out of range, using %d\n",
                fresh.port, RTSPOutput::DEFAULT_PORT);
        fresh.port = RTSPOutput::DEFAULT_PORT;
    }
    fresh.requireGroupSource = root.get("requireGroupSource", true).asBool();

    const Json::Value& mounts = root["mounts"];
    if (mounts.isArray()) {
        for (const auto& entry : mounts) {
            RTSPOutputMount m;
            m.id = entry.get("id", 0).asInt();
            m.name = entry.get("name", "").asString();
            m.enabled = entry.get("enabled", true).asBool();
            m.mountPoint = NormaliseMountPoint(entry.get("mountPoint", "").asString(), m.id);
            m.sourceNode = entry.get("sourceNode", "").asString();
            m.width = entry.get("width", RTSPOutput::DEFAULT_WIDTH).asInt();
            m.height = entry.get("height", RTSPOutput::DEFAULT_HEIGHT).asInt();
            m.framerate = entry.get("framerate", RTSPOutput::DEFAULT_FRAMERATE).asInt();
            m.videoEncoding = entry.get("videoEncoding", "h264").asString();
            m.videoBitrate = entry.get("videoBitrate", RTSPOutput::DEFAULT_VIDEO_BITRATE).asInt();
            m.audioEnabled = entry.get("audioEnabled", false).asBool();
            m.audioNodeName = entry.get("audioNodeName", "").asString();
            m.audioBitrate = entry.get("audioBitrate", RTSPOutput::DEFAULT_AUDIO_BITRATE).asInt();

            if (m.sourceNode.empty()) {
                LogWarn(VB_MEDIAOUT, "RTSPOutputManager: mount '%s' has no source node, skipping\n",
                        m.name.c_str());
                continue;
            }
            if (m.width <= 0 || m.height <= 0 || m.framerate <= 0) {
                LogWarn(VB_MEDIAOUT, "RTSPOutputManager: mount '%s' has an invalid size/rate "
                                     "(%dx%d@%d), using defaults\n",
                        m.name.c_str(), m.width, m.height, m.framerate);
                m.width = RTSPOutput::DEFAULT_WIDTH;
                m.height = RTSPOutput::DEFAULT_HEIGHT;
                m.framerate = RTSPOutput::DEFAULT_FRAMERATE;
            }
            if (m.audioEnabled && m.audioNodeName.empty()) {
                m.audioNodeName = "fpp_rtsp_audio_" + std::to_string(m.id);
            }
            fresh.mounts.push_back(std::move(m));
        }
    }

    std::unique_lock<std::mutex> lock(m_configMutex);
    m_config = std::move(fresh);
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Pipeline descriptions
// ──────────────────────────────────────────────────────────────────────────────
std::string RTSPOutputManager::BuildFactoryLaunch(const RTSPOutputMount& mount) const {
    std::ostringstream oss;

    // Video: read the intervideo channel the rest of the video plane already
    // publishes on, exactly as the HDMI and RTP consumers do.  timeout is the
    // same 5s the other consumers use -- it must not overflow gint64 once
    // converted to microseconds (see VideoOutputManager::StartConsumer).
    oss << "( intervideosrc timeout=5000000000 channel=" << RtspGstQuote(mount.sourceNode)
        << " ! videoconvert ! videoscale ! videorate"
        << " ! video/x-raw,width=" << mount.width
        << ",height=" << mount.height
        << ",pixel-aspect-ratio=1/1"
        << ",framerate=" << mount.framerate << "/1"
        << " ! queue max-size-buffers=2 leaky=downstream ";

    // Encoders are all configured for live streaming rather than filesize:
    // zerolatency/ultrafast, no B-frames, and a keyframe roughly every two
    // seconds so a client that joins mid-stream gets a picture quickly.
    const int keyIntMax = mount.framerate * 2;
    if (mount.videoEncoding == "h265") {
        oss << "! x265enc tune=zerolatency speed-preset=ultrafast"
            << " bitrate=" << mount.videoBitrate
            << " key-int-max=" << keyIntMax
            << " ! rtph265pay name=pay0 pt=" << RTSPOutput::VIDEO_PAYLOAD_TYPE
            << " config-interval=1 ";
    } else if (mount.videoEncoding == "mjpeg") {
        // Motion JPEG has no inter-frame prediction, so every frame is a
        // keyframe and none of the GOP settings above apply.
        oss << "! jpegenc quality=80"
            << " ! rtpjpegpay name=pay0 pt=26 ";
    } else {
        // h264 is the default and the only encoding every client understands.
        oss << "! x264enc tune=zerolatency speed-preset=ultrafast bframes=0"
            << " bitrate=" << mount.videoBitrate
            << " key-int-max=" << keyIntMax
            << " ! video/x-h264,profile=baseline"
            << " ! rtph264pay name=pay0 pt=" << RTSPOutput::VIDEO_PAYLOAD_TYPE
            << " config-interval=1 ";
    }

    if (mount.audioEnabled) {
        // Audio arrives over an interaudio channel rather than straight from
        // PipeWire, so the PipeWire node lives in the always-running bridge
        // pipeline instead of in here.  A GstRTSPMedia only exists while a
        // client is connected, and a node that came and went with client
        // connections could not be wired up in the mix bus UI ahead of time.
        oss << "  interaudiosrc channel=" << RtspGstQuote(mount.AudioChannel())
            << " ! audioconvert ! audioresample"
            << " ! audio/x-raw,rate=" << RTSPOutput::AUDIO_RATE
            << ",channels=" << RTSPOutput::AUDIO_CHANNELS
            << " ! opusenc bitrate=" << mount.audioBitrate
            << " ! rtpopuspay name=pay1 pt=" << RTSPOutput::AUDIO_PAYLOAD_TYPE << " ";
    }

    oss << ")";
    return oss.str();
}

bool RTSPOutputManager::StartAudioBridge(const RTSPOutputMount& mount) {
    AudioBridge bridge;
    bridge.mountId = mount.id;
    bridge.nodeName = mount.audioNodeName;

    if (m_config.requireGroupSource && !PipeWireGraphFeedsNode(mount.audioNodeName)) {
        // Nothing routes into this node yet, and a pipewiresrc with
        // node.autoconnect=false and nothing linked cannot preroll: starting
        // it would block for 30 seconds and then report a failure for a stream
        // the user has not finished wiring up.  Leave it for the next apply,
        // which the Audio Output Groups page performs anyway.
        LogInfo(VB_MEDIAOUT, "RTSPOutputManager: audio for '%s' is waiting for an Audio "
                             "Output Group to feed '%s'\n",
                mount.name.c_str(), mount.audioNodeName.c_str());
        bridge.waitingForSource = true;
        m_audioBridges.push_back(bridge);
        return false;
    }

    // pipewiresrc -> interaudiosink: holds the PipeWire node open for the
    // whole life of the config so routing can be set up before any client
    // connects, and does only format conversion.  The encode stays in the
    // RTSP media, where it runs only while somebody is watching.
    //
    // The audioconvert/audioresample/audioconvert/audiorate split mirrors
    // OpusRTPManager::CreateSendPipeline: the graph does not necessarily run
    // at 48000, and without a real resampler negotiation never completes and
    // set_state() blocks rather than failing cleanly.
    std::ostringstream oss;
    oss << "pipewiresrc name=pwsrc min-buffers=2 do-timestamp=true"
        << " ! queue max-size-buffers=0 max-size-bytes=0 max-size-time=200000000 leaky=downstream"
        << " ! audioconvert ! audioresample ! audioconvert ! audiorate"
        << " ! audio/x-raw,rate=" << RTSPOutput::AUDIO_RATE
        << ",channels=" << RTSPOutput::AUDIO_CHANNELS
        << " ! interaudiosink sync=false channel=" << RtspGstQuote(mount.AudioChannel());

    std::string desc = oss.str();
    LogInfo(VB_MEDIAOUT, "RTSPOutputManager: audio bridge for '%s': %s\n",
            mount.name.c_str(), desc.c_str());

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(desc.c_str(), &error);
    if (!pipeline) {
        LogErr(VB_MEDIAOUT, "RTSPOutputManager: audio bridge for '%s' failed to build: %s\n",
               mount.name.c_str(), error ? error->message : "unknown error");
        if (error) {
            g_error_free(error);
        }
        return false;
    }
    if (error) {
        LogWarn(VB_MEDIAOUT, "RTSPOutputManager: audio bridge warning for '%s': %s\n",
                mount.name.c_str(), error->message);
        g_error_free(error);
    }

    // Inline GstStructure values containing '/' (node.latency=1024/48000)
    // crash gst_value_deserialize, so these are set after the parse -- the
    // same reason OpusRTPManager sets them here rather than in the string.
    GstElement* pwsrc = gst_bin_get_by_name(GST_BIN(pipeline), "pwsrc");
    if (pwsrc) {
        std::string desc2 = "FPP RTSP Audio: " + mount.name;
        GstStructure* props = gst_structure_new("props",
                                                "node.name", G_TYPE_STRING, mount.audioNodeName.c_str(),
                                                "node.description", G_TYPE_STRING, desc2.c_str(),
                                                "node.autoconnect", G_TYPE_BOOLEAN, FALSE,
                                                "node.latency", G_TYPE_STRING, "1024/48000",
                                                NULL);
        g_object_set(pwsrc, "stream-properties", props, NULL);
        gst_structure_free(props);
        gst_object_unref(pwsrc);
    }

    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        LogErr(VB_MEDIAOUT, "RTSPOutputManager: audio bridge for '%s' failed to start\n",
               mount.name.c_str());
        WarningHolder::AddWarning(31, "RTSP output '" + mount.name + "': audio bridge failed to start");
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return false;
    }

    bridge.pipeline = pipeline;
    m_audioBridges.push_back(bridge);
    return true;
}

void RTSPOutputManager::StopAudioBridges() {
    for (auto& b : m_audioBridges) {
        if (b.pipeline) {
            gst_element_set_state(b.pipeline, GST_STATE_NULL);
            gst_object_unref(b.pipeline);
            b.pipeline = nullptr;
        }
    }
    m_audioBridges.clear();
}

// ──────────────────────────────────────────────────────────────────────────────
// Server lifecycle
// ──────────────────────────────────────────────────────────────────────────────
bool RTSPOutputManager::StartServer() {
    GStreamerOutput::EnsureGStreamerInit();

    setenv("PIPEWIRE_RUNTIME_DIR", "/run/pipewire-fpp", 0);

    // gst-rtsp-server services its clients from a GMainContext, and FPP runs
    // no main loop of its own.  Give the server a private context rather than
    // the default one, so nothing else that later wants the default context
    // (a plugin, a future subsystem) has to share a loop with it.
    m_mainContext = g_main_context_new();
    m_mainLoop = g_main_loop_new(m_mainContext, FALSE);

    m_server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(m_server, std::to_string(m_config.port).c_str());

    GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(m_server);
    int mounted = 0;
    for (const auto& mount : m_config.mounts) {
        if (!mount.enabled) {
            continue;
        }
        std::string launch = BuildFactoryLaunch(mount);
        LogInfo(VB_MEDIAOUT, "RTSPOutputManager: mount %s -> %s\n",
                mount.mountPoint.c_str(), launch.c_str());

        GstRTSPMediaFactory* factory = gst_rtsp_media_factory_new();
        gst_rtsp_media_factory_set_launch(factory, launch.c_str());
        // Shared: every client is served from ONE pipeline, so the encoder
        // runs once no matter how many receivers connect.  Without this each
        // client would get its own x264enc, which a Pi cannot afford.
        gst_rtsp_media_factory_set_shared(factory, TRUE);
        // The sources are live: never let a client's disconnect send EOS and
        // tear the shared media down under the others.
        gst_rtsp_media_factory_set_eos_shutdown(factory, FALSE);
        gst_rtsp_mount_points_add_factory(mounts, mount.mountPoint.c_str(), factory);
        mounted++;
    }
    g_object_unref(mounts);

    if (mounted == 0) {
        LogInfo(VB_MEDIAOUT, "RTSPOutputManager: no enabled mounts, not starting the server\n");
        StopServer();
        return false;
    }

    m_serverSourceId = gst_rtsp_server_attach(m_server, m_mainContext);
    if (m_serverSourceId == 0) {
        LogErr(VB_MEDIAOUT, "RTSPOutputManager: could not bind RTSP port %d "
                            "(in use, or not permitted)\n",
               m_config.port);
        WarningHolder::AddWarning(31, "RTSP output: could not bind port " + std::to_string(m_config.port));
        StopServer();
        return false;
    }

    m_loopThread = std::thread([this]() {
        // The loop must run with the same context the server was attached to,
        // and pushing it as this thread's default keeps any source the server
        // creates on our loop rather than the process default.
        g_main_context_push_thread_default(m_mainContext);
        g_main_loop_run(m_mainLoop);
        g_main_context_pop_thread_default(m_mainContext);
    });

    LogInfo(VB_MEDIAOUT, "RTSPOutputManager: serving %d mount(s) on rtsp://<host>:%d\n",
            mounted, m_config.port);
    return true;
}

void RTSPOutputManager::StopServer() {
    if (m_mainLoop) {
        g_main_loop_quit(m_mainLoop);
    }
    if (m_loopThread.joinable()) {
        m_loopThread.join();
    }
    if (m_serverSourceId != 0) {
        // Only safe now that the loop has stopped: removing the server's
        // source closes its listening socket.
        GSource* src = g_main_context_find_source_by_id(m_mainContext, m_serverSourceId);
        if (src) {
            g_source_destroy(src);
        }
        m_serverSourceId = 0;
    }
    if (m_server) {
        g_object_unref(m_server);
        m_server = nullptr;
    }
    if (m_mainLoop) {
        g_main_loop_unref(m_mainLoop);
        m_mainLoop = nullptr;
    }
    if (m_mainContext) {
        g_main_context_unref(m_mainContext);
        m_mainContext = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────────────────────────────────────
bool RTSPOutputManager::Init() {
    LoadConfig();
    return true;
}

bool RTSPOutputManager::ApplyConfig() {
    std::unique_lock<std::mutex> applyLock(m_applyMutex);

    StopServer();
    StopAudioBridges();
    m_active = false;

    LoadConfig();

    std::unique_lock<std::mutex> lock(m_configMutex);
    if (!m_config.enabled) {
        LogDebug(VB_MEDIAOUT, "RTSPOutputManager: disabled\n");
        return true;
    }
    if (m_config.mounts.empty()) {
        LogInfo(VB_MEDIAOUT, "RTSPOutputManager: enabled but no mounts configured\n");
        return true;
    }

    // Bridges first: the RTSP media's interaudiosrc has nothing to read until
    // its bridge is publishing, and a client can connect the moment the
    // server's socket is up.
    for (const auto& mount : m_config.mounts) {
        if (mount.enabled && mount.audioEnabled) {
            StartAudioBridge(mount);
        }
    }

    bool ok = StartServer();
    m_active = ok;
    return ok;
}

void RTSPOutputManager::Shutdown() {
    std::unique_lock<std::mutex> applyLock(m_applyMutex);
    StopServer();
    StopAudioBridges();
    m_active = false;
}

// ──────────────────────────────────────────────────────────────────────────────
// Status
// ──────────────────────────────────────────────────────────────────────────────
RTSPOutputManager::Status RTSPOutputManager::GetStatus() {
    Status st;
    std::unique_lock<std::mutex> lock(m_configMutex);

    st.serverRunning = m_active.load();
    st.port = m_config.port;

    for (const auto& mount : m_config.mounts) {
        Status::MountStatus ms;
        ms.id = mount.id;
        ms.name = mount.name;
        ms.enabled = mount.enabled;
        ms.audioEnabled = mount.audioEnabled;
        ms.audioNodeName = mount.audioNodeName;
        // Host-relative on purpose: fppd does not know which of its addresses
        // the person reading this reached it on, and the UI does.
        ms.url = "rtsp://<host>:" + std::to_string(m_config.port) + mount.mountPoint;

        for (const auto& b : m_audioBridges) {
            if (b.mountId == mount.id && b.waitingForSource) {
                ms.audioWaitingForSource = true;
                ms.note = "Audio is waiting for an Audio Output Group to feed '" + mount.audioNodeName + "'. Video is unaffected.";
            }
        }
        st.mounts.push_back(std::move(ms));
    }
    return st;
}

HttpResponsePtr RTSPOutputManager::render_GET(const HttpRequestPtr& req) {
    std::string url(req->path());

    if (url.find("/rtspoutput/") == 0) {
        url = url.substr(12);
    } else if (url == "/rtspoutput") {
        url = "status";
    }

    if (url == "status") {
        Status st = GetStatus();
        Json::Value result;
        result["active"] = st.serverRunning;
        result["port"] = st.port;

        Json::Value mounts(Json::arrayValue);
        for (const auto& m : st.mounts) {
            Json::Value mj;
            mj["id"] = m.id;
            mj["name"] = m.name;
            mj["url"] = m.url;
            mj["enabled"] = m.enabled;
            mj["audioEnabled"] = m.audioEnabled;
            if (m.audioEnabled) {
                mj["audioNodeName"] = m.audioNodeName;
            }
            // Kept distinct from an error for the same reason Opus RTP does:
            // the UI renders a failure in red and this as guidance.
            if (m.audioWaitingForSource) {
                mj["audioWaitingForSource"] = true;
            }
            if (!m.note.empty()) {
                mj["note"] = m.note;
            }
            mounts.append(mj);
        }
        result["mounts"] = mounts;

        Json::StreamWriterBuilder wbuilder;
        wbuilder["indentation"] = "";
        return makeStringResponse(Json::writeString(wbuilder, result), 200, "application/json");
    }

    return makeStringResponse("{\"error\":\"unknown endpoint\"}", 404, "application/json");
}

#endif // HAS_RTSP_OUTPUT_GSTREAMER
