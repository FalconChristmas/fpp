/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

// Include drogon framework header before FPP headers to avoid
// macro conflicts between trantor's LOG_* macros and FPP's LogLevel enum
#include <drogon/HttpAppFramework.h>
#undef LOG_WARN
#undef LOG_INFO
#undef LOG_DEBUG

#include "fpp-pch.h"

#include "fpp-json.h"
#include "fpphttp.h" // drogon/HTTP helpers used here; no longer pulled transitively (see fpphttp_types.h)
#include "fpphttp_clientip.h" // getEffectiveClientIP() - FPP-internal, not in the plugin API
#include "StatusWebSocket.h"
#include <fcntl.h>

// Defined in fppd.cpp; setting it to 0 breaks the main loop and triggers
// FPP's normal shutdown sequence, same as SIGQUIT or a graceful stop.
extern volatile int runMainFPPDLoop;

#ifdef PLATFORM_OSX
#include <sys/sysctl.h>
#else
#include <sys/ioctl.h>
#include <sys/sysinfo.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstdlib>
#include <ctime>
#include <cxxabi.h>
#include <iomanip>
#include <iostream>
#include <list>
#include <stdint.h>
#include <string>
#include <vector>

#include "MultiSync.h"
#include "OutputMonitor.h"
#include "RecurringTasks.h"
#include "Player.h"
#include "Plugins.h"
#include "Timers.h"

#include <chrono>
#include <future>
#include "Scheduler.h"
#include "Sequence.h"
#include "Variables.h"
#include "Warnings.h"
#include "common.h"
#include "e131bridge.h"
#include "effects.h"
#include "fppversion.h"
#include "gpio.h"
#include "log.h"
#include "mqtt.h"
#include "settings.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "channeloutput/channeloutputthread.h"
#include "channeltester/ChannelTester.h"
#include "commands/Commands.h"
#include "commands/Condition.h"
#include "mediaoutput/AES67Manager.h"
#include "mediaoutput/AudioSourceRegistry.h"
#include "mediaoutput/MediaOutputBase.h"
#include "mediaoutput/MediaOutputStatus.h"
#include "mediaoutput/OpusRTPManager.h"
#include "mediaoutput/StreamSlotManager.h"
#include "mediaoutput/mediaoutput.h"
#include "overlays/PixelOverlay.h"
#include "playlist/Playlist.h"
#include "sensors/Sensors.h"

#include "httpAPI.h"

static std::time_t startupTime = std::time(nullptr);
static bool piPowerBad = false;

/*
 Build a Status JSON String
*/
void GetCurrentFPPDStatus(Json::Value& result) {
    std::string UUID = getSetting("SystemUUID");
    static std::string host_name = getSetting("HostName");
    static std::string host_description = getSetting("HostDescription");
    static std::string fpp_version = getFPPVersion();
    static std::string fppd_branch = getFPPBranch();
    static std::string platform = getPlatform();

    int mode = getFPPmode();
    result["fppd"] = "running";
    result["version"] = fpp_version;
    result["branch"] = fppd_branch;
    result["platform"] = platform;
    result["uuid"] = UUID;
    result["host_name"] = host_name;
    result["host_description"] = host_description;
    result["mode"] = mode;
    result["mode_name"] = toStdStringAndFree(modeToString(getFPPmode()));
    result["status"] = Player::INSTANCE.GetStatus();
    result["bridging"] = HasBridgeData();
    result["multisync"] = multiSync->isMultiSyncEnabled();
    result["powerBad"] = piPowerBad;

    result["channelInputsEnabled"] = InputsEnabled();
    result["channelOutputsEnabled"] = HasUniverseOutputs();

    if (ChannelTester::INSTANCE.Testing()) {
        result["status_name"] = "testing";
    } else {
        switch (result["status"].asInt()) {
        case FPP_STATUS_IDLE:
            result["status_name"] = "idle";
            break;
        case FPP_STATUS_PLAYLIST_PLAYING:
            result["status_name"] = "playing";
            break;
        case FPP_STATUS_STOPPING_GRACEFULLY:
            result["status_name"] = "stopping gracefully";
            break;
        case FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP:
            result["status_name"] = "stopping gracefully after loop";
            break;
        case FPP_STATUS_STOPPING_NOW:
            result["status_name"] = "stopping now";
            break;
        case FPP_STATUS_PLAYLIST_PAUSED:
            result["status_name"] = "paused";
            break;
        default:
            result["status_name"] = "unknown";
        }
    }

    result["volume"] = getVolume();

    auto t = std::time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    std::stringstream sstr;
    sstr << std::put_time(&tm, "%a %b %d %H:%M:%S %Z %Y");
    std::string str = sstr.str();
    result["time"] = str;

    std::string fmt = getSetting("TimeFormat");
    replaceAll(fmt, "%M", "%M:%S");
    result["timeStrFull"] = GetTimeStr(fmt.c_str());
    result["timeStr"] = GetTimeStr(getSetting("TimeFormat"));
    result["dateStr"] = GetDateStr(getSetting("DateFormat"));

    // FPPD process uptime (time since fppd started)
    int totalseconds = (int)(std::time(nullptr) - startupTime);
    int days = totalseconds / 86400;
    int hours = (totalseconds % 86400) / 3600;
    int minutes = (totalseconds % 3600) / 60;
    int seconds = totalseconds % 60;

    result["uptime"] = secondsToTime(totalseconds);
    result["uptimeTotalSeconds"] = totalseconds;
    result["uptimeSeconds"] = seconds;
    result["uptimeMinutes"] = minutes;
    result["uptimeHours"] = hours;
    result["uptimeDays"] = days;

    std::stringstream totalTime;
    totalTime << days << " days, " << hours << " hours, " << minutes << " minutes, " << seconds << " seconds";
    result["uptimeStr"] = totalTime.str();

    // System uptime
    int systemUptimeSecs = 0;
#ifdef PLATFORM_OSX
    struct timeval boot;
    struct timeval now;
    int mib[] = { CTL_KERN, KERN_BOOTTIME };
    size_t size = sizeof(boot);
    if (sysctl(mib, 2, &boot, &size, 0, 0) == 0 && gettimeofday(&now, 0) == 0) {
        systemUptimeSecs = (int)(now.tv_sec - boot.tv_sec);
    }
#else
    struct sysinfo sysInf;
    if (sysinfo(&sysInf) == 0) {
        systemUptimeSecs = (int)sysInf.uptime;
    }
#endif
    result["systemUptimeTotalSeconds"] = systemUptimeSecs;

    Sensors::INSTANCE.reportSensors(result);
    for (auto& warn : WarningHolder::GetWarnings()) {
        result["warnings"].append(warn.message());
        result["warningInfo"].append(warn.toJsonValue());
    }
    if (mode == 1) {
        // bridge mode only returns the base information
        return;
    }

    // MQTT
    result["MQTT"]["configured"] = false;
    result["MQTT"]["connected"] = false;
    if (mqtt) {
        result["MQTT"]["configured"] = true;
        result["MQTT"]["connected"] = mqtt->IsConnected();
    }

    if (getFPPmode() == REMOTE_MODE) {
        int secsElapsed = 0;
        int secsRemaining = 0;
        std::string seqFilename;
        std::string mediaFilename;
        if (sequence->IsSequenceRunning()) {
            seqFilename = sequence->m_seqFilename;
            secsElapsed = sequence->m_seqMSElapsed / 1000;
            secsRemaining = sequence->m_seqMSRemaining / 1000;
        }
        {
            // CloseMediaOutput() deletes mediaOutput under mediaOutputLock at
            // every media transition, and this status build runs on the HTTP
            // threads, the drogon status-push timer and the warning notify
            // thread. Copy the fields out under the lock rather than reading
            // through the raw pointer while building the JSON. Nothing in here
            // re-enters media code, so the hold is a string copy long.
            std::unique_lock<std::mutex> lock(mediaOutputLock);
            if (mediaOutput) {
                mediaFilename = mediaOutput->m_mediaFilename;
                secsElapsed = mediaOutputStatus.secondsElapsed;
                secsRemaining = mediaOutputStatus.secondsRemaining;
            }
        }

        if (seqFilename != "" || mediaFilename != "") {
            result["status"] = 1;
            result["status_name"] = "playing";
        }

        result["playlist"] = seqFilename;
        result["sequence_filename"] = seqFilename;
        result["media_filename"] = mediaFilename.substr(mediaFilename.find_last_of("/\\") + 1);
        result["current_sequence"] = seqFilename;
        result["current_song"] = result["media_filename"];
        result["seconds_played"] = std::to_string(secsElapsed);
        result["seconds_elapsed"] = std::to_string(secsElapsed);
        result["seconds_remaining"] = std::to_string(secsRemaining);
        result["time_elapsed"] = secondsToTime(secsElapsed);
        result["time_remaining"] = secondsToTime(secsRemaining);
    } else {
        std::string NextPlaylist = scheduler->GetNextPlaylistName();
        std::string NextPlaylistStart = scheduler->GetNextPlaylistStartStr();
        result["next_playlist"]["playlist"] = NextPlaylist;
        result["next_playlist"]["start_time"] = NextPlaylistStart;

        Player::INSTANCE.GetCurrentStatus(result);
        result["scheduler"] = scheduler->GetInfo();

        // Add multi-stream slot status
        result["streamSlots"] = StreamSlotManager::Instance().GetAllSlotsStatus();
    }
}

/*
 *
 */
APIServer::APIServer() {
}

/*
 *
 */
APIServer::~APIServer() {
    drogon::app().quit();
    // Join the server thread so Drogon is fully stopped before we return.
    // PluginManager::Cleanup() (called from main() after MainLoop() exits)
    // will dlclose() plugin libraries; if Drogon IO threads are still running
    // at that point they may execute or release plugin handler lambdas whose
    // code is in those libraries -> SEGV_MAPERR. Joining here ensures all
    // Drogon threads have exited before any dlclose() can happen.
    if (m_serverThread && m_serverThread->joinable()) {
        m_serverThread->join();
    }
    delete m_serverThread;
    m_serverThread = nullptr;
    m_pr.reset();
}

// drogon's registerHandler/registerHandlerViaRegex take the callable by
// rvalue reference and move out of it, so a handler that is registered on
// more than one route must be copied for each registration. Re-using the
// same variable would register a moved-from closure: harmless for the
// captureless lambdas below, but for capturing ones (e.g. handleFppd's
// shared_ptr) the later route ends up dereferencing a nulled capture and
// crashes. Every registration below goes through this explicit copy so
// adding a capture to any handler stays safe.
template<typename T>
static T copyHandler(const T& handler) {
    return handler;
}

/*
 *
 */
void APIServer::Init(void) {
    m_pr = std::make_shared<PlayerResource>();

    auto& app = drogon::app();

    // Register all handlers with path patterns that match the old libhttpserver routes.
    // The "true" flag in the old register_resource() meant "family" (prefix match),
    // so we register with {1} catch-all patterns.

    // PlayerResource (/fppd/*)
    auto handleFppd = [pr = m_pr](const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback) {
        HttpResponsePtr resp;
        if (req->method() == drogon::Get || req->isHead())
            resp = pr->render_GET(req);
        else if (req->method() == drogon::Post)
            resp = pr->render_POST(req);
        else if (req->method() == drogon::Put)
            resp = pr->render_PUT(req);
        else if (req->method() == drogon::Delete)
            resp = pr->render_DELETE(req);
        else
            resp = makeStringResponse("Method Not Allowed", 405);
        callback(resp);
    };
    // Register /fppd exact match first; the catch-all regex is registered after
    // the more specific /fppd/* routes below so they take priority. Bare /fppd
    // has no content of its own -- it 404s from PlayerResource just like a bad
    // sub-path would (confirmed directly against fppd:32322). Intentionally
    // left undocumented, same reasoning as bare /command below.
    app.registerHandler("/fppd", copyHandler(handleFppd), {drogon::Get, drogon::Post, drogon::Put, drogon::Delete, drogon::Head});

    // OutputMonitor (/fppd/ports/*)
    auto handlePorts = [](const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback) {
        callback(OutputMonitor::INSTANCE.render_GET(req));
    };
    app.registerHandlerViaRegex("/fppd/ports", copyHandler(handlePorts), {drogon::Get, drogon::Head});
    app.registerHandlerViaRegex("/fppd/ports/.*", copyHandler(handlePorts), {drogon::Get, drogon::Head});

    // ChannelTester (/fppd/testing/*)
    auto handleTesting = [](const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& callback) {
        HttpResponsePtr resp;
        if (req->method() == drogon::Get || req->isHead())
            resp = ChannelTester::INSTANCE.render_GET(req);
        else if (req->method() == drogon::Post)
            resp = ChannelTester::INSTANCE.render_POST(req);
        else
            resp = makeStringResponse("Method Not Allowed", 405);
        callback(resp);
    };
    app.registerHandlerViaRegex("/fppd/testing(/.*)?", copyHandler(handleTesting), {drogon::Get, drogon::Post, drogon::Head});

#ifdef HAS_AES67_GSTREAMER
    /**
     * Get AES67 status: whether it's active, PTP sync state, discovered
     * streams and pipelines.
     *
     * @route GET /api/aes67
     * @response 200 AES67 status.
     */
    // AES67 status/test endpoint
    auto handleAES67 = [](const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback) {
        callback(AES67Manager::INSTANCE.render_GET(req));
    };
    app.registerHandler("/aes67", copyHandler(handleAES67), {drogon::Get, drogon::Head});
    app.registerHandlerViaRegex("/aes67/.*", copyHandler(handleAES67), {drogon::Get, drogon::Head});
#endif

#ifdef HAS_OPUS_RTP_GSTREAMER
    /**
     * Get Opus RTP status: whether it's active and the current pipelines.
     *
     * @route GET /api/opusrtp
     * @response 200 Opus RTP status.
     */
    auto handleOpusRTP = [](const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& callback) {
        callback(OpusRTPManager::INSTANCE.render_GET(req));
    };
    app.registerHandler("/opusrtp", copyHandler(handleOpusRTP), {drogon::Get, drogon::Head});
    app.registerHandlerViaRegex("/opusrtp/.*", copyHandler(handleOpusRTP), {drogon::Get, drogon::Head});
#endif

    // Plugin-published PipeWire audio sources (AudioSourceRegistry). Internal
    // to fppd -- the publicly documented route is the PHP passthrough,
    // GetPipeWirePluginSources() in www/api/controllers/pipewire.php, which
    // is what actually sits at /api/pipewire/audio/plugin-sources (this raw
    // fppd path isn't Apache-proxied directly, so it has no /api/ route of
    // its own to document here).
    auto handlePluginSources = [](const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback) {
        std::string body = SaveJsonToString(AudioSourceRegistry::INSTANCE.toJson());
        callback(makeStringResponse(body, 200, "application/json"));
    };
    app.registerHandler("/pipewire/audio/plugin-sources", copyHandler(handlePluginSources), {drogon::Get, drogon::Head});

    // Internal-only endpoint, deliberately undocumented (no @route
    // docblock) -- lists every HTTP route currently registered with the
    // embedded Drogon server, FPP's own included, so ServeOpenApiSpec() in
    // www/api/index.php can diff it against known @route docblocks and
    // surface whatever's left over as plugin-supplied (e.g. fpp-brightness's
    // /Brightness). Registered under /internal, not /fppd, specifically so
    // it does NOT appear in Apache's direct-proxy namespace whitelist
    // (etc/apache2.site) -- PHP reaches it the same way it reaches every
    // other fppd endpoint, a direct call to localhost:32322, never through
    // Apache. It's still technically reachable externally via the generic
    // /api/plugin-apis/<path> passthrough (that rule proxies any fppd path
    // unconditionally -- it's what makes third-party plugin routes work at
    // all, so it can't be restricted just for this one path), but it's no
    // longer listed anywhere or reachable at a guessable /api/ path.
    auto handlePluginApiRoutes = [](const HttpRequestPtr& req,
                                    std::function<void(const HttpResponsePtr&)>&& callback) {
        static const char* methodNames[] = {"GET", "POST", "HEAD", "PUT", "DELETE", "OPTIONS", "PATCH", "INVALID"};
        Json::Value result(Json::arrayValue);
        for (auto& info : drogon::app().getHandlersInfo()) {
            Json::Value entry;
            entry["path"] = std::get<0>(info);
            drogon::HttpMethod m = std::get<1>(info);
            entry["method"] = (m >= 0 && m <= drogon::Invalid) ? methodNames[m] : "UNKNOWN";
            entry["description"] = std::get<2>(info);
            result.append(entry);
        }
        callback(makeStringResponse(SaveJsonToString(result), 200, "application/json"));
    };
    app.registerHandler("/internal/pluginApiRoutes", copyHandler(handlePluginApiRoutes), {drogon::Get, drogon::Head});

    /**
     * Load or unload a plugin without restarting fppd, so installing a plugin can
     * take effect and uninstalling one can stop having effect mid-show.
     *
     * PluginManager mutates state the main loop walks every tick, and unloading
     * calls into the plugin to stop its threads, so the work is handed to the
     * main loop via a one-shot timer and this thread waits for the result. The
     * wait is bounded: a wedged main loop returns 503 rather than pinning one of
     * drogon's small pool of I/O threads forever.
     *
     * Documented under readable parameter names rather than the positional
     * {1}/{2} drogon registers it as - see the skip list in docs.php.
     *
     * @route POST /api/fppd/plugin/{PluginName}/{action}
     * @response 200 The plugin was loaded or unloaded.
     * ```json
     * {"Status": "OK", "Message": "", "plugin": "fpp-brightness", "loaded": false}
     * ```
     * @response 503 The main loop did not answer in time; nothing was changed.
     */
    auto handlePluginLifecycle = [](const HttpRequestPtr& req,
                                    std::function<void(const HttpResponsePtr&)>&& callback,
                                    const std::string& name, const std::string& action) {
        std::string clientIP = getEffectiveClientIP(req);
        // name and action are raw URL path segments - unvalidated at this point
        // (action isn't checked against "load" until below, and the reject branch
        // is by definition reached with a malformed name), so truncate before
        // logging rather than let a caller choose how many bytes of fppd.log a
        // single rejected request consumes.
        if (name.empty() || name.find('/') != std::string::npos) {
            LogWarn(VB_PLUGIN, "API - Rejected plugin %s request for invalid plugin name \"%s\" from %s\n",
                    TruncateForLog(action).c_str(), TruncateForLog(name).c_str(), clientIP.c_str());
            callback(makeStringResponse("{\"Status\":\"ERROR\",\"Message\":\"invalid plugin name\"}",
                                        400, "application/json"));
            return;
        }
        bool load = (action == "load");
        LogInfo(VB_PLUGIN, "API - %s plugin \"%s\" requested from %s\n",
                load ? "Loading" : "Unloading", TruncateForLog(name).c_str(), clientIP.c_str());
        // Timers replace a same-named entry, so a second request arriving while
        // one is queued would silently drop the first. Keep the names distinct.
        static std::atomic<uint64_t> seq{ 0 };
        std::string timerName = "PluginLifecycle-" + std::to_string(seq++);

        auto result = std::make_shared<std::promise<std::pair<bool, std::string>>>();
        auto done = result->get_future();
        Timers::INSTANCE.addTimer(timerName, GetTimeMS(), [result, name, load]() {
            std::string err;
            bool ok = load ? PluginManager::INSTANCE.loadPlugin(name, err)
                           : PluginManager::INSTANCE.unloadPlugin(name, err);
            result->set_value({ ok, err });
        });

        if (done.wait_for(std::chrono::seconds(30)) != std::future_status::ready) {
            LogErr(VB_PLUGIN, "API - %s plugin \"%s\" (from %s) timed out waiting for the main loop\n",
                   load ? "Loading" : "Unloading", TruncateForLog(name).c_str(), clientIP.c_str());
            callback(makeStringResponse("{\"Status\":\"ERROR\",\"Message\":\"timed out waiting for the main loop\"}",
                                        503, "application/json"));
            return;
        }
        auto [ok, err] = done.get();
        // err can carry dlerror() text, which is not length-bounded either.
        LogInfo(VB_PLUGIN, "API - %s plugin \"%s\" (from %s): %s%s%s\n",
                load ? "Load" : "Unload", TruncateForLog(name).c_str(), clientIP.c_str(),
                ok ? "OK" : "FAILED", err.empty() ? "" : " - ", TruncateForLog(err).c_str());
        Json::Value resp(Json::objectValue);
        resp["Status"] = ok ? "OK" : "ERROR";
        resp["Message"] = err;
        resp["plugin"] = name;
        resp["loaded"] = PluginManager::INSTANCE.isPluginLoaded(name);
        callback(makeStringResponse(SaveJsonToString(resp), ok ? 200 : 400, "application/json"));
    };
    app.registerHandler("/fppd/plugin/{1}/{2}", handlePluginLifecycle, {drogon::Post});

    // PlayerResource catch-all for all other /fppd/* paths (registered AFTER
    // specific /fppd/ports and /fppd/testing routes so they match first)
    app.registerHandlerViaRegex("/fppd/.*", copyHandler(handleFppd), {drogon::Get, drogon::Post, drogon::Put, drogon::Delete, drogon::Head});

    /**
     * List/manage pixel overlay models. See /api/models/* and /api/overlays/*
     * for per-model operations.
     *
     * @route GET /api/models
     * @response 200 Array of overlay models.
     */
    // PixelOverlayManager (/models/*, /overlays/*). NOTE: despite the shared
    // handler, methods are NOT uniformly supported across paths -- render_POST()
    // only acts on p1=="models" and render_PUT() only acts on p1=="overlays"
    // with a /model/{id}/... suffix (bare PUT /api/models 404s: "PUT Not
    // found /models"). Confirmed live; verify before adding more @route
    // docblocks here rather than assuming symmetry.
    auto handleModels = [](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback) {
        HttpResponsePtr resp;
        if (req->method() == drogon::Get || req->isHead())
            resp = PixelOverlayManager::INSTANCE.render_GET(req);
        else if (req->method() == drogon::Post)
            resp = PixelOverlayManager::INSTANCE.render_POST(req);
        else if (req->method() == drogon::Put)
            resp = PixelOverlayManager::INSTANCE.render_PUT(req);
        else
            resp = makeStringResponse("Method Not Allowed", 405);
        callback(resp);
    };
    app.registerHandler("/models", copyHandler(handleModels), {drogon::Get, drogon::Post, drogon::Put, drogon::Head});
    app.registerHandlerViaRegex("/models/.*", copyHandler(handleModels), {drogon::Get, drogon::Post, drogon::Put, drogon::Head});
    // No bare "/overlays" registration: it was never a documented,
    // intentional endpoint (checked the full doc history -- the pre-FPP10
    // endpoints.json help page only ever listed "overlays/models" and
    // "overlays/model/:ModelName", never bare "overlays"), Apache has never
    // proxied it externally, and grep finds zero internal callers anywhere
    // in the codebase. Only the real sub-paths are registered.
    app.registerHandlerViaRegex("/overlays/.*", copyHandler(handleModels), {drogon::Get, drogon::Post, drogon::Put, drogon::Head});

    // CommandManager (/command/*, /commands/*, /commandPresets/*)
    auto handleCommands = [](const HttpRequestPtr& req,
                             std::function<void(const HttpResponsePtr&)>&& callback) {
        HttpResponsePtr resp;
        if (req->method() == drogon::Get || req->isHead())
            resp = CommandManager::INSTANCE.render_GET(req);
        else if (req->method() == drogon::Post)
            resp = CommandManager::INSTANCE.render_POST(req);
        else
            resp = makeStringResponse("Method Not Allowed", 405);
        callback(resp);
    };
    // Bare /command has no content of its own -- it 404s from CommandManager
    // just like a bad sub-path would (confirmed by hitting it directly against
    // fppd:32322). Registered anyway because /command/* needs a parent match;
    // intentionally left undocumented since there's nothing real to document.
    app.registerHandler("/command", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});
    app.registerHandlerViaRegex("/command/.*", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});
    /**
     * List all defined commands.
     *
     * @route GET /api/commands
     * @response 200 Array of commands.
     */
    // render_POST() only acts when the path's first segment is literally
    // "command" (POST /api/command/{name}, already documented there) --
    // bare POST here, and POST /api/commandPresets below, both 404
    // "Not Found". Confirmed live.
    app.registerHandler("/commands", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});
    app.registerHandlerViaRegex("/commands/.*", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});
    /**
     * List all defined command presets.
     *
     * @route GET /api/commandPresets
     * @response 200 Array of command presets.
     */
    app.registerHandler("/commandPresets", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});
    app.registerHandlerViaRegex("/commandPresets/.*", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});

    // GPIOManager (/gpio/*)
    auto handleGpio = [](const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback) {
        HttpResponsePtr resp;
        if (req->method() == drogon::Get || req->isHead())
            resp = GPIOManager::INSTANCE.render_GET(req);
        else if (req->method() == drogon::Post)
            resp = GPIOManager::INSTANCE.render_POST(req);
        else
            resp = makeStringResponse("Method Not Allowed", 405);
        callback(resp);
    };
    /**
     * List all GPIO pins and their current state/configuration.
     *
     * @route GET /api/gpio
     * @response 200 Array of GPIO pins.
     */
    // render_POST() only acts on POST /api/gpio/{pin} (already documented
    // there); bare POST here 404s "Not Found". Confirmed live.
    app.registerHandler("/gpio", copyHandler(handleGpio), {drogon::Get, drogon::Post, drogon::Head});
    app.registerHandlerViaRegex("/gpio/.*", copyHandler(handleGpio), {drogon::Get, drogon::Post, drogon::Head});

    // Variables (/variables/*, reachable externally as /api/variables/* via the
    // Apache proxy whitelist in etc/apache2.site — same pattern as /gpio above)
    auto handleVariables = [](const HttpRequestPtr& req,
                              std::function<void(const HttpResponsePtr&)>&& callback) {
        HttpResponsePtr resp;
        if (req->method() == drogon::Get || req->isHead())
            resp = Variables::INSTANCE.render_GET(req);
        else if (req->method() == drogon::Post)
            resp = Variables::INSTANCE.render_POST(req);
        else if (req->method() == drogon::Delete)
            resp = Variables::INSTANCE.render_DELETE(req);
        else
            resp = makeStringResponse("Method Not Allowed", 405);
        callback(resp);
    };
    /**
     * List all variables and their current values.
     *
     * @route GET /api/variables
     * @response 200 Array of variables.
     */
    // Variables::render_POST()/render_DELETE() both require a name segment
    // (POST /variables/{name}, DELETE /variables/{name} -- already
    // documented there); bare POST/DELETE 400 "Bad Request". Confirmed live.
    app.registerHandler("/variables", copyHandler(handleVariables), { drogon::Get, drogon::Post, drogon::Delete, drogon::Head });
    app.registerHandlerViaRegex("/variables/.*", copyHandler(handleVariables), { drogon::Get, drogon::Post, drogon::Delete, drogon::Head });

    // Player (/player/*)
    auto handlePlayer = [](const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback) {
        HttpResponsePtr resp;
        if (req->method() == drogon::Get || req->isHead())
            resp = Player::INSTANCE.render_GET(req);
        else if (req->method() == drogon::Post)
            resp = Player::INSTANCE.render_POST(req);
        else if (req->method() == drogon::Put)
            resp = Player::INSTANCE.render_PUT(req);
        else
            resp = makeStringResponse("Method Not Allowed", 405);
        callback(resp);
    };
    /**
     * Get the current player state (playing/stopped, volume, current media, etc.).
     *
     * @route GET /api/player
     * @response 200 Current player state.
     */
    // Player::render_POST()/render_PUT() are both unconditional
    // `return makeStringResponse("Not Found", 404, ...)` -- registered for
    // completeness/future use but dead code today. Confirmed live. Do not
    // document POST/PUT here without checking Player.cpp again.
    app.registerHandler("/player", copyHandler(handlePlayer), {drogon::Get, drogon::Post, drogon::Put, drogon::Head});
    app.registerHandlerViaRegex("/player/.*", copyHandler(handlePlayer), {drogon::Get, drogon::Post, drogon::Put, drogon::Head});

    // Let plugins register their own routes
    PluginManager::INSTANCE.registerApis();

    // Configure and start drogon in a separate thread (since app().run() blocks).
    // Bind both v4 and v6 loopback so Apache's reverse-proxy works no
    // matter how libc resolves "localhost" (modern glibc returns ::1
    // first per RFC 6724).
    app.addListener(FPP_BIND_ADDRESS, FPP_HTTP_PORT);
    app.addListener("::1", FPP_HTTP_PORT);
    app.setThreadNum(10);
    app.disableSession();

    // Drogon's defaults are a 1MB max request body and a 64KB in-memory body,
    // above which it spools the body to a temp file. Both are too small for
    // FPP. The bulk pixel endpoints (PUT /api/overlays/model/{m}/data) send a
    // whole frame per request -- a 1024x1024 RGB model is 3MB, well past the
    // 1MB cap -- and spooling every frame of a repeating client to the SD card
    // would make that endpoint slower than the per-pixel API it replaces.
    // POST /api/models (model-overlays.json) was silently capped at 1MB too.
    //
    // The in-memory limit is what bounds worst-case RAM: setThreadNum(10) means
    // up to 10 concurrent bodies held in memory, so 4MB each is ~40MB worst
    // case -- affordable even on a 480MB BeagleBone. Requests larger than that
    // still work, they just go through a temp file.
    app.setClientMaxBodySize(32 * 1024 * 1024);
    app.setClientMaxMemoryBodySize(4 * 1024 * 1024);

    // Replace drogon's default SIGINT and SIGTERM handlers. Drogon's defaults
    // only stop drogon's own threads, leaving fppd's main loop spinning.
    // Drogon calls app().quit() itself before invoking these callbacks, so
    // drogon is already stopping by the time the main loop sees
    // runMainFPPDLoop == 0 and begins FPP's normal shutdown sequence.
    auto fppShutdown = []() { runMainFPPDLoop = 0; };
    app.setIntSignalHandler(fppShutdown);
    app.setTermSignalHandler(fppShutdown);

    // Register the /fppdws status push endpoint and its 1s change-poll timer.
    // The explicit call also keeps StatusWebSocket.o's controller
    // self-registration from being dropped by the linker.
    StatusWebSocketInit();

    m_serverThread = new std::thread([]() {
        drogon::app().run();
    });
    // Do NOT detach: ~APIServer() joins this thread after quit() so Drogon is
    // fully stopped before PluginManager::Cleanup() calls dlclose() on plugin
    // libraries. Plugin route handlers are lambdas with code in those libraries;
    // Drogon IO threads running after dlclose() -> SEGV_MAPERR on the vtable.
}

/*
 *
 */
void LogRequest(const HttpRequestPtr& req) {
    LogDebug(VB_HTTP, "API Req: %s%s from %s\n", req->path().c_str(),
             req->query().c_str(), getEffectiveClientIP(req).c_str());
}

/*
 *
 */
void LogResponse(const HttpRequestPtr& req, int responseCode, const std::string& content) {
    if (WillLog(LOG_EXCESSIVE, VB_HTTP)) {
        LogExcess(VB_HTTP, "API Res: %s%s %d %s\n",
                  req->path().c_str(),
                  req->query().c_str(),
                  responseCode,
                  content.c_str());
    }
}

PlayerResource::PlayerResource() {
#ifdef PLATFORM_PI
#define DEVICE_FILE_NAME "/dev/vcio"
#define MAJOR_NUM 100
#define IOCTL_MBOX_PROPERTY _IOWR(MAJOR_NUM, 0, char*)
#define MAX_STRING 1024
#define GET_GENCMD_RESULT 0x00030080
    piPowerFile = open(DEVICE_FILE_NAME, 0);
#else
    piPowerFile = -1;
#endif
}
PlayerResource::~PlayerResource() {
    if (piPowerFile > 0) {
        close(piPowerFile);
    }
}

/*
 *
 */
// --------------------------------------------------------------------------
// OpenAPI docs for the /fppd/* GET endpoints handled below.
// These @route docblocks are parsed by www/api/tools/generate_openapi.py; the
// fppd daemon serves them on port 32322 and Apache proxies them under /api/*.
// --------------------------------------------------------------------------

/**
 * List the effects currently running on the player.
 *
 * @route GET /api/fppd/effects
 * @response 200 Object with a `runningEffects` array.
 */

/**
 * Get the current fppd logging configuration (log level and enabled channels).
 *
 * @route GET /api/fppd/log
 * @response 200 Current log settings.
 */

/**
 * Get the full current player status (playlist, sequence, time, mode, etc.).
 *
 * @route GET /api/fppd/status
 * @response 200 Current player status JSON.
 */

/**
 * List the messages for all currently active warnings.
 *
 * @route GET /api/fppd/warnings
 * @response 200 Array of warning message strings.
 * ```json
 * ["Low disk space", "No network connection"]
 * ```
 */

/**
 * List all currently active warnings as full objects (message plus metadata).
 *
 * @route GET /api/fppd/warnings_full
 * @response 200 Array of warning objects.
 */

/**
 * Get the number of E1.31/sACN bytes received per universe.
 *
 * @route GET /api/fppd/e131stats
 * @response 200 E1.31 receive statistics.
 */

/**
 * List the MultiSync systems discovered on the network.
 *
 * @route GET /api/fppd/multiSyncSystems
 * @param int localOnly Set to 1 to return only the local system.
 * @response 200 MultiSync systems.
 */

/**
 * Get MultiSync packet statistics.
 *
 * @route GET /api/fppd/multiSyncStats
 * @param int reset Set to 1 to reset the statistics after reading them.
 * @response 200 MultiSync statistics.
 */

/**
 * List the playlists that are currently running.
 *
 * @route GET /api/fppd/playlists
 * @response 200 Currently running playlists.
 */

/**
 * Get the last-modified time of the running playlist file.
 *
 * @route GET /api/fppd/playlist/filetime
 * @response 200 Playlist file time.
 */

/**
 * Get the configuration of the running playlist.
 *
 * @route GET /api/fppd/playlist/config
 * @response 200 Playlist configuration.
 */

/**
 * Get the currently loaded schedule.
 *
 * @route GET /api/fppd/schedule
 * @response 200 Object with a `schedule` member.
 */

/**
 * Expand the schedule over an arbitrary date range.
 *
 * Unlike `/api/fppd/schedule`, which reports only the rolling window fppd has
 * actually scheduled out, this expands the configured schedule rules across any
 * requested range so past and future dates can be previewed. Occurrences are
 * advisory - they describe what the schedule says should happen, not what fppd
 * has committed to running.
 *
 * @route GET /api/fppd/schedule/range
 * @param int start Range start as epoch seconds.
 * @param int end Range end as epoch seconds. Must be after `start` and no more than 420 days later.
 * @param boolean includeDisabled Set to `1` to also expand disabled schedule entries.
 * @param boolean summary Set to `1` to collapse to one item per schedule entry per day, each carrying a `count` of that day's occurrences. Intended for month and other coarse views, where a repeating entry would otherwise return thousands of items.
 * @response 200 Object with a `schedule` member containing `entries`, `items`, `rangeStart` and `rangeEnd`.
 * @response 400 Missing, malformed, or excessively large range.
 */

/**
 * Get FPP version information.
 *
 * @route GET /api/fppd/version
 * @response 200 Version details.
 * ```json
 * {"version": "9.0", "majorVersion": 9, "minorVersion": 0, "branch": "master", "fppdAPI": 4, "Status": "OK"}
 * ```
 */

/**
 * Get (or set) the master output volume.
 *
 * @route GET /api/fppd/volume
 * @param int set New volume (0-100) to apply before returning.
 * @param boolean simple Return the volume as a bare text/plain integer instead of JSON.
 * @response 200 Object with a `volume` member (0-100).
 */

/**
 * Get the list of running sequences.
 *
 * @route GET /api/fppd/sequence
 * @response 200 Running sequences.
 */

/**
 * Dump the cached MQTT messages.
 *
 * @route GET /api/fppd/mqtt/cache
 * @response 200 Object keyed by topic, each value the topic's last cached message as a plain string.
 *   Per-topic last-updated timestamps are exposed separately via
 *   `GET /api/variables?mqtt=true` (`mqtt-<topic>` entries), not on this route.
 * @response 400 MQTT is not initialized.
 */
HttpResponsePtr PlayerResource::render_GET(const HttpRequestPtr& req) {
    LogRequest(req);

    Json::Value result;
    std::string resultStr = "";
    std::string url(req->path());

    if (!replaceStart(url, "/fppd/", ""))
        replaceStart(url, "/fppd", "");

    // Strip trailing slash to match endpoint definitions
    if (endsWith(url, "/"))
        url = url.substr(0, url.length() - 1);

    LogDebug(VB_HTTP, "URL: %s %s\n", url.c_str(), req->query().c_str());

    // Keep IF statement in alphabetical order
    if (url == "effects") {
        GetRunningEffects(result);
    } else if (url == "log") {
        GetLogSettings(result);
    } else if (url == "status") {
        GetCurrentStatus(result);
    } else if (url == "warnings") {
        result = Json::Value(Json::ValueType::arrayValue);
        for (auto& warn : WarningHolder::GetWarnings()) {
            result.append(warn.message());
        }
    } else if (url == "warnings_full") {
        result = Json::Value(Json::ValueType::arrayValue);
        for (auto& warn : WarningHolder::GetWarnings()) {
            result.append(warn.toJsonValue());
        }
    } else if (url == "e131stats") {
        GetE131BytesReceived(result);
    } else if (url == "multiSyncSystems") {
        bool localOnly = false;

        if (getRequestArg(req, "localOnly") == "1")
            localOnly = true;

        GetMultiSyncSystems(result, localOnly);
    } else if (url == "multiSyncStats") {
        bool reset = false;

        if (getRequestArg(req, "reset") == "1")
            reset = true;

        GetMultiSyncStats(result, reset);
    } else if (url == "playlists") {
        GetCurrentPlaylists(result);
    } else if (url == "playlist/filetime") {
        GetPlaylistFileTime(result);
    } else if (url == "playlist/config") {
        GetPlaylistConfig(result);
    } else if (url == "recurringtasks") {
        GetRecurringTasks(result);
    } else if (url == "condition/preview") {
        GetConditionPreview(req, result);
    } else if (url == "schedule") {
        result["schedule"] = scheduler->GetSchedule();
        SetOKResult(result, "");
    } else if (url == "schedule/range") {
        // Expanding the schedule costs time proportional to the range, so cap
        // what a single request can ask for rather than letting a stray query
        // hold the scheduler lock.
        constexpr int MAX_SCHEDULE_RANGE_DAYS = 420;

        time_t rangeStart = (time_t)std::atoll(getRequestArg(req, "start").c_str());
        time_t rangeEnd = (time_t)std::atoll(getRequestArg(req, "end").c_str());

        if ((rangeStart <= 0) || (rangeEnd <= rangeStart)) {
            SetErrorResult(result, 400, "start and end must be epoch seconds with end after start");
        } else if ((rangeEnd - rangeStart) > ((time_t)MAX_SCHEDULE_RANGE_DAYS * 86400)) {
            SetErrorResult(result, 400, "range may not exceed " + std::to_string(MAX_SCHEDULE_RANGE_DAYS) + " days");
        } else {
            bool includeDisabled = getRequestArg(req, "includeDisabled") == "1";
            bool daySummary = getRequestArg(req, "summary") == "1";

            result["schedule"] = scheduler->GetScheduleRange(rangeStart, rangeEnd,
                                                             includeDisabled, daySummary);
            SetOKResult(result, "");
        }
    } else if (url == "version") {
        result["version"] = getFPPVersion();
        result["majorVersion"] = getFPPMajorVersion();
        result["minorVersion"] = getFPPMinorVersion();
        result["branch"] = getFPPBranch();
        result["fppdAPI"] = FPPD_API_VERSION;

        SetOKResult(result, "");
    } else if (url == "volume") {
        if (getRequestArg(req, "set") != "") {
            int i = std::atoi(getRequestArg(req, "set").c_str());
            setVolume(i);
        }
        if (getRequestArg(req, "simple") == "true") {
            std::string v = std::to_string(getVolume());
            return makeStringResponse(v, 200, "text/plain");
        }
        result["volume"] = getVolume();
        SetOKResult(result, "");
    } else if (url == "sequence") {
        LogDebug(VB_HTTP, "API - Getting list of running sequences\n");
    } else if (url == "mqtt/cache") {
        LogDebug(VB_HTTP, "API - Getting MQTT Cached data\n");
        if (mqtt) {
            result["respCode"] = 200;
            result["Status"] = "OK";
            mqtt->dumpMessageCache(result);
        } else {
            result["Status"] = "ERROR";
            result["respCode"] = 400;
            result["Message"] = "Mqtt not Initialized";
        }
    } else {
        // url is an unvalidated caller-supplied path - truncate so an unknown
        // endpoint can't let a caller choose how many bytes of fppd.log each
        // request costs (bounded only by Apache's request-line limit).
        LogErr(VB_HTTP, "API - Error unknown GET request: %s\n", TruncateForLog(url).c_str());

        result["Status"] = "ERROR";
        result["respCode"] = 404;
        result["Message"] = std::string("endpoint fppd/") + url + " does not exist";
    }

    int responseCode = 200;
    if (resultStr.empty() && !result.isArray()) {
        if (result.empty()) {
            result["Status"] = "ERROR";
            result["respCode"] = 400;
            result["Message"] = "GET endpoint helper did not set result JSON";
        } else if (result.isMember("respCode")) {
            responseCode = result["respCode"].asInt();
        }
    }
    if (resultStr.empty()) {
        resultStr = SaveJsonToString(result);
    }
    LogResponse(req, responseCode, resultStr);

    return makeStringResponse(resultStr, responseCode, "application/json");
}

/*
 *
 */
// --------------------------------------------------------------------------
// OpenAPI docs for the /fppd/* POST endpoints handled below.
// --------------------------------------------------------------------------

/**
 * Start (or update) a named overlay effect on the player.
 *
 * @route POST /api/fppd/effects/{name}
 * @response 200 Effect started.
 */

/**
 * Re-read the Falcon hardware (e.g. cape/receiver) configuration.
 *
 * @route POST /api/fppd/falcon/hardware
 * @response 200 Hardware refreshed.
 */

/**
 * Set an external GPIO input state.
 *
 * @route POST /api/fppd/gpio/ext
 * @response 200 GPIO state updated.
 */

/**
 * Set logging levels.
 *
 * There is no separate channel parameter: the {level} path segment carries both
 * the level and the optional channel targeting. A bare level name - one of
 * error, warn, info, debug, or excess - sets that level globally for every log
 * channel. To target specific channels instead, use level:channel,channel and
 * separate multiple groups with a semicolon, e.g. debug:Schedule,Player;info:Sync.
 * Channel names are case-sensitive and match those returned by GET /api/fppd/log
 * (for example Command, Control, HTTP, Schedule, Sync).
 *
 * @route POST /api/fppd/log/level/{level}
 * @pathparam level enum:error,warn,info,debug,excess example:debug Log level to apply globally, or a level:channel[,channel] targeting expression.
 * @response 200 Log level updated.
 * @response 400 Invalid or unrecognized log level.
 */

/**
 * Apply a new channel-output configuration.
 *
 * @route POST /api/fppd/outputs
 * @response 200 Outputs updated.
 */

/**
 * Remap channel outputs.
 *
 * @route POST /api/fppd/outputs/remap
 * @response 200 Outputs remapped.
 */

/**
 * Replace the active schedule.
 *
 * @route POST /api/fppd/schedule
 * @response 200 Schedule updated.
 */

/**
 * Set the master output volume (0-100).
 *
 * @route POST /api/fppd/volume/{volume}
 * @response 200 Object with the new `volume`.
 */

/**
 * Shut down the fppd daemon.
 *
 * @route POST /api/fppd/shutdown
 * @response 200 fppd shutting down.
 */
HttpResponsePtr PlayerResource::render_POST(const HttpRequestPtr& req) {
    LogRequest(req);

    Json::Value data;
    Json::Value result;
    std::string url(req->path());

    if (!replaceStart(url, "/fppd/", ""))
        replaceStart(url, "/fppd", "");

    // Strip trailing slash to match endpoint definitions
    if (endsWith(url, "/"))
        url = url.substr(0, url.length() - 1);

    LogDebug(VB_HTTP, "POST URL: %s %s\n", url.c_str(), req->query().c_str());

    if (getRequestContent(req) != "") {
        if (!LoadJsonFromString(getRequestContent(req), data)) {
            LogErr(VB_CHANNELOUT, "Error parsing POST content\n");
            return makeStringResponse("Error parsing POST content", 400);
        }
    }

    // Keep IF statement in alphabetical order
    if (replaceStart(url, "effects/")) {
        PostEffects(url, data, result);
    } else if (replaceStart(url, "falcon/hardware")) {
        PostFalconHardware(result);
    } else if (replaceStart(url, "gpio/ext")) {
        PostGPIOExt(data, result);
    } else if (replaceStart(url, "log/level/")) {
        // SetLogLevelComplex returns false when the level (or targeted channel)
        // is not recognized and nothing was changed.  Report that back rather
        // than falsely claiming success.
        if (SetLogLevelComplex(url)) {
            // Persist the resulting per-channel levels to the settings file so
            // they survive a restart and stay in sync with the Logging settings
            // page (which reads/writes the LogLevel_* settings).
            for (FPPLoggerInstance* logger : FPPLogger::INSTANCE.allInstances()) {
                setSetting("LogLevel_" + logger->name, LogLevelToString(logger->level), true);
            }
            SetOKResult(result, "Log Level Updated");
        } else {
            SetErrorResult(result, 400, "Invalid or unrecognized log level: " + url);
        }
    } else if (url == "outputs") {
        PostOutputs(data, result);
    } else if (url == "outputs/remap") {
        PostOutputsRemap(data, result);
    } else if (url == "recurringtasks") {
        PostRecurringTasks(data, result);
    } else if (url == "schedule") {
        PostSchedule(data, result);
    } else if (url.find("volume/") == 0) {
        int volume = atoi(url.substr(7).c_str());
        setVolume(volume);
        result["volume"] = volume;
        SetOKResult(result, "Volume set");
    } else if (url == "shutdown") {
        SetOKResult(result, "Shutting down fppd");
        ShutdownFPPD();
    } else {
        LogErr(VB_HTTP, "API - Error unknown POST request: %s\n", TruncateForLog(url).c_str());

        result["Status"] = "ERROR";
        result["respCode"] = 404;
        result["Message"] = std::string("endpoint fppd/") + url + " does not exist";
    }

    if (!result.isMember("Status")) {
        result["Status"] = "ERROR";
        result["respCode"] = 400;
        result["Message"] = "POST endpoint helper did not set result JSON";
    }

    std::string resultStr = SaveJsonToString(result);
    LogResponse(req, result["respCode"].asInt(), resultStr);

    return makeStringResponse(resultStr.c_str(), result["respCode"].asInt(), "application/json");
}

/*
 *
 */
/**
 * Reset the E1.31/sACN receive statistics.
 *
 * @route DELETE /api/fppd/e131stats
 * @response 200 Statistics cleared.
 */
HttpResponsePtr PlayerResource::render_DELETE(const HttpRequestPtr& req) {
    LogRequest(req);

    Json::Value result;
    std::string url(req->path());

    if (!replaceStart(url, "/fppd/", ""))
        replaceStart(url, "/fppd", "");

    // Strip trailing slash to match endpoint definitions
    if (endsWith(url, "/"))
        url = url.substr(0, url.length() - 1);

    LogDebug(VB_HTTP, "DELETE URL: %s %s\n", url.c_str(), req->query().c_str());

    // Keep IF statement in alphabetical order
    if (url == "e131stats") {
        LogDebug(VB_HTTP, "Resetting E131 Statistics");
        ResetBytesReceived();
        SetOKResult(result, "Stats Cleared");
    } else {
        LogErr(VB_HTTP, "API - Error unknown DELETE request: %s\n", TruncateForLog(url).c_str());

        result["Status"] = "ERROR";
        result["respCode"] = 404;
        result["Message"] = std::string("endpoint fppd/") + url + " does not exist";
    }

    if (!result.isMember("Status")) {
        result["Status"] = "ERROR";
        result["respCode"] = 400;
        result["Message"] = "DELETE endpoint helper did not set result JSON";
    }

    std::string resultStr = SaveJsonToString(result);

    LogResponse(req, result["respCode"].asInt(), resultStr);

    return makeStringResponse(resultStr.c_str(), result["respCode"].asInt());
}

/*
 *
 */
HttpResponsePtr PlayerResource::render_PUT(const HttpRequestPtr& req) {
    LogRequest(req);

    Json::Value result;
    std::string url(req->path());

    if (!replaceStart(url, "/fppd/", ""))
        replaceStart(url, "/fppd", "");

    // Strip trailing slash to match endpoint definitions
    if (endsWith(url, "/"))
        url = url.substr(0, url.length() - 1);

    LogDebug(VB_HTTP, "PUT URL: %s %s\n", url.c_str(), req->query().c_str());

    // No PUT endpoints are currently implemented under /fppd/*.
    LogErr(VB_HTTP, "API - Error unknown PUT request: %s\n", TruncateForLog(url).c_str());

    result["Status"] = "ERROR";
    result["respCode"] = 404;
    result["Message"] = std::string("endpoint fppd/") + url + " does not exist";

    std::string resultStr = SaveJsonToString(result);

    LogResponse(req, result["respCode"].asInt(), resultStr);

    return makeStringResponse(resultStr.c_str(), result["respCode"].asInt());
}

/*
 *
 */
void PlayerResource::SetOKResult(Json::Value& result, const std::string& msg) {
    result["Status"] = "OK";
    result["respCode"] = 200;
    result["Message"] = msg;
}

/*
 *
 */
void PlayerResource::SetErrorResult(Json::Value& result, const int respCode, const std::string& msg) {
    result["Status"] = "ERROR";
    result["respCode"] = respCode;
    result["Message"] = msg;
}

/*
 *
 */
void PlayerResource::GetRunningEffects(Json::Value& result) {
    LogDebug(VB_HTTP, "API - Getting list of running effects\n");
    SetOKResult(result, "");
    result["runningEffects"] = GetRunningEffectsJson();
}

/*
 *
 */
void PlayerResource::GetLogSettings(Json::Value& result) {
    SetOKResult(result, "");
    Json::Value log;
    std::vector<FPPLoggerInstance*> allLoggers = FPPLogger::INSTANCE.allInstances();
    std::vector<FPPLoggerInstance*>::iterator it;

    for (it = allLoggers.begin(); it != allLoggers.end(); it++) {
        FPPLoggerInstance* ptr = *it;
        log[ptr->name] = LogLevelToString(ptr->level);
    }

    result["log"] = log;
}

/*
 *
 */
void PlayerResource::GetCurrentStatus(Json::Value& result) {
    LogDebug(VB_HTTP, "API - Getting fppd status\n");
    GetCurrentFPPDStatus(result);
}

/*
 *
 */
void PlayerResource::GetCurrentPlaylists(Json::Value& result) {
    LogDebug(VB_HTTP, "API - Getting current playlist\n");

    Json::Value names(Json::arrayValue);

    if (Player::INSTANCE.IsPlaying())
        names.append(Player::INSTANCE.GetPlaylistName());

    result["playlists"] = names;

    SetOKResult(result, "");
}

/*
 *
 */
void PlayerResource::GetE131BytesReceived(Json::Value& result) {
    result = GetE131UniverseBytesReceived(); // e131bridge.cpp

    if (result.isMember("universes"))
        SetOKResult(result, "");
    else
        SetErrorResult(result, 400, "GetE131UniverseBytesReceived() did not return any universes.");
}

/*
 *
 */
void PlayerResource::GetMultiSyncSystems(Json::Value& result, bool localOnly) {
    result = multiSync->GetSystems(localOnly);

    if (result.isMember("systems"))
        SetOKResult(result, "");
    else
        SetErrorResult(result, 400, "MultiSync did not return any systems.");
}

void PlayerResource::GetMultiSyncStats(Json::Value& result, bool reset) {
    if (reset)
        multiSync->ResetSyncStats();

    result = multiSync->GetSyncStats();

    if (result.isMember("systems"))
        SetOKResult(result, "");
    else
        SetErrorResult(result, 400, "MultiSync did not return any systems.");
}

/*
 *
 */
void PlayerResource::GetPlaylistFileTime(Json::Value& result) {
    if (Player::INSTANCE.IsPlaying()) {
        uint64_t fileTime = Player::INSTANCE.GetFileTime();

        result["fileTime"] = (Json::UInt64)fileTime;
    } else {
        result["fileTime"] = 0;
    }

    SetOKResult(result, "");
}

/*
 *
 */
void PlayerResource::GetPlaylistConfig(Json::Value& result) {
    if (Player::INSTANCE.IsPlaying())
        result = Player::INSTANCE.GetConfig();

    SetOKResult(result, "");
}

/**
 * Report the configured Recurring Tasks merged with last-run status, for the
 * Recurring Tasks admin page (www/recurringtasks.php).
 *
 * @route GET /api/recurringtasks
 * @response 200 {"tasks": [...]}
 */
void PlayerResource::GetRecurringTasks(Json::Value& result) {
    result["tasks"] = Json::Value(Json::arrayValue);
    RecurringTasks::INSTANCE.reportStatus(result["tasks"]);
    SetOKResult(result, "");
}

/**
 * Preview a single If/Conditional Check leaf - the same lookup evaluate()
 * uses internally (Variable/Expression/Time/GPIO Pin/Sun), before any
 * comparator/Value is applied. Backs the If condition editor's "Show Current
 * Value" button, so picking the right Value to compare against isn't
 * guesswork.
 *
 * When a "comparator" param is also supplied (the consolidated eye-preview
 * modal's full-leaf mode), also evaluates Value the same way Value is
 * unconditionally evaluated at runtime and applies the comparator, returning
 * the RHS value and the boolean result too - reuses
 * ConditionNode::PreviewLeafResult(), the exact same evaluation path a real
 * saved leaf's evaluate() takes, so this can never drift from runtime
 * behavior.
 *
 * @route GET /api/fppd/condition/preview
 * @param string source One of the If Check "Source" dropdown values (e.g. "Variable", "GPIO Pin").
 * @param string name The Name/Expression field for that source.
 * @param string comparator Optional - one of the Check "Comparator" values. Enables full-leaf mode.
 * @param string value Optional - the Value field, only used when "comparator" is also given.
 * @param string not Optional - "true" to negate the result, only used when "comparator" is also given.
 * @response 200 {"found": true, "value": "..."} (source/name-only mode) or
 *           {"found": true, "value": "...", "rhsValue": "...", "result": true} (full-leaf mode, LHS resolved) or
 *           {"found": false, "rhsValue": "...", "result": true} (full-leaf mode, LHS not found - rhsValue/result
 *           are still evaluated, since Value always evaluates and a missing LHS just yields `negate`) or
 *           {"found": false} if the source/name doesn't currently resolve (source/name-only mode).
 */
void PlayerResource::GetConditionPreview(const HttpRequestPtr& req, Json::Value& result) {
    std::string source = getRequestArg(req, "source");
    std::string name = getRequestArg(req, "name");
    if (req->getParameter("comparator").empty()) {
        LogDebug(VB_HTTP, "API - Getting condition preview for source \"%s\" name \"%s\"\n", source.c_str(), name.c_str());
        bool found = false;
        std::string value = ConditionNode::PreviewSourceValue(source, name, found);
        result["found"] = found;
        if (found) {
            result["value"] = value;
        }
        SetOKResult(result, "");
        return;
    }
    std::string comparator = getRequestArg(req, "comparator");
    std::string value = getRequestArg(req, "value");
    bool negate = getRequestArg(req, "not") == "true";
    LogDebug(VB_HTTP, "API - Getting full condition preview for source \"%s\" name \"%s\" %s%s \"%s\"\n",
             source.c_str(), name.c_str(), negate ? "NOT " : "", comparator.c_str(), value.c_str());
    bool lhsFound = false;
    std::string lhsValue, rhsValue;
    bool leafResult = false;
    ConditionNode::PreviewLeafResult(source, name, comparator, value, negate, lhsFound, lhsValue, rhsValue, leafResult);
    result["found"] = lhsFound;
    if (lhsFound) {
        result["value"] = lhsValue;
    }
    // rhsValue/result are always resolvable (Value is unconditionally
    // evaluated, and a not-found LHS still yields a definite result of
    // `negate`), so return them regardless of lhsFound - lets the eye-preview
    // modal show what Value evaluates to instead of stopping cold when only
    // the LHS side is missing.
    result["rhsValue"] = rhsValue;
    result["result"] = leafResult;
    SetOKResult(result, "");
}

/*
 *
 */
void PlayerResource::PostEffects(const std::string& effectName, const Json::Value& data, Json::Value& result) {
    if (!data.isMember("command")) {
        SetErrorResult(result, 400, "'command' field not specified");
        return;
    }

    std::string command = data["command"].asString();

    LogDebug(VB_HTTP, "API - PostEffect(%s) cmd: %s\n", effectName.c_str(), command.c_str());

    SetOKResult(result, "PostEffect result would go here");
}

/*
 *
 */
void PlayerResource::PostFalconHardware(Json::Value& result) {
    LogDebug(VB_HTTP, "API - Reloading hardware config\n");

    SetOKResult(result, "PostFalconHardware result would go here");
}

/*
 *
 */
void PlayerResource::PostGPIOExt(const Json::Value& data, Json::Value& result) {
    if (!data.isMember("config")) {
        SetErrorResult(result, 400, "'config' field not specified");
        return;
    }

    LogDebug(VB_HTTP, "API - Doing something with gpio\n");
}

/*
 *
 */
void PlayerResource::PostOutputs(const Json::Value& data, Json::Value& result) {
    if (!data.isMember("command")) {
        SetErrorResult(result, 400, "'command' field not specified");
        return;
    }

    if (data["command"].asString() == "enable") {
        EnableChannelOutput();
        SetOKResult(result, "channel output enabled");
    } else if (data["command"].asString() == "disable") {
        DisableChannelOutput();
        SetOKResult(result, "channel output disabled");
    }
}

/*
 *
 */
void PlayerResource::PostOutputsRemap(const Json::Value& data, Json::Value& result) {
    if (!data.isMember("command")) {
        SetErrorResult(result, 400, "'command' field not specified");
        return;
    }

    if (data["command"].asString() == "reload") {
        // FIXME, need to fix this function to lock the remap array
        // LoadChannelRemapData();
        SetOKResult(result, "channel remaps reloaded");
    }
}

/**
 * Re-read config/recurringtasks.json and re-schedule all Recurring Task
 * timers to match, so a save on www/recurringtasks.php (which writes that
 * file via the generic api/configfile endpoint) takes effect without an
 * fppd restart. Also handles an immediate "Test Run" of one task, run
 * synchronously against the posted task definition (not a saved name), so
 * the admin page can preview unsaved edits.
 *
 * @route POST /api/recurringtasks
 * @body {"command": "reload"}
 * @body {"command": "test", "task": {"name": "...", "type": "command", "command": "URL", "args": [...], "filterType": "json", "filterJsonPath": "data.temp"}}
 * @response 200 Recurring tasks reloaded, or {"ok":bool,"raw":string,"filtered":string,"error":string} for "test".
 * @response 400 'command' field not specified.
 */
void PlayerResource::PostRecurringTasks(const Json::Value& data, Json::Value& result) {
    if (!data.isMember("command")) {
        SetErrorResult(result, 400, "'command' field not specified");
        return;
    }

    std::string command = data["command"].asString();
    if (command == "reload") {
        RecurringTasks::INSTANCE.Reload();
        SetOKResult(result, "Recurring tasks reloaded");
    } else if (command == "test") {
        Json::Value testResult = RecurringTasks::INSTANCE.TestRunTask(data["task"]);
        for (auto const& key : testResult.getMemberNames()) {
            result[key] = testResult[key];
        }
        SetOKResult(result, "");
    }
}

/*
 *
 */
void PlayerResource::PostSchedule(const Json::Value data, Json::Value& result) {
    if (!data.isMember("command")) {
        SetErrorResult(result, 400, "'command' field not specified");
        return;
    }

    if (data["command"].asString() == "reload") {
        scheduler->ReloadScheduleFile();

        SetOKResult(result, "Schedule reload triggered");
    }
}

void PlayerResource::periodicWork() {
    if (piPowerFile > 0) {
#ifdef PLATFORM_PI
        int i = 0;
        int32_t p[(MAX_STRING >> 2) + 7];
        p[i++] = 0;          // size
        p[i++] = 0x00000000; // process request

        p[i++] = GET_GENCMD_RESULT; // (the tag id)
        p[i++] = MAX_STRING;        // buffer_len
        p[i++] = 0;                 // request_len (set to response length)
        p[i++] = 0;                 // error repsonse

        memcpy(p + i, "get_throttled", strlen("get_throttled") + 1);
        i += MAX_STRING >> 2;
        p[i++] = 0x00000000;  // end tag
        p[0] = i * sizeof *p; // actual size

        int ret_val = ioctl(piPowerFile, IOCTL_MBOX_PROPERTY, p);
        std::string s = (char*)&p[6];
        if (s.starts_with("throttled=0x")) {
            s = s.substr(12);
            uint32_t res = std::stol(s, nullptr, 16);
            piPowerBad = res & 0x1;
            if (piPowerBad) {
                piPowerWarningCount++;
                if (piPowerWarningCount > 5 && !piPowerWarningAdded) {
                    WarningHolder::AddWarning(15, "Raspberry Pi Voltage Too Low");
                    piPowerWarningAdded = true;
                }
            } else {
                piPowerWarningCount = 0;
            }
        }
#endif
    }
}

void APIServer::periodicWork() {
    if (m_pr) {
        m_pr->periodicWork();
    }
}
