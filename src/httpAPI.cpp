/*
 *   HTTP API for the Falcon Player Daemon 
 *   Falcon Player project (FPP) 
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "channeloutput/channeloutput.h"
#include "channeloutput/channeloutputthread.h"
#include "common.h"
#include "e131bridge.h"
#include "fpp.h"
#include "fppd.h"
#include "fppversion.h"
#include "httpAPI.h"
#include "log.h"
#include "MultiSync.h"
#include "playlist/Playlist.h"
#include "Scheduler.h"
#include "settings.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

#include "stdlib.h"
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <jsoncpp/json/json.h>

#include "mediaoutput/mediaoutput.h"
#include "sensors/Sensors.h"
#include "commands/Commands.h"
#include "PixelOverlay.h"
#include "Plugins.h"


/*
 *
 */
APIServer::APIServer()
{
}

/*
 *
 */
APIServer::~APIServer()
{
	m_ws->sweet_kill();
	m_ws->stop();

    PluginManager::INSTANCE.unregisterApis(m_ws);

	m_ws->unregister_resource("/fppd");
    m_ws->unregister_resource("/models");
    m_ws->unregister_resource("/overlays");
    m_ws->unregister_resource("/command");
    m_ws->unregister_resource("/commands");

	delete m_pr;
	delete m_ws;

	m_ws = NULL;
}

/*
 *
 */
void APIServer::Init(void)
{
	m_params = create_webserver(FPP_HTTP_PORT).max_threads(5);

	m_ws = new webserver(m_params);

	m_pr = new PlayerResource;
	m_ws->register_resource("/fppd", m_pr, true);
    m_ws->register_resource("/models", &PixelOverlayManager::INSTANCE, true);
    m_ws->register_resource("/overlays", &PixelOverlayManager::INSTANCE, true);
    m_ws->register_resource("/command", &CommandManager::INSTANCE, true);
    m_ws->register_resource("/commands", &CommandManager::INSTANCE, true);

    PluginManager::INSTANCE.registerApis(m_ws);

	m_ws->start(false);
}

/*
 *
 */
void LogRequest(const http_request &req)
{
	LogDebug(VB_HTTP, "API Req: %s%s\n", req.get_path().c_str(),
		req.get_querystring().c_str());
}

/*
 *
 */
void LogResponse(const http_request &req, int responseCode, const std::string &content)
{
	if (LogLevelIsSet(LOG_EXCESSIVE))
	{
		LogDebug(VB_HTTP, "API Res: %s%s %d %s\n",
			req.get_path().c_str(),
			req.get_querystring().c_str(),
			responseCode,
			content.c_str());
	}
}

/*
 *
 */
const http_response PlayerResource::render_GET(const http_request &req)
{
	LogRequest(req);

	Json::Value result;
	std::string url = req.get_path();

	boost::replace_first(url, "/fppd/", "");

	LogDebug(VB_HTTP, "URL: %s %s\n", url.c_str(), req.get_querystring().c_str());

	// Keep IF statement in alphabetical order
    if (url == "effects")
	{
		GetRunningEffects(result);
	}
	else if (url == "events")
	{
		GetRunningEvents(result);
	}
	else if (url == "log")
	{
		GetLogSettings(result);
	}
	else if (url == "status")
	{
		GetCurrentStatus(result);
	}
	else if (url == "e131stats")
	{
		GetE131BytesReceived(result);
	}
	else if (url == "multiSyncSystems")
	{
		GetMultiSyncSystems(result);
	}
    else if (url == "playlists")
    {
        GetCurrentPlaylists(result);
    }
	else if (url == "playlist/filetime")
	{
		GetPlaylistFileTime(result);
	}
	else if (url == "playlist/config")
	{
		GetPlaylistConfig(result);
	}
	else if (url == "schedule")
	{
		LogDebug(VB_HTTP, "API - Getting current schedule information\n");
	}
	else if (url == "version")
	{
		result["version"] = getFPPVersion();
		result["branch"] = getFPPBranch();
		result["fppdAPI"] = FPPD_API_VERSION;

		SetOKResult(result, "");
	}
	else if (url == "volume")
	{
		result["volume"] = getVolume();
		SetOKResult(result, "");
	}
	else if (url == "sequence")
	{
		LogDebug(VB_HTTP, "API - Getting list of running sequences\n");
	}
	else if (url == "testing")
	{
		LogDebug(VB_HTTP, "API - Getting test mode status\n");
		result["config"] = JSONStringToObject(ChannelTester::INSTANCE.GetConfig().c_str());
		SetOKResult(result, "");
	}
	else
	{
		LogErr(VB_HTTP, "API - Error unknown GET request: %s\n", url.c_str());

		result["status"] = "ERROR";
		result["respCode"] = 404;
		result["message"] = std::string("endpoint fppd/") + url + " does not exist";
	}

    int responseCode = 200;
	if (result.empty()) {
		result["status"] = "ERROR";
		result["respCode"] = 400;
		result["message"] = "GET endpoint helper did not set result JSON";
    } else if (result.isMember("respCode")) {
        responseCode = result["respCode"].asInt();
    }

	Json::FastWriter fastWriter;
	std::string resultStr = fastWriter.write(result);
	LogResponse(req, responseCode, resultStr);

	return http_response_builder(resultStr, responseCode, "application/json");;
}

/*
 *
 */
const http_response PlayerResource::render_POST(const http_request &req)
{
	LogRequest(req);

	Json::Value data;
	Json::Reader reader;
	Json::Value result;
	std::string url = req.get_path();

	boost::replace_first(url, "/fppd/", "");

	LogDebug(VB_HTTP, "POST URL: %s %s\n", url.c_str(), req.get_querystring().c_str());

	if (req.get_content() != "")
	{
		bool success = reader.parse(req.get_content(), data);
		if (!success)
		{
			LogErr(VB_CHANNELOUT, "Error parsing POST content\n");
			return http_response_builder("Error parsing POST content", 400).string_response();
		}
	}

	// Keep IF statement in alphabetical order
    if (boost::starts_with(url, "effects/"))
	{
		boost::replace_first(url, "effects/", "");

		PostEffects(url, data, result);
	}
	else if (boost::starts_with(url, "events/"))
	{
		boost::replace_first(url, "events/", "");

		PostEvents(url, data, result);
	}
	else if (boost::starts_with(url, "falcon/hardware"))
	{
		PostFalconHardware(result);
	}
	else if (boost::starts_with(url, "gpio/ext"))
	{
		PostGPIOExt(data, result);
	}
	else if (boost::starts_with(url, "log/level/"))
	{
		boost::replace_first(url, "log/level/", "");

		SetLogLevel(url.c_str());
		SetOKResult(result, "Log Level set");
	}
	else if (boost::starts_with(url, "log/mask/"))
	{
		boost::replace_first(url, "log/mask/", "");

		SetLogMask(url.c_str());
		SetOKResult(result, "Log Mask set");
	}
	else if (url == "outputs")
	{
		PostOutputs(data, result);
	}
	else if (url == "outputs/remap")
	{
		PostOutputsRemap(data, result);
	}
	else if (url.find("playlists/") == 0)
	{
		boost::replace_first(url, "playlists/", "");

		if (url == "stop")
		{
			// Stop all running playlists
			LogDebug(VB_HTTP, "API - Stopping all running playlists w/ content '%s'\n", req.get_content().c_str());
		}
		else if (boost::ends_with(url, "/start"))
		{
			// Start a playlist
			boost::replace_last(url, "/start", "");
			LogDebug(VB_HTTP, "API - Starting playlist '%s' w/ content '%s'\n", url.c_str(), req.get_content().c_str());
		}
		else if (boost::ends_with(url, "/nextItem"))
		{
			boost::replace_last(url, "/nextItem", "");
			LogDebug(VB_HTTP, "API - Skipping to next entry in playlist '%s'\n", url.c_str());
		}
		else if (boost::ends_with(url, "/restartItem"))
		{
			boost::replace_last(url, "/restartItem", "");
			LogDebug(VB_HTTP, "API - Restarting current item in playlist '%s'\n", url.c_str());
		}
		else if (boost::ends_with(url, "/prevItem"))
		{
			boost::replace_last(url, "/prevItem", "");
			LogDebug(VB_HTTP, "API - Skipping to prev entry in playlist '%s'\n", url.c_str());
		}
		else if (url.find("/section/") != std::string::npos)
		{
			std::string playlistName = url.substr(0, url.find("/section/"));
			std::string section = url.substr(url.rfind("/section/") + 6);
			LogDebug(VB_HTTP, "API - Jumping to section '%s' in playlist '%s'\n", section.c_str(), url.c_str());
		}
		else if (url.find("/item/") != std::string::npos)
		{
			std::string playlistName = url.substr(0, url.find("/item/"));
			int item = atoi(url.substr(url.rfind("/item/") + 6).c_str());
			LogDebug(VB_HTTP, "API - Jumping to item %d in playlist '%s'\n", item, playlistName.c_str());
		}
		else if (boost::ends_with(url, "/stop"))
		{
			boost::replace_last(url, "/stop", "");
			LogDebug(VB_HTTP, "API - Stopping playlist '%s' w/ content '%s'\n", url.c_str(), req.get_content().c_str());
		}
	}
	else if (url == "schedule")
	{
		PostSchedule(data, result);
	}
	else if (url.find("volume/") == 0)
	{
		int volume = atoi(url.substr(7).c_str());
		setVolume(volume);
		result["volume"] = volume;
		SetOKResult(result, "Volume set");
	}
	else if (url.find("sequences/") == 0)
	{
		boost::replace_first(url, "sequences/", "");

		if (boost::ends_with(url, "/start"))
		{
			boost::replace_last(url, "/start", "");
			LogDebug(VB_HTTP, "API - Starting sequence '%s'\n", url.c_str());
		}
		else if (boost::ends_with(url, "/stop"))
		{
			boost::replace_last(url, "/stop", "");
			LogDebug(VB_HTTP, "API - Stopping sequence '%s'\n", url.c_str());
		}
		else if (boost::ends_with(url, "/pause"))
		{
			boost::replace_last(url, "/pause", "");
			LogDebug(VB_HTTP, "API - (un)Pausing sequence '%s'\n", url.c_str());
		}
		else if (boost::ends_with(url, "/pause/0"))
		{
			LogDebug(VB_HTTP, "API - UnPausing sequence '%s'\n", url.c_str());
		}
		else if (boost::ends_with(url, "/pause/1"))
		{
			LogDebug(VB_HTTP, "API - Pausing sequence '%s'\n", url.c_str());
		}
		else if (boost::ends_with(url, "/step"))
		{
			boost::replace_last(url, "/step", "");
			LogDebug(VB_HTTP, "API - Stepping sequence '%s' by 1 frame\n", url.c_str());
		}
		else if (boost::ends_with(url, "/back"))
		{
			boost::replace_last(url, "/back", "");
			LogDebug(VB_HTTP, "API - Stepping sequence '%s' BACK by 1 frame\n", url.c_str());
		}
		else if (url.find("/step/") != std::string::npos)
		{
			std::string sequenceName = url.substr(0, url.find("/step/"));
			int frames = atoi(url.substr(url.find("/step/") + 6).c_str());
			LogDebug(VB_HTTP, "API - Stepping sequence '%s' by %d frame(s)\n", sequenceName.c_str(), frames);
		}
		else if (url.find("/back/") != std::string::npos)
		{
			std::string sequenceName = url.substr(0, url.find("/back/"));
			int frames = atoi(url.substr(url.find("/back/") + 6).c_str());
			LogDebug(VB_HTTP, "API - Stepping sequence '%s' BACK by %d frame(s)\n", sequenceName.c_str(), frames);
		}
	}
	else if (url == "testing")
	{
		PostTesting(data, result);
	}
	else if (url == "settings/reload")
	{
		LogDebug(VB_HTTP, "API - Reloading all settings\n");
	}
	else if (url.find("settings/reload/") == 0)
	{
		boost::replace_first(url, "settings/reload/", "");
		LogDebug(VB_HTTP, "API - Reloading setting: %s\n", url.c_str());
	}
	else if (url == "restart")
	{
		LogDebug(VB_HTTP, "API - Restarting fppd daemon\n");
	}
	else if (url == "shutdown")
	{
		SetOKResult(result, "Shutting down fppd");
		ShutdownFPPD();
	}
	else
	{
		LogErr(VB_HTTP, "API - Error unknown POST request: %s\n", url.c_str());

		result["status"] = "ERROR";
		result["respCode"] = 404;
		result["message"] = std::string("endpoint fppd/") + url + " does not exist";
	}


	if (!result.isMember("status"))
	{
		result["status"] = "ERROR";
		result["respCode"] = 400;
		result["message"] = "POST endpoint helper did not set result JSON";
	}

	Json::FastWriter fastWriter;
	std::string resultStr = fastWriter.write(result);
	LogResponse(req, result["respCode"].asInt(), resultStr);

    return http_response_builder(resultStr.c_str(), result["respCode"].asInt(), "application/json").string_response();
}


/*
 *
 */
const http_response PlayerResource::render_DELETE(const http_request &req)
{
	LogRequest(req);

	Json::Value result;
	std::string url = req.get_path();

	boost::replace_first(url, "/fppd/", "");

	LogDebug(VB_HTTP, "DELETE URL: %s %s\n", url.c_str(), req.get_querystring().c_str());

	// Keep IF statement in alphabetical order
	if (url.find("event/") == 0)
	{
		boost::replace_first(url, "event/", "");
		int id = atoi(url.c_str());
		LogDebug(VB_HTTP, "API - Deleting event with running ID %d\n", id);
	}
	else
	{
		LogErr(VB_HTTP, "API - Error unknown DELETE request: %s\n", url.c_str());

		result["status"] = "ERROR";
		result["respCode"] = 404;
		result["message"] = std::string("endpoint fppd/") + url + " does not exist";
	}


	if (!result.isMember("status"))
	{
		result["status"] = "ERROR";
		result["respCode"] = 400;
		result["message"] = "DELETE endpoint helper did not set result JSON";
	}

	Json::FastWriter fastWriter;
	std::string resultStr = fastWriter.write(result);

	LogResponse(req, result["respCode"].asInt(), resultStr);

	return http_response_builder(resultStr.c_str(), result["respCode"].asInt()).string_response();
}


/*
 *
 */
const http_response PlayerResource::render_PUT(const http_request &req)
{
	LogRequest(req);

	Json::Value result;
	std::string url = req.get_path();

	boost::replace_first(url, "/fppd/", "");

	LogDebug(VB_HTTP, "PUT URL: %s %s\n", url.c_str(), req.get_querystring().c_str());

	// Keep IF statement in alphabetical order
	if (url.find("playlists/") == 0)
	{
		boost::replace_first(url, "playlists/", "");

		if (boost::ends_with(url, "/settings"))
		{
			boost::replace_last(url, "/settings", "");

			LogDebug(VB_HTTP, "API - Updating runtime settings for playlist '%s'\n", url.c_str());
		}
	}
	else
	{
		LogErr(VB_HTTP, "API - Error unknown PUT request: %s\n", url.c_str());

		result["status"] = "ERROR";
		result["respCode"] = 404;
		result["message"] = std::string("endpoint fppd/") + url + " does not exist";
	}



	if (!result.isMember("status"))
	{
		result["status"] = "ERROR";
		result["respCode"] = 400;
		result["message"] = "PUT endpoint helper did not set result JSON";
	}

	Json::FastWriter fastWriter;
	std::string resultStr = fastWriter.write(result);

	LogResponse(req, result["respCode"].asInt(), resultStr);

	return http_response_builder(resultStr.c_str(), result["respCode"].asInt()).string_response();;
}

/*
 *
 */
void PlayerResource::SetOKResult(Json::Value &result, const std::string &msg)
{
	result["status"] = "OK";
	result["respCode"] = 200;
	result["message"] = msg;
}

/*
 *
 */
void PlayerResource::SetErrorResult(Json::Value &result, const int respCode, const std::string &msg)
{
	result["status"] = "ERROR";
	result["respCode"] = respCode;
	result["message"] = msg;
}

/*
 *
 */
void PlayerResource::GetRunningEffects(Json::Value &result)
{
	LogDebug(VB_HTTP, "API - Getting list of running effects\n");
}

/*
 *
 */
void PlayerResource::GetRunningEvents(Json::Value &result)
{
	LogDebug(VB_HTTP, "API - Getting list of running events\n");
}

/*
 *
 */
void PlayerResource::GetLogSettings(Json::Value &result)
{
	SetOKResult(result, "");
	Json::Value log;

	log["level"] = logLevelStr;
	log["mask"] = logMaskStr;
	result["log"] = log;
}

inline std::string toStdStringAndFree(char * v) {
    std::string s = v;
    free(v);
    return s;
}
inline std::string secondsToTime(int i) {
    std::stringstream sstr;
    int min = i / 60;
    int sec = i % 60;
    sstr << std::setw(2) << std::setfill('0') << min;
    sstr << ":";
    sstr << std::setw(2) << std::setfill('0') << sec;
    return sstr.str();
}
/*
 *
 */
void PlayerResource::GetCurrentStatus(Json::Value &result)
{
	LogDebug(VB_HTTP, "API - Getting fppd status\n");

    int mode = getFPPmode();
    result["fppd"] = "running";
    result["mode"] = mode;
    result["mode_name"] = toStdStringAndFree(modeToString(getFPPmode()));
    result["status"] = FPPstatus;
    result["status_name"] = ChannelTester::INSTANCE.Testing() ? "testing" : (FPPstatus == 0 ? "idle" : (FPPstatus == 1 ? "playing" : "stopping gracefully"));
    result["volume"] = getVolume();

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream sstr;
    sstr << std::put_time(&tm, "%a %b %d %H:%M:%S %Z %Y");
    std::string str = sstr.str();
    result["time"] = str;
    
    Sensors::INSTANCE.reportSensors(result);
    if (mode == 1) {
        //bridge mode only returns the base information
        return;
    }

    // base data returned by all the other modes
    result["current_playlist"]["playlist"] = "";
    result["current_playlist"]["type"] = "";
    result["current_playlist"]["index"] = "0";
    result["current_playlist"]["count"] = "0";
    result["current_sequence"] = "";
    result["current_song"] = "";
    result["seconds_played"] = "0";
    result["seconds_remaining"] = "0";
    result["time_elapsed"] = "00:00";
    result["time_remaining"] = "00:00";

    char NextPlaylist[128] = "No playlist scheduled.";
    char NextScheduleStartText[64] = "";
    scheduler->GetNextScheduleStartText(NextScheduleStartText);
    scheduler->GetNextPlaylistText(NextPlaylist);
    
    if (getFPPmode() == REMOTE_MODE) {
        int secsElapsed = 0;
        int secsRemaining = 0;
        std::string seqFilename;
        std::string mediaFilename;
        if (sequence->IsSequenceRunning()) {
            seqFilename = sequence->m_seqFilename;
            secsElapsed = sequence->m_seqSecondsElapsed;
            secsRemaining = sequence->m_seqSecondsRemaining;
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
    } else if (FPPstatus == FPP_STATUS_IDLE) {
        result["next_playlist"]["playlist"] = NextPlaylist;
        result["next_playlist"]["start_time"] = NextScheduleStartText;
        result["repeat_mode"] = "0";
    } else {
        Json::Value pl = playlist->GetInfo();
        if (pl["currentEntry"].isMember("dynamic")) {
            pl["currentEntry"] = pl["currentEntry"]["dynamic"];
        }
        result["repeat_mode"] = pl["repeat"].asInt();
        result["next_playlist"]["playlist"] = NextPlaylist;
        result["next_playlist"]["start_time"] = NextScheduleStartText;

        std::string plname = pl["name"].asString();
        plname = plname.substr(plname.find_last_of("\\/") + 1);
        plname = plname.substr(0, plname.find_last_of("."));
        result["current_playlist"]["playlist"] = plname;
        result["current_playlist"]["index"] = std::to_string(playlist->GetPosition());
        result["current_playlist"]["count"] = std::to_string(pl["size"].asInt());
        result["current_playlist"]["type"] = pl["currentEntry"]["type"].asString();

        int secsElapsed = 0;
        int secsRemaining = 0;
  
        std::string currentSeq;
        std::string currentSong;

        if ((pl["currentEntry"]["type"] == "both") ||
            (pl["currentEntry"]["type"] == "media")) {
            
            currentSeq = pl["currentEntry"]["type"].asString() == "both"
                ? pl["currentEntry"]["sequence"]["sequenceName"].asString() : "";
            currentSong = pl["currentEntry"]["type"].asString() == "both"
                ? pl["currentEntry"]["media"]["mediaFilename"].asString()
                : pl["currentEntry"]["mediaFilename"].asString();

            secsElapsed = pl["currentEntry"]["type"].asString() == "both"
                ? pl["currentEntry"]["media"]["secondsElapsed"].asInt()
                : pl["currentEntry"]["secondsElapsed"].asInt();
            secsRemaining = pl["currentEntry"]["type"].asString() == "both"
                ? pl["currentEntry"]["media"]["secondsRemaining"].asInt()
                : pl["currentEntry"]["secondsRemaining"].asInt();
        } else if (pl["currentEntry"]["type"] == "sequence") {
            currentSeq = pl["currentEntry"]["sequenceName"].asString();
            secsElapsed = sequence->m_seqSecondsElapsed;
            secsRemaining = sequence->m_seqSecondsRemaining;
        } else if (pl["currentEntry"]["type"] == "script") {
            currentSeq = pl["currentEntry"]["scriptFilename"].asString();
            secsElapsed = pl["currentEntry"]["secondsElapsed"].asInt();
        } else {
            secsElapsed = pl["currentEntry"]["type"].asString() == "pause" ? pl["currentEntry"]["duration"].asInt() - pl["currentEntry"]["remaining"].asInt() : 0;
            secsRemaining = pl["currentEntry"]["type"].asString() == "pause" ? pl["currentEntry"]["remaining"].asInt() : 0;
        }
        result["current_sequence"] = currentSeq;
        result["current_song"] = currentSong;
        result["seconds_played"] = std::to_string(secsElapsed);
        result["seconds_elapsed"] = std::to_string(secsElapsed);
        result["seconds_remaining"] = std::to_string(secsRemaining);
        result["time_elapsed"] = secondsToTime(secsElapsed);
        result["time_remaining"] = secondsToTime(secsRemaining);
    }
}

/*
 *
 */
void PlayerResource::GetCurrentPlaylists(Json::Value &result)
{
	LogDebug(VB_HTTP, "API - Getting current playlist\n");

	Json::Value names(Json::arrayValue);

	if (playlist->IsPlaying())
		names.append(playlist->GetPlaylistName());

	result["playlists"] = names;

	SetOKResult(result, "");
}

/*
 *
 */
void PlayerResource::GetE131BytesReceived(Json::Value &result)
{
	result = GetE131UniverseBytesReceived(); // e131bridge.cpp

	if (result.isMember("universes"))
		SetOKResult(result, "");
	else
		SetErrorResult(result, 400, "GetE131UniverseBytesReceived() did not return any universes.");
}

/*
 *
 */
void PlayerResource::GetMultiSyncSystems(Json::Value &result)
{
	result = multiSync->GetSystems();

	if (result.isMember("systems"))
		SetOKResult(result, "");
	else
		SetErrorResult(result, 400, "MultiSync did not return any systems.");
}

/*
 *
 */
void PlayerResource::GetPlaylistFileTime(Json::Value &result)
{
	if (playlist->IsPlaying())
	{
		uint64_t fileTime = playlist->GetFileTime();

		result["fileTime"] = (Json::UInt64)fileTime;
	}
	else
	{
		result["fileTime"] = 0;
	}

	SetOKResult(result, "");
}

/*
 *
 */
void PlayerResource::GetPlaylistConfig(Json::Value &result)
{
	if (playlist->IsPlaying())
		result = playlist->GetConfig();

	SetOKResult(result, "");
}

/*
 *
 */
void PlayerResource::PostEffects(const std::string &effectName, const Json::Value &data, Json::Value &result)
{
	if (!data.isMember("command"))
	{
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
void PlayerResource::PostEvents(const std::string &eventID, const Json::Value &data, Json::Value &result)
{
	if (!data.isMember("command"))
	{
		SetErrorResult(result, 400, "'command' field not specified");
		return;
	}

	std::string command = data["command"].asString();

	LogDebug(VB_HTTP, "API - PostEvent(%s) cmd: %s\n", eventID.c_str(), command.c_str());

	SetOKResult(result, "PostEvent result would go here");
}

/*
 *
 */
void PlayerResource::PostFalconHardware(Json::Value &result)
{
	LogDebug(VB_HTTP, "API - Reloading hardware config\n");

	SetOKResult(result, "PostFalconHardware result would go here");
}

/*
 *
 */
void PlayerResource::PostGPIOExt(const Json::Value &data, Json::Value &result)
{
	if (!data.isMember("config"))
	{
		SetErrorResult(result, 400, "'config' field not specified");
		return;
	}

	LogDebug(VB_HTTP, "API - Doing something with gpio\n");
}

/*
 *
 */
void PlayerResource::PostOutputs(const Json::Value &data, Json::Value &result)
{
	if (!data.isMember("command"))
	{
		SetErrorResult(result, 400, "'command' field not specified");
		return;
	}

	if (data["command"].asString() == "enable")
	{
		EnableChannelOutput();
		SetOKResult(result, "channel output enabled");
	}
	else if (data["command"].asString() == "disable")
	{
		DisableChannelOutput();
		SetOKResult(result, "channel output disabled");
	}
}

/*
 *
 */
void PlayerResource::PostOutputsRemap(const Json::Value &data, Json::Value &result)
{
	if (!data.isMember("command"))
	{
		SetErrorResult(result, 400, "'command' field not specified");
		return;
	}

	if (data["command"].asString() == "reload")
	{
		// FIXME, need to fix this function to lock the remap array
		//LoadChannelRemapData();
		SetOKResult(result, "channel remaps reloaded");
	}
}

/*
 *
 */
void PlayerResource::PostSchedule(const Json::Value data, Json::Value &result)
{
	if (!data.isMember("command"))
	{
		SetErrorResult(result, 400, "'command' field not specified");
		return;
	}

	if (data["command"].asString() == "reload")
	{
		if(FPPstatus==FPP_STATUS_IDLE)
			scheduler->ReLoadCurrentScheduleInfo();

		scheduler->ReLoadNextScheduleInfo();

		SetOKResult(result, "Schedule reload triggered");
	}
}

/*
 *
 */
void PlayerResource::PostTesting(const Json::Value data, Json::Value &result)
{
	Json::FastWriter fastWriter;
	std::string config = fastWriter.write(data);

	if (ChannelTester::INSTANCE.SetupTest(config))
	{
		SetOKResult(result, "Test Mode Activated");
	}
	else
	{
		SetOKResult(result, "Test Mode Deactivated");
	}
}

