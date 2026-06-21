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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <stdint.h>
#include <string>
#include <vector>

#include "MultiSync.h"
#include "OutputMonitor.h"
#include "Player.h"
#include "Plugins.h"
#include "Scheduler.h"
#include "Sequence.h"
#include "Warnings.h"
#include "common.h"
#include "e131bridge.h"
#include "effects.h"
#include "fppversion.h"
#include "gpio.h"
#include "log.h"
#include "mqtt.h"
#include "settings.h"
#include "channeloutput/channeloutputthread.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "channeltester/ChannelTester.h"
#include "commands/Commands.h"
#include "mediaoutput/AES67Manager.h"
#include "mediaoutput/OpusRTPManager.h"
#include "mediaoutput/MediaOutputBase.h"
#include "mediaoutput/MediaOutputStatus.h"
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
    auto tm = *std::localtime(&t);
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
        result["warningInfo"].append(warn);
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
        if (mediaOutput) {
            mediaFilename = mediaOutput->m_mediaFilename;
            secsElapsed = mediaOutputStatus.secondsElapsed;
            secsRemaining = mediaOutputStatus.secondsRemaining;
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
    // the more specific /fppd/* routes below so they take priority.
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
    // AES67 status/test endpoint
    auto handleAES67 = [](const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback) {
        callback(AES67Manager::INSTANCE.render_GET(req));
    };
    app.registerHandler("/aes67", copyHandler(handleAES67), {drogon::Get, drogon::Head});
    app.registerHandlerViaRegex("/aes67/.*", copyHandler(handleAES67), {drogon::Get, drogon::Head});
#endif

#ifdef HAS_OPUS_RTP_GSTREAMER
    auto handleOpusRTP = [](const HttpRequestPtr& req,
                            std::function<void(const HttpResponsePtr&)>&& callback) {
        callback(OpusRTPManager::INSTANCE.render_GET(req));
    };
    app.registerHandler("/opusrtp", copyHandler(handleOpusRTP), {drogon::Get, drogon::Head});
    app.registerHandlerViaRegex("/opusrtp/.*", copyHandler(handleOpusRTP), {drogon::Get, drogon::Head});
#endif

    // PlayerResource catch-all for all other /fppd/* paths (registered AFTER
    // specific /fppd/ports and /fppd/testing routes so they match first)
    app.registerHandlerViaRegex("/fppd/.*", copyHandler(handleFppd), {drogon::Get, drogon::Post, drogon::Put, drogon::Delete, drogon::Head});

    // PixelOverlayManager (/models/*, /overlays/*)
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
    app.registerHandler("/overlays", copyHandler(handleModels), {drogon::Get, drogon::Post, drogon::Put, drogon::Head});
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
    app.registerHandler("/command", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});
    app.registerHandlerViaRegex("/command/.*", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});
    app.registerHandler("/commands", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});
    app.registerHandlerViaRegex("/commands/.*", copyHandler(handleCommands), {drogon::Get, drogon::Post, drogon::Head});
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
    app.registerHandler("/gpio", copyHandler(handleGpio), {drogon::Get, drogon::Post, drogon::Head});
    app.registerHandlerViaRegex("/gpio/.*", copyHandler(handleGpio), {drogon::Get, drogon::Post, drogon::Head});

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

    // Replace drogon's default SIGINT and SIGTERM handlers. Drogon's defaults
    // only stop drogon's own threads, leaving fppd's main loop spinning.
    // Drogon calls app().quit() itself before invoking these callbacks, so
    // drogon is already stopping by the time the main loop sees
    // runMainFPPDLoop == 0 and begins FPP's normal shutdown sequence.
    auto fppShutdown = []() { runMainFPPDLoop = 0; };
    app.setIntSignalHandler(fppShutdown);
    app.setTermSignalHandler(fppShutdown);

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
    LogDebug(VB_HTTP, "API Req: %s%s\n", req->path().c_str(),
             req->query().c_str());
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
 * @response 200 Cached MQTT messages.
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
            result.append(warn);
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
    } else if (url == "schedule") {
        result["schedule"] = scheduler->GetSchedule();
        SetOKResult(result, "");
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
        LogErr(VB_HTTP, "API - Error unknown GET request: %s\n", url.c_str());

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
 * Set the logging level for a specific log channel.
 *
 * @route POST /api/fppd/log/level/{level}
 * @response 200 Log level updated.
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
 * Stop all running playlists.
 *
 * @route POST /api/fppd/playlists/stop
 * @response 200 All playlists stopped.
 */

/**
 * Start a playlist by name.
 *
 * @route POST /api/fppd/playlists/{name}/start
 * @response 200 Playlist started.
 */

/**
 * Stop a running playlist by name.
 *
 * @route POST /api/fppd/playlists/{name}/stop
 * @response 200 Playlist stopped.
 */

/**
 * Skip to the next item in a running playlist.
 *
 * @route POST /api/fppd/playlists/{name}/nextItem
 * @response 200 Advanced to next item.
 */

/**
 * Skip to the previous item in a running playlist.
 *
 * @route POST /api/fppd/playlists/{name}/prevItem
 * @response 200 Moved to previous item.
 */

/**
 * Restart the current item in a running playlist.
 *
 * @route POST /api/fppd/playlists/{name}/restartItem
 * @response 200 Current item restarted.
 */

/**
 * Jump to a named section (mainPlaylist, leadIn, leadOut) of a playlist.
 *
 * @route POST /api/fppd/playlists/{name}/section/{section}
 * @response 200 Jumped to section.
 */

/**
 * Jump to a specific item index in a playlist.
 *
 * @route POST /api/fppd/playlists/{name}/item/{item}
 * @response 200 Jumped to item.
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
 * Start playing a sequence by name.
 *
 * @route POST /api/fppd/sequences/{name}/start
 * @response 200 Sequence started.
 */

/**
 * Stop a running sequence by name.
 *
 * @route POST /api/fppd/sequences/{name}/stop
 * @response 200 Sequence stopped.
 */

/**
 * Toggle pause on a running sequence (append /0 to resume, /1 to pause).
 *
 * @route POST /api/fppd/sequences/{name}/pause
 * @response 200 Pause state toggled.
 */

/**
 * Step a paused sequence forward (optionally by /{frames}).
 *
 * @route POST /api/fppd/sequences/{name}/step
 * @response 200 Sequence stepped forward.
 */

/**
 * Step a paused sequence backward (optionally by /{frames}).
 *
 * @route POST /api/fppd/sequences/{name}/back
 * @response 200 Sequence stepped backward.
 */

/**
 * Reload all settings from disk.
 *
 * @route POST /api/fppd/settings/reload
 * @response 200 Settings reloaded.
 */

/**
 * Reload a single setting from disk.
 *
 * @route POST /api/fppd/settings/reload/{setting}
 * @response 200 Setting reloaded.
 */

/**
 * Restart the fppd daemon.
 *
 * @route POST /api/fppd/restart
 * @response 200 fppd restarting.
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
        SetLogLevelComplex(url);
        SetOKResult(result, "Log Level Updated");
    } else if (url == "outputs") {
        PostOutputs(data, result);
    } else if (url == "outputs/remap") {
        PostOutputsRemap(data, result);
    } else if (replaceStart(url, "playlists/")) {
        if (url == "stop") {
            // Stop all running playlists
            LogDebug(VB_HTTP, "API - Stopping all running playlists w/ content '%s'\n", getRequestContent(req).c_str());
        } else if (endsWith(url, "/start")) {
            // Start a playlist
            replaceEnd(url, "/start", "");
            LogDebug(VB_HTTP, "API - Starting playlist '%s' w/ content '%s'\n", url.c_str(), getRequestContent(req).c_str());
        } else if (endsWith(url, "/nextItem")) {
            replaceEnd(url, "/nextItem", "");
            LogDebug(VB_HTTP, "API - Skipping to next entry in playlist '%s'\n", url.c_str());
        } else if (endsWith(url, "/restartItem")) {
            replaceEnd(url, "/restartItem", "");
            LogDebug(VB_HTTP, "API - Restarting current item in playlist '%s'\n", url.c_str());
        } else if (endsWith(url, "/prevItem")) {
            replaceEnd(url, "/prevItem", "");
            LogDebug(VB_HTTP, "API - Skipping to prev entry in playlist '%s'\n", url.c_str());
        } else if (url.find("/section/") != std::string::npos) {
            std::string playlistName = url.substr(0, url.find("/section/"));
            std::string section = url.substr(url.rfind("/section/") + 6);
            LogDebug(VB_HTTP, "API - Jumping to section '%s' in playlist '%s'\n", section.c_str(), url.c_str());
        } else if (url.find("/item/") != std::string::npos) {
            std::string playlistName = url.substr(0, url.find("/item/"));
            int item = atoi(url.substr(url.rfind("/item/") + 6).c_str());
            LogDebug(VB_HTTP, "API - Jumping to item %d in playlist '%s'\n", item, playlistName.c_str());
        } else if (endsWith(url, "/stop")) {
            replaceEnd(url, "/stop", "");
            LogDebug(VB_HTTP, "API - Stopping playlist '%s' w/ content '%s'\n", url.c_str(), getRequestContent(req).c_str());
        }
    } else if (url == "schedule") {
        PostSchedule(data, result);
    } else if (url.find("volume/") == 0) {
        int volume = atoi(url.substr(7).c_str());
        setVolume(volume);
        result["volume"] = volume;
        SetOKResult(result, "Volume set");
    } else if (url.find("sequences/") == 0) {
        replaceStart(url, "sequences/", "");

        if (endsWith(url, "/start")) {
            replaceEnd(url, "/start", "");
            LogDebug(VB_HTTP, "API - Starting sequence '%s'\n", url.c_str());
        } else if (endsWith(url, "/stop")) {
            replaceEnd(url, "/stop", "");
            LogDebug(VB_HTTP, "API - Stopping sequence '%s'\n", url.c_str());
        } else if (endsWith(url, "/pause")) {
            replaceEnd(url, "/pause", "");
            LogDebug(VB_HTTP, "API - (un)Pausing sequence '%s'\n", url.c_str());
        } else if (endsWith(url, "/pause/0")) {
            LogDebug(VB_HTTP, "API - UnPausing sequence '%s'\n", url.c_str());
        } else if (endsWith(url, "/pause/1")) {
            LogDebug(VB_HTTP, "API - Pausing sequence '%s'\n", url.c_str());
        } else if (endsWith(url, "/step")) {
            replaceEnd(url, "/step", "");
            LogDebug(VB_HTTP, "API - Stepping sequence '%s' by 1 frame\n", url.c_str());
        } else if (endsWith(url, "/back")) {
            replaceEnd(url, "/back", "");
            LogDebug(VB_HTTP, "API - Stepping sequence '%s' BACK by 1 frame\n", url.c_str());
        } else if (url.find("/step/") != std::string::npos) {
            std::string sequenceName = url.substr(0, url.find("/step/"));
            int frames = atoi(url.substr(url.find("/step/") + 6).c_str());
            LogDebug(VB_HTTP, "API - Stepping sequence '%s' by %d frame(s)\n", sequenceName.c_str(), frames);
        } else if (url.find("/back/") != std::string::npos) {
            std::string sequenceName = url.substr(0, url.find("/back/"));
            int frames = atoi(url.substr(url.find("/back/") + 6).c_str());
            LogDebug(VB_HTTP, "API - Stepping sequence '%s' BACK by %d frame(s)\n", sequenceName.c_str(), frames);
        }
    } else if (url == "settings/reload") {
        LogDebug(VB_HTTP, "API - Reloading all settings\n");
    } else if (replaceStart(url, "settings/reload/")) {
        LogDebug(VB_HTTP, "API - Reloading setting: %s\n", url.c_str());
    } else if (url == "restart") {
        LogDebug(VB_HTTP, "API - Restarting fppd daemon\n");
    } else if (url == "shutdown") {
        SetOKResult(result, "Shutting down fppd");
        ShutdownFPPD();
    } else {
        LogErr(VB_HTTP, "API - Error unknown POST request: %s\n", url.c_str());

        result["Status"] = "ERROR";
        result["respCode"] = 404;
        result["Message"] = std::string("endpoint fppd/") + url + " does not exist";
    }

    if (!result.isMember("status")) {
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
        LogErr(VB_HTTP, "API - Error unknown DELETE request: %s\n", url.c_str());

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
/**
 * Update the runtime settings of a running playlist.
 *
 * @route PUT /api/fppd/playlists/{name}/settings
 * @response 200 Playlist settings updated.
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

    // Keep IF statement in alphabetical order
    if (replaceStart(url, "playlists/")) {
        if (endsWith(url, "/settings")) {
            replaceEnd(url, "/settings", "");

            LogDebug(VB_HTTP, "API - Updating runtime settings for playlist '%s'\n", url.c_str());
        }
    } else {
        LogErr(VB_HTTP, "API - Error unknown PUT request: %s\n", url.c_str());

        result["Status"] = "ERROR";
        result["respCode"] = 404;
        result["Message"] = std::string("endpoint fppd/") + url + " does not exist";
    }

    if (!result.isMember("status")) {
        result["Status"] = "ERROR";
        result["respCode"] = 400;
        result["Message"] = "PUT endpoint helper did not set result JSON";
    }

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
