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

#include "fpp-pch.h"

#ifdef PLATFORM_OSX
#include <sys/sysctl.h>
#else
#include <sys/sysinfo.h>
#endif

#include <cstdlib>
#include <ctime>
#include <cxxabi.h>
#include <fstream>
#include <httpserver.hpp>
#include <iomanip>
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
#include "channeltester/ChannelTester.h"
#include "commands/Commands.h"
#include "mediaoutput/MediaOutputBase.h"
#include "mediaoutput/MediaOutputStatus.h"
#include "mediaoutput/mediaoutput.h"
#include "overlays/PixelOverlay.h"
#include "playlist/Playlist.h"
#include "sensors/Sensors.h"

#include "httpAPI.h"

static std::time_t startupTime = std::time(nullptr);

/*
 Build a Status JSON String
*/
void GetCurrentFPPDStatus(Json::Value& result) {
    static std::string UUID = getSetting("SystemUUID");

    int mode = getFPPmode();
    result["fppd"] = "running";
    result["uuid"] = UUID;
    result["mode"] = mode;
    result["mode_name"] = toStdStringAndFree(modeToString(getFPPmode()));
    result["status"] = Player::INSTANCE.GetStatus();
    result["bridging"] = HasBridgeData();
    result["multisync"] = multiSync->isMultiSyncEnabled();

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

    std::time_t timeDiff = std::time(nullptr) - startupTime;
    int totalseconds = (int)timeDiff;

#ifdef PLATFORM_OSX
    struct timeval boot;
    struct timeval now;
    int mib[] = { CTL_KERN, KERN_BOOTTIME };
    size_t size = sizeof(boot);
    if (sysctl(mib, 2, &boot, &size, 0, 0) == 0 && gettimeofday(&now, 0)) {
        uint64_t uptime_micro = (now.tv_sec * 1000000 + now.tv_usec) - (boot.tv_sec * 1000000 + boot.tv_usec);
        totalseconds = (int)(uptime_micro / 1000000);
    }
#else
    struct sysinfo sysInf;
    if (sysinfo(&sysInf) == 0) {
        int uptimeSecs = sysInf.uptime;
        if (uptimeSecs < totalseconds) {
            startupTime = std::time(nullptr);
            timeDiff = std::time(nullptr) - startupTime;
            totalseconds = (int)timeDiff;
        }
    }
#endif
    double days = ((double)timeDiff) / 86400;
    double hours = ((double)(totalseconds % 86400)) / 3600;
    int seconds = totalseconds % 86400 % 3600;
    double minutes = ((double)seconds) / 60;
    seconds = seconds % 60;

    result["uptime"] = secondsToTime((int)timeDiff);
    result["uptimeTotalSeconds"] = (int)timeDiff;
    result["uptimeSeconds"] = seconds;
    result["uptimeDays"] = days;
    result["uptimeMinutes"] = minutes;
    result["uptimeHours"] = hours;
    result["uptimeDays"] = days;

    std::stringstream totalTime;
    totalTime << (int)days << " days, " << (int)hours << " hours, " << (int)minutes << " minutes, " << seconds << " seconds";
    result["uptimeStr"] = totalTime.str();

    Sensors::INSTANCE.reportSensors(result);
    std::list<std::string> warnings = WarningHolder::GetWarnings();
    for (auto& warn : warnings) {
        result["warnings"].append(warn);
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
        result["media_filename"] = mediaFilename;
        result["current_sequence"] = seqFilename;
        result["current_song"] = mediaFilename;
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
    m_ws->sweet_kill();
    m_ws->stop();

    PluginManager::INSTANCE.unregisterApis(m_ws);

    m_ws->unregister_resource("/fppd/ports");
    m_ws->unregister_resource("/fppd/testing");
    m_ws->unregister_resource("/fppd");
    m_ws->unregister_resource("/models");
    m_ws->unregister_resource("/overlays");
    m_ws->unregister_resource("/command");
    m_ws->unregister_resource("/commands");
    m_ws->unregister_resource("/commandPresets");
    m_ws->unregister_resource("/gpio");

    delete m_pr;
    delete m_ws;

    m_ws = NULL;
}

/*
 *
 */
void APIServer::Init(void) {
    m_params = create_webserver(FPP_HTTP_PORT).max_threads(10).use_dual_stack();

    m_ws = new webserver(m_params);

    m_pr = new PlayerResource;
    m_ws->register_resource("/fppd/ports", &OutputMonitor::INSTANCE, true);
    m_ws->register_resource("/fppd/testing", &ChannelTester::INSTANCE, true);
    m_ws->register_resource("/fppd", m_pr, true);
    m_ws->register_resource("/models", &PixelOverlayManager::INSTANCE, true);
    m_ws->register_resource("/overlays", &PixelOverlayManager::INSTANCE, true);
    m_ws->register_resource("/command", &CommandManager::INSTANCE, true);
    m_ws->register_resource("/commands", &CommandManager::INSTANCE, true);
    m_ws->register_resource("/commandPresets", &CommandManager::INSTANCE, true);
    m_ws->register_resource("/gpio", &GPIOManager::INSTANCE, true);
    m_ws->register_resource("/player", &Player::INSTANCE, true);

    PluginManager::INSTANCE.registerApis(m_ws);

    m_ws->start(false);
}

/*
 *
 */
void LogRequest(const http_request& req) {
    LogDebug(VB_HTTP, "API Req: %s%s\n", std::string(req.get_path()).c_str(),
             std::string(req.get_querystring()).c_str());
}

/*
 *
 */
void LogResponse(const http_request& req, int responseCode, const std::string& content) {
    if (WillLog(LOG_EXCESSIVE, VB_HTTP)) {
        LogExcess(VB_HTTP, "API Res: %s%s %d %s\n",
                  std::string(req.get_path()).c_str(),
                  std::string(req.get_querystring()).c_str(),
                  responseCode,
                  content.c_str());
    }
}

PlayerResource::PlayerResource() {
}

/*
 *
 */
HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> PlayerResource::render_GET(const http_request& req) {
    LogRequest(req);

    Json::Value result;
    std::string resultStr = "";
    std::string url(req.get_path());

    replaceStart(url, "/fppd/", "");

    LogDebug(VB_HTTP, "URL: %s %s\n", url.c_str(), std::string(req.get_querystring()).c_str());

    // Keep IF statement in alphabetical order
    if (url == "effects") {
        GetRunningEffects(result);
    } else if (url == "log") {
        GetLogSettings(result);
    } else if (url == "status") {
        GetCurrentStatus(result);
    } else if (url == "warnings") {
        std::list<std::string> warnings = WarningHolder::GetWarnings();
        result = Json::Value(Json::ValueType::arrayValue);
        for (auto& warn : warnings) {
            result.append(warn);
        }
    } else if (url == "e131stats") {
        GetE131BytesReceived(result);
    } else if (url == "multiSyncSystems") {
        bool localOnly = false;

        if (std::string(req.get_arg("localOnly")) == "1")
            localOnly = true;

        GetMultiSyncSystems(result, localOnly);
    } else if (url == "multiSyncStats") {
        bool reset = false;

        if (std::string(req.get_arg("reset")) == "1")
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
        if (std::string(req.get_arg("set")) != "") {
            int i = std::atoi(std::string(req.get_arg("set")).c_str());
            setVolume(i);
        }
        if (std::string(req.get_arg("simple")) == "true") {
            std::string v = std::to_string(getVolume());
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(v, 200, "text/plain"));
        }
        result["volume"] = getVolume();
        SetOKResult(result, "");
    } else if (url == "sequence") {
        LogDebug(VB_HTTP, "API - Getting list of running sequences\n");
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

    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, responseCode, "application/json"));
}

/*
 *
 */
HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> PlayerResource::render_POST(const http_request& req) {
    LogRequest(req);

    Json::Value data;
    Json::Value result;
    std::string url(req.get_path());

    replaceStart(url, "/fppd/", "");

    LogDebug(VB_HTTP, "POST URL: %s %s\n", url.c_str(), std::string(req.get_querystring()).c_str());

    if (req.get_content() != "") {
        if (!LoadJsonFromString(std::string(req.get_content()), data)) {
            LogErr(VB_CHANNELOUT, "Error parsing POST content\n");
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Error parsing POST content", 400));
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
            LogDebug(VB_HTTP, "API - Stopping all running playlists w/ content '%s'\n", std::string(req.get_content()).c_str());
        } else if (endsWith(url, "/start")) {
            // Start a playlist
            replaceEnd(url, "/start", "");
            LogDebug(VB_HTTP, "API - Starting playlist '%s' w/ content '%s'\n", url.c_str(), std::string(req.get_content()).c_str());
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
            LogDebug(VB_HTTP, "API - Stopping playlist '%s' w/ content '%s'\n", url.c_str(), std::string(req.get_content()).c_str());
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

    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr.c_str(), result["respCode"].asInt(), "application/json"));
}

/*
 *
 */
HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> PlayerResource::render_DELETE(const http_request& req) {
    LogRequest(req);

    Json::Value result;
    std::string url(req.get_path());

    replaceStart(url, "/fppd/", "");

    LogDebug(VB_HTTP, "DELETE URL: %s %s\n", url.c_str(), std::string(req.get_querystring()).c_str());

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

    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr.c_str(), result["respCode"].asInt()));
}

/*
 *
 */
HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> PlayerResource::render_PUT(const http_request& req) {
    LogRequest(req);

    Json::Value result;
    std::string url(req.get_path());

    replaceStart(url, "/fppd/", "");

    LogDebug(VB_HTTP, "PUT URL: %s %s\n", url.c_str(), std::string(req.get_querystring()).c_str());

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

    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr.c_str(), result["respCode"].asInt()));
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
