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

#include <mutex>
#include <set>

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
    m_consumersPath = FPP_DIR_MEDIA("/config/pipewire-video-consumers.json");
}

RTSPOutputManager::~RTSPOutputManager() {
    Shutdown();
}

std::string RTSPOutputMount::AudioChannel() const {
    return audioNodeName + "_ch";
}

std::string RTSPOutputMount::ResolveSourceNode() const {
    if (!sourceNode.empty()) {
        return sourceNode;
    }
    if (!streamSlots.empty()) {
        // A group aimed at a playback stream slot has no named input source.
        // The slot's intervideo channel exists whether or not anything is
        // playing, and intervideosrc holds the mount up on black until it is,
        // which is what a client watching a stream between songs should see.
        // Only the first slot can be served: one mount is one channel.
        return "fppd_video_stream_" + std::to_string(streamSlots.front());
    }
    return "";
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
// The warnings the last apply raised, retracted by the next: saving the
// config again, or the Restart that follows freeing the port, is how a user
// retries, and they used to stay up until fppd restarted.  RemoveWarning()
// needs the exact text, so it is kept as raised.
static std::mutex s_rtspWarningsLock;
static std::set<std::string> s_rtspWarnings;

static void RaiseRtspWarning(const std::string& w) {
    std::lock_guard<std::mutex> l(s_rtspWarningsLock);
    s_rtspWarnings.insert(w);
    WarningHolder::AddWarning(31, w);
}

static void RetractRtspWarnings() {
    std::lock_guard<std::mutex> l(s_rtspWarningsLock);
    for (const auto& w : s_rtspWarnings) {
        WarningHolder::RemoveWarning(31, w);
    }
    s_rtspWarnings.clear();
}

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

    // asBool()/asInt() throw on a value of the wrong type ("enabled": "yes"),
    // and this runs at startup with nothing above to catch it: fppd crash
    // looped on a hand-edited file.  Keep the last good config instead.
    try {
        fresh.enabled = root.get("enabled", false).asBool();
        fresh.port = root.get("port", RTSPOutput::DEFAULT_PORT).asInt();
        fresh.requireGroupSource = root.get("requireGroupSource", true).asBool();
        fresh.latencyMs = root.get("latencyMs", RTSPOutput::DEFAULT_LATENCY_MS).asInt();
    } catch (const std::exception& ex) {
        LogWarn(VB_MEDIAOUT, "RTSPOutputManager: ignoring %s, a value has the wrong type: %s\n",
                m_configPath.c_str(), ex.what());
        return false;
    }
    if (fresh.port < 1024 || fresh.port > 65535) {
        LogWarn(VB_MEDIAOUT, "RTSPOutputManager: port %d out of range, using %d\n",
                fresh.port, RTSPOutput::DEFAULT_PORT);
        fresh.port = RTSPOutput::DEFAULT_PORT;
    }
    if (fresh.latencyMs < 0 || fresh.latencyMs > 5000) {
        LogWarn(VB_MEDIAOUT, "RTSPOutputManager: latencyMs %d out of range, using %d\n",
                fresh.latencyMs, RTSPOutput::DEFAULT_LATENCY_MS);
        fresh.latencyMs = RTSPOutput::DEFAULT_LATENCY_MS;
    }

    LoadMounts(fresh);

    std::unique_lock<std::mutex> lock(m_configMutex);
    m_config = std::move(fresh);
    return true;
}

// Streams come from the Video Output Groups, flattened by the web UI into the
// same consumer list VideoOutputManager reads. Taking them from there rather
// than from our own config is the whole point: a group owns its video source,
// so an RTSP stream inherits it instead of naming it a second time where the
// two could drift apart -- and it picks up stream-slot groups for free.
void RTSPOutputManager::LoadMounts(RTSPOutputConfig& cfg) {
    if (!FileExists(m_consumersPath)) {
        LogDebug(VB_MEDIAOUT, "RTSPOutputManager: no consumer config at %s\n", m_consumersPath.c_str());
        return;
    }
    std::ifstream ifs(m_consumersPath);
    if (!ifs.is_open()) {
        LogWarn(VB_MEDIAOUT, "RTSPOutputManager: cannot open %s\n", m_consumersPath.c_str());
        return;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, ifs, &root, &errors)) {
        LogWarn(VB_MEDIAOUT, "RTSPOutputManager: consumer JSON parse error: %s\n", errors.c_str());
        return;
    }
    if (!root.isArray()) {
        return;
    }

    for (const auto& entry : root) {
        RTSPOutputMount m;
        // as in LoadConfig(): a value of the wrong type skips that stream
        try {
            if (!entry.isObject() || entry.get("type", "").asString() != "rtsp") {
                continue;
            }
            m.name = entry.get("name", "").asString();
            m.mountPoint = NormaliseMountPoint(entry.get("mountPoint", "").asString(),
                                               static_cast<int>(cfg.mounts.size() + 1));
            m.sourceNode = entry.get("sourceNode", "").asString();
            if (entry.isMember("streamSlots") && entry["streamSlots"].isArray()) {
                for (const auto& sl : entry["streamSlots"]) {
                    m.streamSlots.push_back(sl.asInt());
                }
            }
            m.width = entry.get("width", RTSPOutput::DEFAULT_WIDTH).asInt();
            m.height = entry.get("height", RTSPOutput::DEFAULT_HEIGHT).asInt();
            m.framerate = entry.get("framerate", RTSPOutput::DEFAULT_FRAMERATE).asInt();
            m.videoEncoding = entry.get("videoEncoding", "h264").asString();
            m.videoBitrate = entry.get("videoBitrate", RTSPOutput::DEFAULT_VIDEO_BITRATE).asInt();
            m.audioEnabled = entry.get("audioEnabled", false).asBool();
            m.audioNodeName = entry.get("audioNodeName", "").asString();
            m.audioBitrate = entry.get("audioBitrate", RTSPOutput::DEFAULT_AUDIO_BITRATE).asInt();
        } catch (const std::exception& ex) {
            LogWarn(VB_MEDIAOUT, "RTSPOutputManager: skipping a stream in %s, a value has the wrong type: %s\n",
                    m_consumersPath.c_str(), ex.what());
            continue;
        }

        if (m.ResolveSourceNode().empty()) {
            // The group names neither a video source nor a stream slot, so
            // there is no channel to read and the mount would serve nothing.
            LogWarn(VB_MEDIAOUT, "RTSPOutputManager: '%s' has no video source, skipping\n",
                    m.name.c_str());
            continue;
        }
        if (m.width <= 0 || m.height <= 0 || m.framerate <= 0) {
            LogWarn(VB_MEDIAOUT, "RTSPOutputManager: '%s' has an invalid size/rate (%dx%d@%d), "
                                 "using defaults\n",
                    m.name.c_str(), m.width, m.height, m.framerate);
            m.width = RTSPOutput::DEFAULT_WIDTH;
            m.height = RTSPOutput::DEFAULT_HEIGHT;
            m.framerate = RTSPOutput::DEFAULT_FRAMERATE;
        }
        if (m.audioEnabled && m.audioNodeName.empty()) {
            LogWarn(VB_MEDIAOUT, "RTSPOutputManager: '%s' wants audio but has no node name, "
                                 "serving video only\n",
                    m.name.c_str());
            m.audioEnabled = false;
        }
        cfg.mounts.push_back(std::move(m));
    }
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
    oss << "( intervideosrc timeout=5000000000 channel=" << RtspGstQuote(mount.ResolveSourceNode())
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

bool RTSPOutputManager::StartAudioBridge(const RTSPOutputMount& mount, bool requireGroupSource,
                                         std::vector<AudioBridge>& bridges) {
    AudioBridge bridge;
    bridge.mountPoint = mount.mountPoint;
    bridge.nodeName = mount.audioNodeName;

    if (requireGroupSource && !PipeWireGraphFeedsNode(mount.audioNodeName)) {
        // Nothing routes into this node yet, and a pipewiresrc with
        // node.autoconnect=false and nothing linked cannot preroll: starting
        // it would block for 30 seconds and then report a failure for a stream
        // the user has not finished wiring up.  Leave it for the next apply,
        // which the Audio Output Groups page performs anyway.
        LogInfo(VB_MEDIAOUT, "RTSPOutputManager: audio for '%s' is waiting for an Audio "
                             "Output Group to feed '%s'\n",
                mount.name.c_str(), mount.audioNodeName.c_str());
        bridge.waitingForSource = true;
        bridges.push_back(bridge);
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

    // Started on its own thread.  A pipewiresrc whose node nothing feeds
    // (requireGroupSource off) takes 30 s to fail the state change, and the
    // caller is ApplyConfig() -- fppd's main loop for reloadRTSPOutputs, and
    // startup.  The thread holds its own ref; the bridge entry owns the
    // pipeline and StopAudioBridges() tears it down whether or not the start
    // has finished.
    bridge.pipeline = pipeline;
    GstElement* startRef = GST_ELEMENT(gst_object_ref(pipeline));
    std::string mountName = mount.name;
    std::thread([startRef, mountName]() {
        SetThreadName("FPP-RTSPAudio");
        if (gst_element_set_state(startRef, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
            LogErr(VB_MEDIAOUT, "RTSPOutputManager: audio bridge for '%s' failed to start\n",
                   mountName.c_str());
            RaiseRtspWarning("RTSP output '" + mountName + "': audio bridge failed to start");
        }
        gst_object_unref(startRef);
    }).detach();

    bridges.push_back(bridge);
    return true;
}

void RTSPOutputManager::StopAudioBridges() {
    // Taken out under m_configMutex: GetStatus() walks the vector on HTTP
    // threads, and clearing it under only the apply lock freed elements out
    // from under it.  Stopped with the lock released.
    std::vector<AudioBridge> bridges;
    {
        std::unique_lock<std::mutex> lock(m_configMutex);
        bridges.swap(m_audioBridges);
    }
    for (auto& b : bridges) {
        if (b.pipeline) {
            // Bounded: a bridge whose start is still stuck in pipewiresrc
            // holds the state lock, and this runs on the main loop and in
            // Shutdown().  A wedged one is left to the reaper.
            if (!GStreamerOutput::SetPipelineToNullBounded(b.pipeline, 2000)) {
                LogWarn(VB_MEDIAOUT, "RTSPOutputManager: audio bridge for '%s' did not stop, abandoning it\n",
                        b.mountPoint.c_str());
            }
            gst_object_unref(b.pipeline);
            b.pipeline = nullptr;
        }
    }
}

// Concurrent RTSP sessions.  One shared encoder serves them all, but each
// still gets its own transport.
static constexpr guint RTSP_MAX_SESSIONS = 32;

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
        // Without this each media keeps gst-rtsp-server's 200ms default, which
        // a viewer feels as lag for no benefit on a live local source.
        gst_rtsp_media_factory_set_latency(factory, m_config.latencyMs);
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
        RaiseRtspWarning("RTSP output: could not bind port " + std::to_string(m_config.port));
        StopServer();
        return false;
    }

    // A UDP viewer that vanishes without a TEARDOWN (a phone app killed, a
    // dropped network) leaves its session behind, and the server goes on
    // sending its RTP into the void.  The session times out (60s by
    // default), but only once someone runs the pool's cleanup -- which
    // gst-rtsp-server leaves to the application.  The cap bounds what a
    // crowd of viewers, or one client opening sessions in a loop, can cost.
    if (GstRTSPSessionPool* pool = gst_rtsp_server_get_session_pool(m_server)) {
        gst_rtsp_session_pool_set_max_sessions(pool, RTSP_MAX_SESSIONS);
        g_object_unref(pool);
    }
    m_sessionCleanup = g_timeout_source_new_seconds(2);
    g_source_set_callback(
        m_sessionCleanup,
        [](gpointer data) -> gboolean {
            GstRTSPSessionPool* pool = gst_rtsp_server_get_session_pool(static_cast<GstRTSPServer*>(data));
            if (pool) {
                guint expired = gst_rtsp_session_pool_cleanup(pool);
                if (expired) {
                    LogInfo(VB_MEDIAOUT, "RTSPOutputManager: expired %u session(s) whose client went away\n", expired);
                }
                g_object_unref(pool);
            }
            return G_SOURCE_CONTINUE;
        },
        m_server, nullptr);
    g_source_attach(m_sessionCleanup, m_mainContext);

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
    if (m_server) {
        // Close the connected clients and their sessions first, while the loop
        // that services them still runs.  Quitting the loop and dropping the
        // server left them connected: their media (intervideosrc -> x264enc)
        // kept encoding through every reload and shutdown, a viewer stayed on
        // the old config, and its context held the old listening socket.
        g_list_free_full(gst_rtsp_server_client_filter(
                             m_server,
                             [](GstRTSPServer*, GstRTSPClient*, gpointer) { return GST_RTSP_FILTER_REMOVE; },
                             nullptr),
                         g_object_unref);
        if (GstRTSPSessionPool* pool = gst_rtsp_server_get_session_pool(m_server)) {
            g_list_free_full(gst_rtsp_session_pool_filter(
                                 pool,
                                 [](GstRTSPSessionPool*, GstRTSPSession*, gpointer) { return GST_RTSP_FILTER_REMOVE; },
                                 nullptr),
                             g_object_unref);
            g_object_unref(pool);
        }
    }
    if (m_mainLoop) {
        g_main_loop_quit(m_mainLoop);
    }
    if (m_loopThread.joinable()) {
        m_loopThread.join();
    }
    if (m_sessionCleanup) {
        // It points at m_server, which goes below.
        g_source_destroy(m_sessionCleanup);
        g_source_unref(m_sessionCleanup);
        m_sessionCleanup = nullptr;
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

    // Before anything below builds a pipeline.  The audio bridges are parsed
    // before StartServer(), and at boot nothing has initialised GStreamer by
    // then: gst_parse_launch() crashed in gst_value_deserialize(), and the
    // crash handler's exit() then hung in this object's destructor on the
    // apply lock until the crash watchdog killed fppd -- on every boot.
    GStreamerOutput::EnsureGStreamerInit();

    StopServer();
    StopAudioBridges();
    m_active = false;
    RetractRtspWarnings();

    LoadConfig();

    RTSPOutputConfig cfg;
    {
        std::unique_lock<std::mutex> lock(m_configMutex);
        cfg = m_config;
    }
    if (!cfg.enabled) {
        LogDebug(VB_MEDIAOUT, "RTSPOutputManager: disabled\n");
        return true;
    }
    if (cfg.mounts.empty()) {
        LogInfo(VB_MEDIAOUT, "RTSPOutputManager: enabled but no mounts configured\n");
        return true;
    }

    // Bridges first: the RTSP media's interaudiosrc has nothing to read until
    // its bridge is publishing, and a client can connect the moment the
    // server's socket is up.  Built without m_configMutex (see
    // StartAudioBridge()).
    std::vector<AudioBridge> bridges;
    for (const auto& mount : cfg.mounts) {
        if (mount.audioEnabled) {
            StartAudioBridge(mount, cfg.requireGroupSource, bridges);
        }
    }

    std::unique_lock<std::mutex> lock(m_configMutex);
    m_audioBridges = std::move(bridges);
    bool ok = StartServer();
    m_active = ok;
    return ok;
}

bool RTSPOutputManager::ServesStreamSlot(int slot) {
    std::unique_lock<std::mutex> lock(m_configMutex);
    if (!m_config.enabled) {
        return false;
    }
    std::string channel = "fppd_video_stream_" + std::to_string(slot);
    for (const auto& mount : m_config.mounts) {
        if (mount.ResolveSourceNode() == channel) {
            return true;
        }
    }
    return false;
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
    st.latencyMs = m_config.latencyMs;

    for (const auto& mount : m_config.mounts) {
        Status::MountStatus ms;
        ms.name = mount.name;
        ms.mountPoint = mount.mountPoint;
        ms.enabled = true;
        ms.audioEnabled = mount.audioEnabled;
        ms.audioNodeName = mount.audioNodeName;
        // Host-relative on purpose: fppd does not know which of its addresses
        // the person reading this reached it on, and the UI does.
        ms.url = "rtsp://<host>:" + std::to_string(m_config.port) + mount.mountPoint;

        for (const auto& b : m_audioBridges) {
            if (b.mountPoint == mount.mountPoint && b.waitingForSource) {
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
        result["latencyMs"] = st.latencyMs;

        Json::Value mounts(Json::arrayValue);
        for (const auto& m : st.mounts) {
            Json::Value mj;
            mj["name"] = m.name;
            mj["mountPoint"] = m.mountPoint;
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
