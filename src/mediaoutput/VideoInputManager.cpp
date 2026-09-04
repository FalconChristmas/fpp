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

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds
#include <cmath>

#include "V4L2Device.h"
#include "VideoInputManager.h"
#include "VideoOutputManager.h"
#include "common.h"
#include "settings.h"
#include "log.h"

#include <fstream>
#include <sstream>

#include "fpp-json.h"

#ifdef HAS_GSTREAMER_VIDEO_INPUT
#include <gst/app/gstappsink.h>
#endif

// pipewiresink mode=provide enum value (GST_PIPEWIRE_SINK_MODE_PROVIDE)
static constexpr int PIPEWIRE_SINK_MODE_PROVIDE = 2;

// Quote a config-derived value for a gst_parse_launch() description.
//
// That description is a grammar, not a format string: space, '!', ',' and '='
// are all syntax. Splicing a raw config value in lets it reshape the pipeline -
// an RTSP URI containing a space or a comma turns the rest of the description
// into something else entirely, and the failure does not surface as the GError
// the callers already check. It has been observed faulting inside libgstreamer's
// own parser (gst_value_deserialize <- gst_caps_from_string <-
// gst_parse_launch_full), which takes fppd down with it and, since the source is
// re-started on the next reload, crash-loops the daemon.
//
// Quoting fixes it: inside quotes the parser takes the value literally, having
// first un-escaped backslash sequences - so backslashes and quotes in the value
// must themselves be escaped. Quoting a value that needed no escaping parses to
// exactly the same string, so this is safe to apply unconditionally.
static std::string GstQuote(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 2);
    out += '"';
    for (char c : value) {
        if (c == '\\' || c == '"') {
            out += '\\';
        }
        out += c;
    }
    out += '"';
    return out;
}

// Sanitise a URI down to URL-safe characters before it is spliced into a
// single-quoted shell argument, so nothing in it can close the quote.
static std::string ShellSafeUri(const std::string& uri) {
    std::string out;
    out.reserve(uri.size());
    for (char c : uri) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == ':' || c == '/'
            || c == '.' || c == '-' || c == '_' || c == '~' || c == '?'
            || c == '&' || c == '=' || c == '%' || c == '+' || c == '@') {
            out += c;
        }
    }
    return out;
}

// What a yt-dlp resolution produced.
struct YtDlpResult {
    std::string url;         // empty when resolution failed
    double fps = 0.0;        // 0 when fps wasn't requested, or didn't parse
    std::string diagnostics; // yt-dlp's own error text, for the log
};

// Resolve a page URL to a directly playable stream URL via yt-dlp.
//
// stderr used to be sent to /dev/null, which made every failure look identical
// in the log - "returned no valid URL" - while the reason for it was thrown
// away. The packaged yt-dlp goes stale against YouTube every few months and
// then fails with a specific, actionable message ("The page needs to be
// reloaded", "Sign in to confirm you're not a bot"), so that is the failure
// operators actually hit and the one the log could least afford to swallow.
// stderr is folded into the capture and reported instead: yt-dlp's own
// diagnostics are line-oriented and never begin with "http", so the URL line
// stays unambiguous among them.
static YtDlpResult RunYtDlp(const std::string& format, const std::string& uri, bool wantFps) {
    YtDlpResult res;
    std::string cmd = "yt-dlp -f '" + format + "' --print url";
    if (wantFps) {
        cmd += " --print fps";
    }
    cmd += " '" + ShellSafeUri(uri) + "' 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        res.diagnostics = "could not run yt-dlp";
        return res;
    }
    std::string output;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) {
        output += buf;
    }
    // pclose() may return -1 when SA_NOCLDWAIT is set on SIGCHLD (the kernel
    // auto-reaps the child, so waitpid fails with ECHILD), so the exit status
    // can't be trusted here - validate the output content instead.
    pclose(pipe);

    std::istringstream iss(output);
    std::string line;
    bool fpsLineNext = false;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (fpsLineNext) {
            // --print fps emits the framerate on the line right after the URL.
            fpsLineNext = false;
            try {
                res.fps = std::stod(line);
            } catch (...) {
                res.fps = 0.0;
            }
        } else if (res.url.empty() && line.rfind("http", 0) == 0) {
            res.url = line;
            fpsLineNext = wantFps;
        } else if (res.url.empty() && res.diagnostics.size() < 400) {
            if (!res.diagnostics.empty()) {
                res.diagnostics += " | ";
            }
            res.diagnostics += line;
        }
    }
    return res;
}

// Adopt the stream's native framerate, but never above the configured one -
// don't push a source to 60fps just because the stream is 60fps natively when
// the user asked for 30. videorate handles the downsampling.
static void ApplyDetectedFps(double detectedFps, int& framerate, const char* name) {
    if (detectedFps < 1.0 || detectedFps > 120.0) {
        LogInfo(VB_MEDIAOUT, "VideoInputManager: yt-dlp fps not available for '%s', using config %dfps\n",
                name, framerate);
        return;
    }
    int detected = (int)std::lround(detectedFps);
    int configFps = framerate;
    framerate = std::min(detected, configFps);
    LogInfo(VB_MEDIAOUT, "VideoInputManager: yt-dlp detected %dfps for '%s', "
            "capped to configured %dfps -> using %dfps\n",
            detected, name, configFps, framerate);
}

// Control characters have no representation in the description grammar and
// never belong in a URI, device path or element name, so reject them up front
// rather than trying to encode them. Checked once per source in StartSource()
// instead of at each of the seven splice sites.
static bool GstValueUsable(const std::string& value, const char* what,
                           const std::string& sourceName) {
    for (unsigned char c : value) {
        if (c < 0x20 || c == 0x7f) {
            LogWarn(VB_MEDIAOUT, "VideoInputManager: %s for '%s' contains a control character\n",
                    what, sourceName.c_str());
            WarningHolder::AddWarning(56, "Video input '" + sourceName + "' has an invalid " + what);
            return false;
        }
    }
    return true;
}

VideoInputManager& VideoInputManager::Instance() {
    static VideoInputManager instance;
    return instance;
}

VideoInputManager::~VideoInputManager() {
    Shutdown();
}

void VideoInputManager::Init() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized)
        return;

    // Only init if PipeWire backend is active (Simple or Advanced).
    // Simple PipeWire mode does not configure video input sources by itself,
    // but the manager is still safe to leave initialised.
    if (!isPipeWireBackend()) {
        std::string backend = getSetting("MediaBackend");
        LogDebug(VB_MEDIAOUT, "VideoInputManager: Skipping init (MediaBackend=%s, not pipewire)\n", backend.c_str());
        return;
    }

    m_initialized = true;
    // A prior Shutdown() may have latched m_shutdownDone; since we're
    // (re)starting sources here, a future Shutdown() must be allowed to
    // run its teardown again.
    m_shutdownDone = false;

    if (LoadConfig()) {
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Loaded %d source(s), starting enabled ones\n",
                (int)m_sources.size());
        for (auto& source : m_sources) {
            if (source.enabled) {
                StartSource(source);
            }
        }
    } else {
        LogDebug(VB_MEDIAOUT, "VideoInputManager: No video input sources configured\n");
    }
}

void VideoInputManager::Reload() {
    LogInfo(VB_MEDIAOUT, "VideoInputManager: Reloading configuration\n");

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        StopAllSources();
        m_sources.clear();
    }

    // Join the teardown threads spawned by the StopAllSources() above (plus
    // any still outstanding from an earlier cycle) before touching the
    // devices/URIs again — otherwise StartSource() below can race the old
    // pipeline's gst_element_set_state(..., NULL) and fail to reopen a
    // v4l2/rtsp resource that's still held (EBUSY).
    JoinTeardownThreads();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialized = true;
    // See Init(): allow a subsequent Shutdown() to run its teardown again.
    m_shutdownDone = false;

    if (LoadConfig()) {
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Reloaded with %d source(s)\n", (int)m_sources.size());
        for (auto& source : m_sources) {
            if (source.enabled) {
                StartSource(source);
            }
        }
    }
}

void VideoInputManager::Shutdown() {
    // Idempotent: main() calls this at shutdown and ~VideoInputManager() calls
    // it again at static-destruction time. Run the teardown only once.
    if (m_shutdownDone.exchange(true)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        StopAllSources();
        m_sources.clear();
        m_initialized = false;
    }
    // Make sure every teardown thread spawned above (and any left over from
    // a prior Reload) has actually finished before we consider Shutdown()
    // complete, so nothing outlives the process.
    JoinTeardownThreads();
    LogInfo(VB_MEDIAOUT, "VideoInputManager: Shutdown complete\n");
}

bool VideoInputManager::HasSources() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_sources.empty();
}

bool VideoInputManager::HasActiveSources() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_sources) {
        if (s.running)
            return true;
    }
    return false;
}

int VideoInputManager::ActiveSourceCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int count = 0;
    for (const auto& s : m_sources) {
        if (s.running)
            count++;
    }
    return count;
}

std::string VideoInputManager::GetSourceNodeName(int sourceId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_sources) {
        if (s.id == sourceId && s.running)
            return s.pipeWireNodeName;
    }
    return "";
}

std::vector<std::pair<int, std::string>> VideoInputManager::GetSourceList() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<int, std::string>> result;
    for (const auto& s : m_sources) {
        result.emplace_back(s.id, s.pipeWireNodeName);
    }
    return result;
}

int VideoInputManager::GetSourceFramerate(const std::string& channelName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_sources) {
        if (s.pipeWireNodeName == channelName)
            return s.framerate;
    }
    return 0;
}

bool VideoInputManager::LoadConfig() {
    std::string configPath = FPP_DIR_MEDIA("/config/pipewire-video-input-sources-gen.json");

    if (!FileExists(configPath)) {
        LogDebug(VB_MEDIAOUT, "VideoInputManager: No config at %s\n", configPath.c_str());
        return false;
    }

    std::ifstream ifs(configPath);
    if (!ifs.is_open()) {
        LogWarn(VB_MEDIAOUT, "VideoInputManager: Cannot open %s\n", configPath.c_str());
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, ifs, &root, &errors)) {
        LogWarn(VB_MEDIAOUT, "VideoInputManager: JSON parse error: %s\n", errors.c_str());
        return false;
    }

    if (!root.isArray()) {
        LogWarn(VB_MEDIAOUT, "VideoInputManager: Config is not an array\n");
        return false;
    }

    for (const auto& entry : root) {
        SourceInfo si;
        si.id = entry.get("id", 0).asInt();
        si.name = entry.get("name", "").asString();
        si.type = entry.get("type", "").asString();
        si.pipeWireNodeName = entry.get("pipeWireNodeName", "").asString();
        si.enabled = entry.get("enabled", true).asBool();

        si.width = entry.get("width", 320).asInt();
        si.height = entry.get("height", 240).asInt();
        si.framerate = entry.get("framerate", 10).asInt();

        if (si.type == "videotestsrc") {
            si.pattern = entry.get("pattern", "smpte").asString();
        } else if (si.type == "v4l2src") {
            si.device = entry.get("device", "/dev/video0").asString();
            si.powerLineFrequency = entry.get("powerLineFrequency", -1).asInt();
            si.exposureMode = entry.get("exposureMode", "camera").asString();
            si.exposureTime100us = entry.get("exposureTime100us", -1).asInt();
            si.dynamicFramerate = entry.get("dynamicFramerate", -1).asInt();
            if (si.powerLineFrequency < 0 || si.powerLineFrequency > 2)
                si.powerLineFrequency = -1;
            if (si.exposureMode != "auto" && si.exposureMode != "manual")
                si.exposureMode = "camera";
            if (si.dynamicFramerate < 0 || si.dynamicFramerate > 1)
                si.dynamicFramerate = -1;
            // 100us units: 1 .. 1s.  The device's own range is applied on
            // top of this in V4L2Device::ApplyControls.
            if (si.exposureTime100us < 1 || si.exposureTime100us > 10000)
                si.exposureTime100us = -1;
        } else if (si.type == "rtspsrc") {
            si.uri = entry.get("uri", "").asString();
            si.latency = entry.get("latency", 200).asInt();
            if (si.latency < 0) si.latency = 0;
            if (si.latency > 10000) si.latency = 10000;
        } else if (si.type == "urisrc") {
            si.uri = entry.get("uri", "").asString();
            si.bufferSec = entry.get("bufferSec", 3.0).asDouble();
            if (si.bufferSec < 0) si.bufferSec = 0;
            if (si.bufferSec > 30) si.bufferSec = 30;
        } else if (si.type == "rtpsrc") {
            si.port = entry.get("port", 5004).asInt();
            si.encoding = entry.get("encoding", "H264").asString();
            si.multicastGroup = entry.get("multicastGroup", "").asString();
            if (si.port < 1024) si.port = 1024;
            if (si.port > 65535) si.port = 65535;
        }

        // Audio extraction settings
        si.audioEnabled = entry.get("audioEnabled", false).asBool();
        si.audioPipeWireNodeName = entry.get("audioPipeWireNodeName", "").asString();

        if (si.type.empty() || si.pipeWireNodeName.empty()) {
            LogWarn(VB_MEDIAOUT, "VideoInputManager: Skipping source with missing type or node name\n");
            continue;
        }

        // Clamp to reasonable bounds
        if (si.width < 16) si.width = 16;
        if (si.width > 3840) si.width = 3840;
        if (si.height < 16) si.height = 16;
        if (si.height > 2160) si.height = 2160;
        if (si.framerate < 1) si.framerate = 1;
        if (si.framerate > 60) si.framerate = 60;

        m_sources.push_back(std::move(si));
    }

    return !m_sources.empty();
}

std::string VideoInputManager::BuildDeviceCaps(const SourceInfo& source) {
    std::vector<V4L2Device::Mode> modes = V4L2Device::EnumerateModes(source.device);
    if (modes.empty()) {
        LogInfo(VB_MEDIAOUT, "VideoInputManager: %s enumerated no capture modes, "
                             "letting the device pick its own\n", source.device.c_str());
        return "";
    }

    V4L2Device::Mode chosen;
    if (!V4L2Device::SelectMode(modes, source.width, source.height, source.framerate, chosen))
        return "";

    std::string caps = V4L2Device::ModeToCaps(chosen);
    if (caps.empty()) {
        LogInfo(VB_MEDIAOUT, "VideoInputManager: %s best mode is %s (no caps mapping), "
                             "letting the device pick its own\n",
                source.device.c_str(), chosen.fourcc.c_str());
        return "";
    }

    if (chosen.width != source.width || chosen.height != source.height ||
        chosen.fps() < source.framerate - 0.01) {
        // Worth saying out loud: this is the difference between what the
        // operator typed and what the camera can actually deliver, and the
        // scaler silently papering over it is what made the old behaviour
        // hard to spot.
        LogInfo(VB_MEDIAOUT, "VideoInputManager: %s has no %dx%d@%d mode; capturing %s %dx%d@%.3g "
                             "and converting\n",
                source.device.c_str(), source.width, source.height, source.framerate,
                chosen.fourcc.c_str(), chosen.width, chosen.height, chosen.fps());
    } else {
        LogInfo(VB_MEDIAOUT, "VideoInputManager: %s capturing %s %dx%d@%.3g\n",
                source.device.c_str(), chosen.fourcc.c_str(), chosen.width, chosen.height, chosen.fps());
    }

    return caps;
}

bool VideoInputManager::StartSource(SourceInfo& source) {
#ifdef HAS_GSTREAMER_VIDEO_INPUT
    if (source.running) {
        LogWarn(VB_MEDIAOUT, "VideoInputManager: Source '%s' already running\n", source.name.c_str());
        return true;
    }

    setenv("PIPEWIRE_RUNTIME_DIR", "/run/pipewire-fpp", 0);

    if (!GstValueUsable(source.uri, "URI", source.name) ||
        !GstValueUsable(source.device, "capture device path", source.name) ||
        !GstValueUsable(source.pattern, "test pattern", source.name) ||
        !GstValueUsable(source.multicastGroup, "multicast group", source.name) ||
        !GstValueUsable(source.pipeWireNodeName, "node name", source.name)) {
        return false;
    }

    // Build the source element portion of the pipeline
    std::string srcElement;
    bool useDecodebin = false;
    bool useUridecodebin = false;
    if (source.type == "videotestsrc") {
        srcElement = "videotestsrc is-live=true";
        if (!source.pattern.empty()) {
            srcElement += " pattern=" + GstQuote(source.pattern);
        }
    } else if (source.type == "v4l2src") {
        if (source.device.empty()) {
            LogWarn(VB_MEDIAOUT, "VideoInputManager: v4l2src '%s' has no device path\n", source.name.c_str());
            WarningHolder::AddWarning(56, "Video input '" + source.name + "' has no capture device path configured");
            return false;
        }
        srcElement = "v4l2src device=" + GstQuote(source.device);
    } else if (source.type == "rtspsrc") {
        if (source.uri.empty()) {
            LogWarn(VB_MEDIAOUT, "VideoInputManager: rtspsrc '%s' has no URI\n", source.name.c_str());
            WarningHolder::AddWarning(56, "Video input '" + source.name + "' (RTSP) has no URI configured");
            return false;
        }
        // rtspsrc → decodebin handles codec negotiation (H.264, H.265, MJPEG, etc.)
        srcElement = "rtspsrc location=" + GstQuote(source.uri)
                   + " latency=" + std::to_string(source.latency)
                   + " protocols=tcp";
        useDecodebin = true;
    } else if (source.type == "urisrc") {
        if (source.uri.empty()) {
            LogWarn(VB_MEDIAOUT, "VideoInputManager: urisrc '%s' has no URI\n", source.name.c_str());
            WarningHolder::AddWarning(56, "Video input '" + source.name + "' has no URI configured");
            return false;
        }
        std::string resolvedUri = source.uri;

        // Detect YouTube URLs and resolve to a direct stream URL via yt-dlp.
        // YouTube page URLs can't be played by GStreamer directly, and the
        // resolved URLs expire after ~1 hour, so we resolve at start time.
        // When audioEnabled is true for YouTube sources, skip resolution here —
        // StartSourceWithAudio resolves separate video + audio URLs.
        if (!source.audioEnabled &&
            (resolvedUri.find("youtube.com/") != std::string::npos ||
             resolvedUri.find("youtu.be/") != std::string::npos)) {
            LogInfo(VB_MEDIAOUT, "VideoInputManager: Resolving YouTube URL for '%s' via yt-dlp\n",
                    source.name.c_str());
            // Select the best H.264 video-only stream that fits the
            // configured output height.  This avoids fetching 240p when
            // the user wants 720p/1080p, while still capping bandwidth
            // to what the Pi can actually decode and display.
            std::string ytFmt = "bestvideo[height<=" + std::to_string(source.height)
                + "][vcodec^=avc1]/worst[vcodec^=avc1]";
            YtDlpResult yt = RunYtDlp(ytFmt, resolvedUri, true);
            if (yt.url.empty()) {
                // Do not fall back to the original page URL. uridecodebin will
                // happily fetch it, get text/html back, and fail with "Your
                // GStreamer installation is missing a plug-in ... Missing
                // decoder: text/html" - which reads like a broken GStreamer
                // install and sends you looking in entirely the wrong place.
                // The source cannot start until yt-dlp resolves it, so fail
                // here and say why.
                LogErr(VB_MEDIAOUT, "VideoInputManager: yt-dlp could not resolve the URL for '%s': %s\n",
                       source.name.c_str(),
                       yt.diagnostics.empty() ? "no output" : yt.diagnostics.c_str());
                WarningHolder::AddWarning(56, "Video input '" + source.name
                    + "': yt-dlp could not resolve the YouTube URL - it may need updating");
                return false;
            }
            LogInfo(VB_MEDIAOUT, "VideoInputManager: yt-dlp resolved '%s' -> %zu-char stream URL\n",
                    source.name.c_str(), yt.url.size());
            resolvedUri = yt.url;
            ApplyDetectedFps(yt.fps, source.framerate, source.name.c_str());
        }

        // uridecodebin handles full URI negotiation: HTTP, HLS (.m3u8),
        // MPEG-DASH, direct files, etc.  It selects the right source
        // element (souphttpsrc, hlsdemux, …) and decoder automatically.
        // caps="video/x-raw" restricts output to decoded video only —
        // without it, streams containing audio+video (e.g. HLS) will
        // expose an audio pad with no consumer, blocking preroll.
        srcElement = "uridecodebin uri=" + GstQuote(resolvedUri)
                   + " caps=\"video/x-raw\"";
        // uridecodebin already includes decoding — do NOT add another
        // decodebin after it.  Just need videoconvert + videoscale + caps.
        useDecodebin = false;
        useUridecodebin = true;
    } else if (source.type == "rtpsrc") {
        // UDP RTP receiver with appropriate depayloader
        std::string caps;
        std::string depay;
        if (source.encoding == "H264") {
            caps = "application/x-rtp,media=video,encoding-name=H264,clock-rate=90000";
            depay = "rtph264depay";
        } else if (source.encoding == "H265") {
            caps = "application/x-rtp,media=video,encoding-name=H265,clock-rate=90000";
            depay = "rtph265depay";
        } else if (source.encoding == "JPEG") {
            caps = "application/x-rtp,media=video,encoding-name=JPEG,clock-rate=90000";
            depay = "rtpjpegdepay";
        } else if (source.encoding == "MP2T") {
            caps = "application/x-rtp,media=video,encoding-name=MP2T,clock-rate=90000";
            depay = "rtpmp2tdepay";
        } else {
            // RAW or unknown — use decodebin to auto-detect
            caps = "application/x-rtp";
            depay = "";
        }
        srcElement = "udpsrc port=" + std::to_string(source.port);
        if (!source.multicastGroup.empty()) {
            srcElement += " multicast-group=" + GstQuote(source.multicastGroup)
                        + " auto-multicast=true";
        }
        srcElement += " caps=\"" + caps + "\"";
        if (!depay.empty()) {
            srcElement += " ! " + depay;
        }
        useDecodebin = true;
    } else {
        LogWarn(VB_MEDIAOUT, "VideoInputManager: Unknown source type '%s'\n", source.type.c_str());
        WarningHolder::AddWarning(56, "Video input '" + source.name + "' has unknown source type '" + source.type + "'");
        return false;
    }

    // Build full pipeline:
    //   <source> ! capsfilter ! videoconvert ! queue ! pipewiresink(mode=provide)
    // For rtspsrc: rtspsrc ! decodebin ! videoconvert ! videoscale ! caps ! queue ! pipewiresink

    // When audio is enabled for YouTube urisrc sources, delegate to
    // StartSourceWithAudio which resolves separate video + audio URLs.
    // YouTube muxed HLS formats return 403 for segment downloads, so
    // we use two separate uridecodebin elements.
    if (source.audioEnabled && useUridecodebin) {
        bool isYouTube = source.uri.find("youtube.com/") != std::string::npos ||
                         source.uri.find("youtu.be/") != std::string::npos;
        if (isYouTube) {
            if (StartSourceWithAudio(source)) {
                return true;
            }
            // StartSourceWithAudio() couldn't resolve the video and/or audio
            // URL (e.g. yt-dlp found no separate audio stream) and returned
            // false without starting anything. Don't leave the source
            // unstarted — retry as video-only. Clearing audioEnabled makes
            // this re-entrant call take the plain yt-dlp video-URL
            // resolution path above (it's skipped above while audioEnabled
            // is set, since that path is StartSourceWithAudio's job).
            LogWarn(VB_MEDIAOUT, "VideoInputManager: '%s' failed to start with audio, "
                    "retrying as video-only\n", source.name.c_str());
            source.audioEnabled = false;
            return StartSource(source);
        }
        LogWarn(VB_MEDIAOUT, "VideoInputManager: audioEnabled not supported for non-YouTube urisrc '%s', ignoring audio\n",
                source.name.c_str());
    }

    // pixel-aspect-ratio=1/1 is load-bearing.  Forcing width and height
    // alone lets videoscale hit the requested size by changing the PAR
    // instead of actually rescaling: a 4:3 camera configured as 1280x720
    // produced caps of 1280x720 with pixel-aspect-ratio=3/4, i.e. a frame
    // that still displays 4:3.  Pinning square pixels makes videoscale do
    // the real work, and its add-borders default letterboxes rather than
    // distorting when the source and target aspects differ.
    std::string capsStr = "video/x-raw,width=" + std::to_string(source.width)
                        + ",height=" + std::to_string(source.height)
                        + ",pixel-aspect-ratio=1/1"
                        + ",framerate=" + std::to_string(source.framerate) + "/1";

    std::string pipelineDesc;
    if (useUridecodebin) {
        // uridecodebin already decodes — just convert/scale/queue to intervideosink.
        // videorate converts the stream's native framerate to the configured one.
        // clocksync paces bursty HLS delivery at real-time.
        pipelineDesc = srcElement
            + " ! videoconvert"
            + " ! videoscale"
            + " ! videorate"
            + " ! " + capsStr
            + " ! queue max-size-time=2000000000 max-size-buffers=0 max-size-bytes=0"
            + " ! clocksync"
            + " ! intervideosink sync=false channel=" + GstQuote(source.pipeWireNodeName);
    } else if (useDecodebin) {
        // RTSP: decodebin handles codec negotiation, videoscale + caps enforce resolution.
        // videorate needed when stream framerate differs from configured value.
        // clocksync paces delivery at real-time.
        pipelineDesc = srcElement
            + " ! decodebin"
            + " ! videoconvert"
            + " ! videoscale"
            + " ! videorate"
            + " ! " + capsStr
            + " ! queue max-size-time=2000000000 max-size-buffers=0 max-size-bytes=0"
            + " ! clocksync"
            + " ! intervideosink sync=false channel=" + GstQuote(source.pipeWireNodeName);
    } else if (source.type == "v4l2src") {
        // Cameras only negotiate their own native modes.  Asking v4l2src
        // directly for video/x-raw at an arbitrary width/height/framerate
        // fails to negotiate and the pipeline never leaves READY, which
        // surfaced only as "failed to start" in the log.  decodebin covers
        // MJPEG-only and H.264 webcams (very common above VGA), and passes
        // already-raw formats straight through; videoscale/videorate then
        // convert whatever the camera gave us to the configured size/rate.
        //
        // Leaving the device *entirely* unconstrained, though, meant the
        // configured resolution and framerate never reached it at all:
        // v4l2src picked the device's own preferred mode (a 1920x1080@30
        // source was observed capturing 640x480@60) and videoscale/videorate
        // resampled that to the configured numbers, so the settings looked
        // applied while the capture ignored them.  BuildDeviceCaps asks the
        // device what it actually supports and pins the closest real mode,
        // returning "" -- and so restoring exactly the unconstrained
        // behaviour above -- whenever it can't be sure.
        std::string deviceCaps = BuildDeviceCaps(source);

        pipelineDesc = srcElement
            + (deviceCaps.empty() ? "" : " ! " + deviceCaps)
            + " ! decodebin"
            + " ! videoconvert"
            + " ! videoscale"
            + " ! videorate"
            + " ! " + capsStr
            + " ! queue max-size-buffers=2 leaky=downstream"
            + " ! intervideosink sync=false channel=" + GstQuote(source.pipeWireNodeName);
    } else {
        pipelineDesc = srcElement
            + " ! " + capsStr
            + " ! videoconvert"
            + " ! queue max-size-buffers=2"
            + " ! intervideosink sync=false channel=" + GstQuote(source.pipeWireNodeName);
    }

    LogInfo(VB_MEDIAOUT, "VideoInputManager: Starting source '%s' (%s): %s\n",
            source.name.c_str(), source.type.c_str(), pipelineDesc.c_str());

    GError* error = nullptr;
    source.pipeline = gst_parse_launch(pipelineDesc.c_str(), &error);
    if (!source.pipeline) {
        LogErr(VB_MEDIAOUT, "VideoInputManager: Failed to create pipeline for '%s': %s\n",
               source.name.c_str(), error ? error->message : "unknown error");
        if (error) g_error_free(error);
        return false;
    }
    if (error) {
        LogWarn(VB_MEDIAOUT, "VideoInputManager: Pipeline warning for '%s': %s\n",
                source.name.c_str(), error->message);
        g_error_free(error);
    }

    // Video sink is intervideosink — no PipeWire configuration needed.
    // The channel name (set in the pipeline description) matches the
    // consumer's intervideosrc channel for in-process video routing.
    // PipeWire's video driver scheduling (rate=0) cannot drive video
    // graph cycles, so we bypass PipeWire for video entirely.

    // Start in a background thread — set_state(PLAYING) can block
    source.shutdownRequested = false;
    source.running = true;

    std::string sourceName = source.name;
    std::string nodeNameCopy = source.pipeWireNodeName;
    GstElement* pipeline = source.pipeline;
    std::atomic<bool>* shutdownFlag = &source.shutdownRequested;
    std::atomic<bool>* runningFlag = &source.running;

    // Copied by value into the thread rather than reached through `source`:
    // the run thread outlives this call and m_sources can be reallocated by
    // a Reload while it is still going.
    std::string deviceCopy = source.device;
    V4L2Device::ControlSettings controls;
    if (source.type == "v4l2src") {
        controls.powerLineFrequency = source.powerLineFrequency;
        controls.exposureMode = source.exposureMode;
        controls.exposureTime100us = source.exposureTime100us;
        controls.dynamicFramerate = source.dynamicFramerate;
    }

    source.runThread = std::thread([pipeline, sourceName, nodeNameCopy, shutdownFlag, runningFlag,
                                    deviceCopy, controls]() {
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' thread starting pipeline (node=%s)\n",
                sourceName.c_str(), nodeNameCopy.c_str());

        // Device controls (anti-flicker / exposure) are applied twice on
        // purpose: once here so v4l2src inherits them when it opens the
        // node, and again once the pipeline is actually PLAYING so a driver
        // that resets on open doesn't undo them.  The writes are idempotent
        // and cost one ioctl each.
        auto applyControls = [deviceCopy, controls, sourceName]() {
            if (!controls.AnyRequested())
                return;
            std::string summary;
            if (V4L2Device::ApplyControls(deviceCopy, controls, summary) && !summary.empty()) {
                LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' device controls: %s\n",
                        sourceName.c_str(), summary.c_str());
            }
        };
        applyControls();

        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            if (!shutdownFlag->load()) {
                LogWarn(VB_MEDIAOUT, "VideoInputManager: Source '%s' failed to start\n", sourceName.c_str());
                WarningHolder::AddWarning(56, "Video input '" + sourceName + "' failed to start");
            }
            *runningFlag = false;
            return;
        }

        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' pipeline state=%s (waiting for data)\n",
                sourceName.c_str(),
                ret == GST_STATE_CHANGE_SUCCESS ? "PLAYING" :
                ret == GST_STATE_CHANGE_ASYNC   ? "ASYNC"   : "OTHER");

        // If pipeline went straight to PLAYING (no async), notify consumers now.
        if (ret == GST_STATE_CHANGE_SUCCESS) {
            applyControls();
            VideoOutputManager::Instance().NotifyProducerReady(nodeNameCopy);
        }

        // Monitor the pipeline bus for errors / EOS
        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
        while (!shutdownFlag->load()) {
            GstMessage* msg = gst_bus_timed_pop(bus, 500 * GST_MSECOND);
            if (!msg) continue;

            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError* err = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_error(msg, &err, &debug);
                    if (!shutdownFlag->load()) {
                        LogWarn(VB_MEDIAOUT, "VideoInputManager: Source '%s' error: %s (%s)\n",
                                sourceName.c_str(), err ? err->message : "?", debug ? debug : "");
                    }
                    if (err) g_error_free(err);
                    g_free(debug);
                    gst_message_unref(msg);
                    gst_object_unref(bus);
                    // Drop the dead pipeline out of PLAYING so it releases
                    // its device fd / socket / decoder buffers now instead
                    // of holding them until the next Reload/Shutdown (this
                    // pipeline is per-daemon-lifetime, so that could be
                    // days). StopSource() still owns unref'ing source.pipeline.
                    gst_element_set_state(pipeline, GST_STATE_NULL);
                    *runningFlag = false;
                    return;
                }
                case GST_MESSAGE_BUFFERING: {
                    // HLS / adaptive streams send buffering messages from
                    // internal queue2 elements.  For live streams, pausing
                    // on low buffer levels starves downstream (pipewiresink)
                    // and prevents PipeWire consumers from connecting.
                    // Just log without pausing — the internal queues handle
                    // buffering on their own.
                    gint percent = 0;
                    gst_message_parse_buffering(msg, &percent);
                    if (percent == 0 || percent == 100) {
                        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' buffering %d%%\n",
                                sourceName.c_str(), percent);
                    }
                    break;
                }
                case GST_MESSAGE_STATE_CHANGED: {
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                        GstState oldState, newState, pending;
                        gst_message_parse_state_changed(msg, &oldState, &newState, &pending);
                        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' state %s -> %s (pending=%s)\n",
                                sourceName.c_str(),
                                gst_element_state_get_name(oldState),
                                gst_element_state_get_name(newState),
                                gst_element_state_get_name(pending));
                        if (newState == GST_STATE_PLAYING && pending == GST_STATE_VOID_PENDING) {
                            applyControls();
                            VideoOutputManager::Instance().NotifyProducerReady(nodeNameCopy);
                        }
                    }
                    break;
                }
                case GST_MESSAGE_EOS:
                    if (!shutdownFlag->load()) {
                        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' received EOS\n", sourceName.c_str());
                    }
                    gst_message_unref(msg);
                    gst_object_unref(bus);
                    // Same reasoning as the ERROR case above — EOS also
                    // means the pipeline is done producing and must not be
                    // left in PLAYING.
                    gst_element_set_state(pipeline, GST_STATE_NULL);
                    *runningFlag = false;
                    return;
                default:
                    break;
            }
            gst_message_unref(msg);
        }
        gst_object_unref(bus);
        *runningFlag = false;
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' thread exiting\n", sourceName.c_str());
    });

    LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' launched (node=%s)\n",
            source.name.c_str(), source.pipeWireNodeName.c_str());
    return true;
#else
    LogWarn(VB_MEDIAOUT, "VideoInputManager: GStreamer not available\n");
    WarningHolder::AddWarning(56, "Video input requires GStreamer, which is not available on this device");
    return false;
#endif
}

// ─── Start source with separate video + audio streams (YouTube HLS) ───────
bool VideoInputManager::StartSourceWithAudio(SourceInfo& source) {
#ifdef HAS_GSTREAMER_VIDEO_INPUT
    if (source.running) {
        LogWarn(VB_MEDIAOUT, "VideoInputManager: Source '%s' already running\n", source.name.c_str());
        return true;
    }

    setenv("PIPEWIRE_RUNTIME_DIR", "/run/pipewire-fpp", 0);

    LogInfo(VB_MEDIAOUT, "VideoInputManager: Starting source '%s' with audio extraction\n",
            source.name.c_str());

    // YouTube muxed HLS formats (91/92) return HTTP 403 for segment
    // downloads, so we resolve separate video-only and audio-only URLs
    // and use two independent uridecodebin elements.

    // Resolve video-only URL and detect native framerate
    std::string videoUrl;
    {
        // Select best H.264 stream fitting the configured height
        std::string ytFmt = "bestvideo[height<=" + std::to_string(source.height)
            + "][vcodec^=avc1]/worst[vcodec^=avc1]";
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Resolving YouTube video URL for '%s'\n",
                source.name.c_str());
        YtDlpResult yt = RunYtDlp(ytFmt, source.uri, true);
        if (yt.url.empty()) {
            LogWarn(VB_MEDIAOUT, "VideoInputManager: Failed to resolve video URL for '%s': %s\n",
                    source.name.c_str(),
                    yt.diagnostics.empty() ? "no output" : yt.diagnostics.c_str());
            WarningHolder::AddWarning(56, "Video input '" + source.name + "': could not resolve the video URL");
            return false;
        }
        videoUrl = yt.url;
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Video URL resolved (%zu chars)\n", videoUrl.size());
        ApplyDetectedFps(yt.fps, source.framerate, source.name.c_str());
    }

    // Resolve audio-only URL
    std::string audioUrl;
    {
        // 234 = high quality HLS audio, 233 = low quality HLS audio
        // 140 = AAC 128k from DASH (non-live fallback), 139 = HE-AAC 48k
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Resolving YouTube audio URL for '%s'\n",
                source.name.c_str());
        YtDlpResult yt = RunYtDlp("234/233/140/139/bestaudio", source.uri, false);
        if (yt.url.empty()) {
            LogWarn(VB_MEDIAOUT, "VideoInputManager: Failed to resolve audio URL for '%s' (%s), "
                    "falling back to video-only\n", source.name.c_str(),
                    yt.diagnostics.empty() ? "no output" : yt.diagnostics.c_str());
            // Returning false here is not a no-op: our caller (StartSource())
            // retries with audioEnabled cleared and builds the regular
            // video-only pipeline instead of leaving the source unstarted.
            return false;
        }
        audioUrl = yt.url;
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Audio URL resolved (%zu chars)\n", audioUrl.size());
    }

    // Build a SINGLE combined pipeline for video and audio.
    // Both paths share one pipeline clock and base_time, preventing
    // A/V drift that occurs when separate pipelines pace independently.
    // (intervideosink + pipewiresink in one pipeline works fine;
    // the original deadlock was two pipewiresinks in provide mode.)
    // pixel-aspect-ratio=1/1 is load-bearing.  Forcing width and height
    // alone lets videoscale hit the requested size by changing the PAR
    // instead of actually rescaling: a 4:3 camera configured as 1280x720
    // produced caps of 1280x720 with pixel-aspect-ratio=3/4, i.e. a frame
    // that still displays 4:3.  Pinning square pixels makes videoscale do
    // the real work, and its add-borders default letterboxes rather than
    // distorting when the source and target aspects differ.
    std::string capsStr = "video/x-raw,width=" + std::to_string(source.width)
                        + ",height=" + std::to_string(source.height)
                        + ",pixel-aspect-ratio=1/1"
                        + ",framerate=" + std::to_string(source.framerate) + "/1";

    // Convert bufferSec to nanoseconds for GStreamer properties.
    // This buffers decoded HLS content so clocksync can pace it smoothly.
    long long bufferNs = (long long)(source.bufferSec * 1000000000.0);
    if (bufferNs < 1000000000LL) bufferNs = 1000000000LL; // minimum 1s

    // Two independent branches in one GstPipeline: they share the
    // pipeline clock, so both clocksync elements pace identically.
    std::string combinedPipelineDesc =
        "uridecodebin uri=" + GstQuote(videoUrl) + " caps=\"video/x-raw\""
        " buffer-duration=" + std::to_string(bufferNs) +
        " ! videoconvert"
        " ! videoscale"
        " ! videorate"
        " ! " + capsStr +
        " ! queue max-size-time=" + std::to_string(bufferNs) +
        " max-size-buffers=0 max-size-bytes=0"
        " ! clocksync"
        " ! intervideosink sync=false channel=" + GstQuote(source.pipeWireNodeName) +
        " uridecodebin uri=" + GstQuote(audioUrl) + " caps=\"audio/x-raw\""
        " ! audioconvert"
        " ! audioresample"
        " ! audio/x-raw,format=F32LE,rate=48000,channels=2,layout=interleaved"
        " ! queue max-size-time=5000000000 max-size-buffers=0 max-size-bytes=0"
        " ! clocksync"
        " ! pipewiresink name=apwsink";

    LogInfo(VB_MEDIAOUT, "VideoInputManager: '%s' combined A/V pipeline: %s\n",
            source.name.c_str(), combinedPipelineDesc.c_str());

    // --- Create combined A/V pipeline ---
    GError* error = nullptr;
    source.pipeline = gst_parse_launch(combinedPipelineDesc.c_str(), &error);
    if (!source.pipeline) {
        LogErr(VB_MEDIAOUT, "VideoInputManager: Failed to create pipeline for '%s': %s\n",
               source.name.c_str(), error ? error->message : "unknown error");
        if (error) g_error_free(error);
        return false;
    }
    if (error) {
        LogWarn(VB_MEDIAOUT, "VideoInputManager: Pipeline warning for '%s': %s\n",
                source.name.c_str(), error->message);
        g_error_free(error);
    }

    // No separate audio pipeline — audio is in the combined pipeline.
    source.audioPipeline = nullptr;

    // Configure audio pipewiresink as an Audio/Source in provide mode.
    GstElement* apwsink = gst_bin_get_by_name(GST_BIN(source.pipeline), "apwsink");
    if (apwsink) {
        std::string audioNodeName = source.audioPipeWireNodeName;
        if (audioNodeName.empty())
            audioNodeName = source.pipeWireNodeName + "_audio";
        std::string audioNodeDesc = "FPP Audio Input: " + source.name;

        GstStructure* aprops = gst_structure_new("props",
            "media.class", G_TYPE_STRING, "Audio/Source",
            "node.name", G_TYPE_STRING, audioNodeName.c_str(),
            "node.description", G_TYPE_STRING, audioNodeDesc.c_str(),
            "node.autoconnect", G_TYPE_BOOLEAN, FALSE,
            "node.always-process", G_TYPE_BOOLEAN, TRUE,
            "node.latency", G_TYPE_STRING, "2048/48000",
            "object.register", G_TYPE_STRING, "true",
            NULL);
        g_object_set(apwsink, "stream-properties", aprops, NULL);
        gst_structure_free(aprops);

        g_object_set(apwsink, "mode", PIPEWIRE_SINK_MODE_PROVIDE, NULL);
        // sync=FALSE: provide mode crashes with sync=TRUE.
        // clocksync element upstream paces HLS audio at real-time instead.
        g_object_set(apwsink, "sync", FALSE, NULL);

        // Add a throughput probe on pipewiresink's sink pad to measure
        // how fast audio actually flows (vs expected real-time pace).
        GstPad* sinkPad = gst_element_get_static_pad(apwsink, "sink");
        if (sinkPad) {
            struct ProbeData {
                int64_t totalBytes;
                int64_t startUs;
                int64_t lastLogUs;
                int64_t lastPtsNs;
                int sampleRate;
                int bytesPerFrame;
            };
            auto* pd = new ProbeData{0, 0, 0, 0, 48000, 8}; // default 2ch*F32LE=8 bpf
            gst_pad_add_probe(sinkPad, GST_PAD_PROBE_TYPE_BUFFER,
                [](GstPad* pad, GstPadProbeInfo* info, gpointer udata) -> GstPadProbeReturn {
                    auto* d = static_cast<ProbeData*>(udata);
                    GstBuffer* buf = GST_PAD_PROBE_INFO_BUFFER(info);
                    if (d->startUs == 0) {
                        d->startUs = g_get_monotonic_time();
                        // Log actual negotiated caps
                        GstCaps* caps = gst_pad_get_current_caps(pad);
                        if (caps) {
                            gchar* cs = gst_caps_to_string(caps);
                            LogInfo(VB_MEDIAOUT, "Audio probe caps: %s\n", cs);
                            // Extract bytes-per-frame from caps to fix calculation
                            GstStructure* s = gst_caps_get_structure(caps, 0);
                            const gchar* fmt = gst_structure_get_string(s, "format");
                            int ch = 0; gst_structure_get_int(s, "channels", &ch);
                            int bps = 2; // default S16LE
                            if (fmt && (strcmp(fmt, "F32LE") == 0 || strcmp(fmt, "F32BE") == 0 ||
                                        strcmp(fmt, "S32LE") == 0 || strcmp(fmt, "S32BE") == 0))
                                bps = 4;
                            else if (fmt && (strcmp(fmt, "F64LE") == 0 || strcmp(fmt, "F64BE") == 0))
                                bps = 8;
                            if (ch > 0) d->bytesPerFrame = ch * bps;
                            g_free(cs);
                            gst_caps_unref(caps);
                        }
                    }
                    d->totalBytes += gst_buffer_get_size(buf);
                    // Also track by PTS
                    GstClockTime pts = GST_BUFFER_PTS(buf);
                    if (GST_CLOCK_TIME_IS_VALID(pts)) d->lastPtsNs = pts;
                    int64_t now = g_get_monotonic_time();
                    if (now - d->lastLogUs >= 5000000) { // every 5s
                        d->lastLogUs = now;
                        double audioSecs = (double)d->totalBytes / (d->sampleRate * d->bytesPerFrame);
                        double wallSecs = (double)(now - d->startUs) / 1000000.0;
                        double ratio = wallSecs > 0 ? audioSecs / wallSecs : 0;
                        double ptsSecs = (double)d->lastPtsNs / 1000000000.0;
                        LogInfo(VB_MEDIAOUT, "Audio throughput: %.1fs audio in %.1fs wall (ratio=%.3f) bpf=%d pts=%.1fs\n",
                                audioSecs, wallSecs, ratio, d->bytesPerFrame, ptsSecs);
                    }
                    return GST_PAD_PROBE_OK;
                }, pd,
                [](gpointer udata) { delete static_cast<ProbeData*>(udata); });
            gst_object_unref(sinkPad);
        }

        gst_object_unref(apwsink);
    }

    // Force the combined pipeline to use the system clock instead of
    // letting pipewiresink provide its own.  Both clocksync elements
    // then pace against the same monotonic clock.
    GstClock* sysClock = gst_system_clock_obtain();
    gst_pipeline_use_clock(GST_PIPELINE(source.pipeline), sysClock);
    gst_object_unref(sysClock);

    LogInfo(VB_MEDIAOUT, "VideoInputManager: '%s' combined pipeline built (video=%s, audio=%s)\n",
            source.name.c_str(), source.pipeWireNodeName.c_str(),
            source.audioPipeWireNodeName.c_str());

    // Start combined pipeline in its own thread
    source.shutdownRequested = false;
    source.running = true;

    std::string sourceName = source.name;
    std::string nodeNameCopy = source.pipeWireNodeName;
    GstElement* combinedPipeline = source.pipeline;
    std::atomic<bool>* shutdownFlag = &source.shutdownRequested;
    std::atomic<bool>* runningFlag = &source.running;

    source.runThread = std::thread([combinedPipeline, sourceName, nodeNameCopy, shutdownFlag, runningFlag]() {
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' A/V thread starting (node=%s)\n",
                sourceName.c_str(), nodeNameCopy.c_str());

        GstStateChangeReturn ret = gst_element_set_state(combinedPipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            if (!shutdownFlag->load()) {
                LogWarn(VB_MEDIAOUT, "VideoInputManager: Source '%s' failed to start\n",
                        sourceName.c_str());
                WarningHolder::AddWarning(56, "Video input '" + sourceName + "' failed to start");
            }
            *runningFlag = false;
            return;
        }

        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' pipeline state=%s\n",
                sourceName.c_str(),
                ret == GST_STATE_CHANGE_SUCCESS ? "PLAYING" :
                ret == GST_STATE_CHANGE_ASYNC   ? "ASYNC"   : "OTHER");

        if (ret == GST_STATE_CHANGE_SUCCESS) {
            VideoOutputManager::Instance().NotifyProducerReady(nodeNameCopy);
        }

        GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(combinedPipeline));
        while (!shutdownFlag->load()) {
            GstMessage* msg = gst_bus_timed_pop(bus, 500 * GST_MSECOND);
            if (!msg) continue;

            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError* err = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_error(msg, &err, &debug);
                    if (!shutdownFlag->load()) {
                        const gchar* srcName = GST_MESSAGE_SRC(msg) ?
                            GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)) : "?";
                        LogWarn(VB_MEDIAOUT, "VideoInputManager: Source '%s' error [%s]: %s (%s)\n",
                                sourceName.c_str(), srcName,
                                err ? err->message : "?", debug ? debug : "");
                    }
                    if (err) g_error_free(err);
                    g_free(debug);
                    gst_message_unref(msg);
                    gst_object_unref(bus);
                    // Drop the dead combined A/V pipeline out of PLAYING so
                    // it releases its device fd / socket / decoder buffers
                    // now instead of holding them until the next
                    // Reload/Shutdown. StopSource() still owns unref'ing
                    // source.pipeline.
                    gst_element_set_state(combinedPipeline, GST_STATE_NULL);
                    *runningFlag = false;
                    return;
                }
                case GST_MESSAGE_BUFFERING: {
                    gint percent = 0;
                    gst_message_parse_buffering(msg, &percent);
                    if (percent == 0 || percent == 100) {
                        const gchar* srcName = GST_MESSAGE_SRC(msg) ?
                            GST_OBJECT_NAME(GST_MESSAGE_SRC(msg)) : "?";
                        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' [%s] buffering %d%%\n",
                                sourceName.c_str(), srcName, percent);
                    }
                    break;
                }
                case GST_MESSAGE_STATE_CHANGED: {
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(combinedPipeline)) {
                        GstState oldState, newState, pending;
                        gst_message_parse_state_changed(msg, &oldState, &newState, &pending);
                        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' state %s -> %s (pending=%s)\n",
                                sourceName.c_str(),
                                gst_element_state_get_name(oldState),
                                gst_element_state_get_name(newState),
                                gst_element_state_get_name(pending));
                        if (newState == GST_STATE_PLAYING && pending == GST_STATE_VOID_PENDING) {
                            VideoOutputManager::Instance().NotifyProducerReady(nodeNameCopy);
                        }
                    }
                    break;
                }
                case GST_MESSAGE_EOS:
                    if (!shutdownFlag->load()) {
                        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' received EOS\n",
                                sourceName.c_str());
                    }
                    gst_message_unref(msg);
                    gst_object_unref(bus);
                    // Same reasoning as the ERROR case above.
                    gst_element_set_state(combinedPipeline, GST_STATE_NULL);
                    *runningFlag = false;
                    return;
                default:
                    break;
            }
            gst_message_unref(msg);
        }
        gst_object_unref(bus);
        *runningFlag = false;
        LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' A/V thread exiting\n", sourceName.c_str());
    });

    // No separate audio thread — audio runs in the combined pipeline.

    LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' launched with audio (node=%s)\n",
            source.name.c_str(), source.pipeWireNodeName.c_str());
    return true;
#else
    LogWarn(VB_MEDIAOUT, "VideoInputManager: GStreamer not available\n");
    WarningHolder::AddWarning(56, "Video input requires GStreamer, which is not available on this device");
    return false;
#endif
}

void VideoInputManager::StopSource(SourceInfo& source) {
#ifdef HAS_GSTREAMER_VIDEO_INPUT
    source.shutdownRequested = true;
    source.audioShutdownRequested = true;

    // Join the bus monitor threads first — they check shutdown flags
    // every 500ms (gst_bus_timed_pop timeout) and exit promptly.
    if (source.runThread.joinable()) {
        source.runThread.join();
    }
    if (source.audioRunThread.joinable()) {
        source.audioRunThread.join();
    }

    if (source.pipeline) {
        GstElement* p = source.pipeline;
        source.pipeline = nullptr;
        std::string name = source.name;
        // Tracked (not detached) so Shutdown()/Reload() can join it before
        // the process exits or the same device/URI is reopened — see
        // JoinTeardownThreads().
        std::lock_guard<std::mutex> tlock(m_teardownMutex);
        m_teardownThreads.emplace_back([p, name]() {
            gst_element_set_state(p, GST_STATE_NULL);
            gst_object_unref(p);
            LogInfo(VB_MEDIAOUT, "VideoInputManager: Video pipeline for '%s' released\n", name.c_str());
        });
    }

    if (source.audioPipeline) {
        GstElement* ap = source.audioPipeline;
        source.audioPipeline = nullptr;
        std::string name = source.name;
        std::lock_guard<std::mutex> tlock(m_teardownMutex);
        m_teardownThreads.emplace_back([ap, name]() {
            gst_element_set_state(ap, GST_STATE_NULL);
            gst_object_unref(ap);
            LogInfo(VB_MEDIAOUT, "VideoInputManager: Audio pipeline for '%s' released\n", name.c_str());
        });
    }

    source.running = false;
    LogInfo(VB_MEDIAOUT, "VideoInputManager: Source '%s' stopped\n", source.name.c_str());
#endif
}

void VideoInputManager::StopAllSources() {
    for (auto& source : m_sources) {
        StopSource(source);
    }
}

void VideoInputManager::JoinTeardownThreads() {
    // Swap the list out under the lock, then join outside it — joining can
    // block for a while if a pipeline is wedged in gst_element_set_state(),
    // and StopSource() (called from other sources' teardown, or a
    // concurrent caller) shouldn't have to wait on m_teardownMutex for that.
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> tlock(m_teardownMutex);
        threads.swap(m_teardownThreads);
    }
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}


bool VideoInputManager::GrabSnapshotJPEG(int sourceId, int maxWidth, int timeoutMs,
                                         std::vector<uint8_t>& jpegOut) {
    jpegOut.clear();
#ifdef HAS_GSTREAMER_VIDEO_INPUT
    if (maxWidth < 32) maxWidth = 32;
    if (maxWidth > 1280) maxWidth = 1280;
    if (timeoutMs < 100) timeoutMs = 100;
    if (timeoutMs > 10000) timeoutMs = 10000;

    std::string channel;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& src : m_sources) {
            if (src.id == sourceId) {
                if (!src.running.load()) {
                    return false;
                }
                channel = src.pipeWireNodeName;
                break;
            }
        }
    }
    if (channel.empty() || !GstValueUsable(channel, "node name", "snapshot")) {
        return false;
    }

    // pixel-aspect-ratio=1/1 is load-bearing.  With a width-only capsfilter
    // videoscale satisfies the request by changing the PAR rather than the
    // height -- a 240x135 source came out as a 320x135 JPEG, and since JPEG
    // carries no PAR the preview showed a squashed picture.  Pinning PAR to
    // square forces videoscale to pick the height instead, so the preview
    // has the same shape as the real output.
    std::string desc = "intervideosrc name=isrc timeout=" + std::to_string((long long)timeoutMs * 1000000LL)
                     + " channel=" + GstQuote(channel)
                     + " ! videoconvert"
                     + " ! videoscale"
                     + " ! video/x-raw,width=" + std::to_string(maxWidth)
                     + ",pixel-aspect-ratio=1/1"
                     + " ! jpegenc quality=70"
                     + " ! appsink name=snap max-buffers=1 drop=true sync=false";

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(desc.c_str(), &error);
    if (!pipeline) {
        LogWarn(VB_MEDIAOUT, "VideoInputManager: snapshot pipeline failed for source %d: %s\n",
                sourceId, error ? error->message : "unknown error");
        if (error) g_error_free(error);
        return false;
    }
    if (error) {
        g_error_free(error);
        error = nullptr;
    }

    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "snap");
    if (!sink) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return false;
    }

    bool ok = false;
    if (gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE) {
        GstSample* sample = gst_app_sink_try_pull_sample(
            GST_APP_SINK(sink), (GstClockTime)timeoutMs * GST_MSECOND);
        if (sample) {
            GstBuffer* buf = gst_sample_get_buffer(sample);
            GstMapInfo map;
            if (buf && gst_buffer_map(buf, &map, GST_MAP_READ)) {
                jpegOut.assign(map.data, map.data + map.size);
                gst_buffer_unmap(buf, &map);
                ok = !jpegOut.empty();
            }
            gst_sample_unref(sample);

            // Log what intervideosrc actually negotiated off the shared
            // surface, now that a frame has arrived and caps are fixed.
            // This is the only in-process view of the channel's real caps:
            // an out-of-process gst-launch probe can never see it, because
            // the inter elements share a surface registry private to one
            // process and would report the element default (320x240) and
            // emit black frames instead.
            GstElement* isrc = gst_bin_get_by_name(GST_BIN(pipeline), "isrc");
            if (isrc) {
                GstPad* pad = gst_element_get_static_pad(isrc, "src");
                if (pad) {
                    GstCaps* c = gst_pad_get_current_caps(pad);
                    if (c) {
                        gchar* cs = gst_caps_to_string(c);
                        LogDebug(VB_MEDIAOUT, "VideoInputManager: channel '%s' negotiated caps: %s\n",
                                 channel.c_str(), cs ? cs : "?");
                        g_free(cs);
                        gst_caps_unref(c);
                    }
                    gst_object_unref(pad);
                }
                gst_object_unref(isrc);
            }
        } else {
            LogDebug(VB_MEDIAOUT, "VideoInputManager: snapshot timed out for source %d (no frames on channel %s)\n",
                     sourceId, channel.c_str());
        }
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(sink);
    gst_object_unref(pipeline);
    return ok;
#else
    return false;
#endif
}
