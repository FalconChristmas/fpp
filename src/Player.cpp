/*
 *   Player class for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2020 the Falcon Player Developers
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

#include "Player.h"
#include "common.h"
#include "log.h"

Player Player::INSTANCE;

Player::Player() :
    lastCheckTime(std::time(nullptr)),
    origStartTime(0),
    origStopTime(0),
    startTime(0),
    stopTime(0),
    stopMethod(0),
    forceStopped(false) {
}

Player::~Player() {
    if (playlist)
        delete playlist;
}

void Player::Init() {
    playlist = new Playlist();
}

int Player::StartPlaylist(const std::string& name, const int repeat,
                          const int startPosition, const int endPosition, const int manualPriority) {

    if ((GetStatus() == FPP_STATUS_IDLE) ||
        (name != playlistName)) {
        playlistName = name;
        startTime = std::time(nullptr);
        stopTime = 0;
        origStopTime = 0;
        stopMethod = 0;
        priority = manualPriority;
    }

    forceStopped = false;
    forceStoppedPlaylist = "";

    LogDebug(VB_PLAYLIST, "Manually starting %srepeating playlist '%s'\n",
             (repeat == -1 ? playlist->GetRepeat() : repeat) ? "" : "non-",
             playlistName.c_str());

    return playlist->Play(playlistName.c_str(), startPosition, repeat, -1, endPosition);
}

int Player::StartScheduledPlaylist(const std::string& name, const int position,
                                   const int repeat, const int scheduleEntry, const int scheduledPriority,
                                   const time_t sTime, const time_t eTime, const int method) {
    playlistName = name;
    origStartTime = sTime;
    origStopTime = eTime;
    startTime = std::time(nullptr);
    stopTime = eTime;
    stopMethod = method;
    priority = scheduledPriority;
    forceStopped = false;
    forceStoppedPlaylist = "";

    LogDebug(VB_PLAYLIST, "Starting %srepeating playlist '%s' with scheduled '%s' in %d seconds\n",
             repeat ? "" : "non-",
             playlistName.c_str(),
             stopMethod == 0 ? "Graceful Stop" : stopMethod == 1 ? "Hard Stop" : stopMethod == 2 ? "Graceful Stop After Loop" : "",
             (int)(stopTime - std::time(nullptr)));

    return playlist->Play(playlistName.c_str(), position, repeat, scheduleEntry);
}

int Player::AdjustPlaylistStopTime(const int seconds) {
    if ((GetStatus() != FPP_STATUS_PLAYLIST_PLAYING) &&
        (GetStatus() != FPP_STATUS_STOPPING_GRACEFULLY) &&
        (GetStatus() != FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)) {
        LogInfo(VB_SCHEDULE, "Tried to extend a running playlist, but there is no playlist running.\n");
        return 0;
    }

    if (!WasScheduled()) {
        LogInfo(VB_SCHEDULE, "Tried to extend running playlist, but it was manually started.\n");
        return 0;
    }

    // UI should catch this, but also check here
    if ((seconds > (12 * 60 * 60)) ||
        (seconds < (-3 * 60 * 60))) {
        return 0;
    }

    if (seconds >= 0) {
        LogDebug(VB_PLAYLIST, "Extending scheduled playlist '%s' by %d seconds\n",
                 playlistName.c_str(), seconds);
    } else {
        LogDebug(VB_PLAYLIST, "Shortening scheduled playlist '%s' by %d seconds\n",
                 playlistName.c_str(), seconds);
    }

    stopTime += seconds;

    LogDebug(VB_PLAYLIST, "New end time is now: %s", ctime(&stopTime));

    return 1;
}

void Player::InsertPlaylistAsNext(const std::string& filename, const int startPosition, const int endPos) {
    playlist->InsertPlaylistAsNext(filename, startPosition, endPos);
}

void Player::InsertPlaylistImmediate(const std::string& filename, const int startPosition, const int endPos) {
    playlist->InsertPlaylistImmediate(filename, startPosition, endPos);
}

int Player::StopNow(int forceStop) {
    forceStopped = (bool)forceStop;
    if (forceStopped)
        forceStoppedPlaylist = playlistName;
    else
        forceStoppedPlaylist = "";

    return playlist->StopNow(forceStop);
}

int Player::StopGracefully(int forceStop, int afterCurrentLoop) {
    forceStopped = (bool)forceStop;
    if (forceStopped)
        forceStoppedPlaylist = playlistName;
    else
        forceStoppedPlaylist = "";

    return playlist->StopGracefully(forceStop, afterCurrentLoop);
}

int Player::Process() {
    std::time_t procTime = std::time(nullptr);

    // See if we need to stop a scheduled playlist
    if ((playlist->getPlaylistStatus() == FPP_STATUS_PLAYLIST_PLAYING) &&
        (stopTime) &&
        (lastCheckTime != procTime)) {
        lastCheckTime = procTime;

        if (stopTime > procTime) {
            bool logSwitch = false;
            int diff = stopTime - procTime;

            // Print the countdown more frequently as we get closer
            if (((diff > 300) && ((diff % 300) == 0)) ||
                ((diff > 60) && (diff <= 300) && ((diff % 60) == 0)) ||
                ((diff > 10) && (diff <= 60) && ((diff % 10) == 0)) ||
                ((diff <= 10))) {
                logSwitch = true;
            }

            if (logSwitch) {
                LogDebug(VB_PLAYLIST, "Playlist '%s' will switch to '%s' in %d second%s\n",
                         playlistName.c_str(),
                         stopMethod == 0 ? "Stopping Gracefully" : stopMethod == 1 ? "Hard Stop" : stopMethod == 2 ? "Stopping Gracefully After Loop" : "",
                         diff,
                         diff == 1 ? "" : "s");
            }
        }

        if (stopTime <= procTime) {
            // if user shortened the scheduled end time mark as force stopped
            int forceStop = (stopTime < origStopTime) ? 1 : 0;

            switch (stopMethod) {
            case 0: // Gracefully
                StopGracefully(forceStop);
                break;
            case 2: // After Loop
                StopGracefully(forceStop, 1);
                break;
            case 1: // Hard Stop
            default:
                while (GetStatus() != FPP_STATUS_IDLE)
                    StopNow(forceStop);
                break;
            }
        }
    }

    return playlist->Process();
}

void Player::ProcessMedia() {
    playlist->ProcessMedia();
}

int Player::IsPlaying() {
    return playlist->IsPlaying();
}

std::string Player::GetPlaylistName() {
    return playlist->GetPlaylistName();
}

PlaylistStatus Player::GetStatus() {
    return playlist->getPlaylistStatus();
}

int Player::GetRepeat() {
    return playlist->GetRepeat();
}

int Player::GetStopMethod() {
    return stopMethod;
}

int Player::GetPosition() {
    return playlist->GetPosition();
}

Json::Value Player::GetInfo(void) {
    return playlist->GetInfo();
}

int Player::GetScheduleEntry() {
    return playlist->GetScheduleEntry();
}

uint64_t Player::GetFileTime() {
    return playlist->GetFileTime();
}

Json::Value Player::GetConfig() {
    return playlist->GetConfig();
}

Json::Value Player::GetMqttStatusJSON() {
    return playlist->GetMqttStatusJSON();
}

int Player::WasScheduled() {
    return playlist->WasScheduled();
}

int Player::FindPosForMS(uint64_t& ms) {
    return playlist->FindPosForMS(ms);
}

void Player::GetFilenamesForPos(int pos, std::string& seq, std::string& med) {
    playlist->GetFilenamesForPos(pos, seq, med);
}

int Player::Load(const std::string filename) {
    return playlist->Load(filename.c_str());
}

int Player::Start() {
    return playlist->Start();
}

void Player::RestartItem() {
    playlist->RestartItem();
}

void Player::NextItem() {
    playlist->NextItem();
}

void Player::PrevItem() {
    playlist->PrevItem();
}

void Player::Pause() {
    playlist->Pause();
}

void Player::Resume() {
    playlist->Resume();
}

int Player::Cleanup(void) {
    return playlist->Cleanup();
}

Json::Value Player::GetStatusJSON() {
    Json::Value result;
    Json::Value playlists(Json::arrayValue);
    Json::Value pl;

    pl = playlist->GetInfo();
    pl["details"] = playlist->GetConfig();
    pl["status"] = (int)playlist->getPlaylistStatus();
    pl["scheduled"] = playlist->WasScheduled() ? true : false;
    pl["position"] = playlist->GetPosition();
    pl["lastModified"] = (Json::UInt64)playlist->GetFileTime();

    // things we store locally that would need to be in an array if we can
    // play multiple playlists concurrently
    pl["origStartTime"] = (Json::UInt64)origStartTime;
    pl["origStopTime"] = (Json::UInt64)origStopTime;
    pl["startTime"] = (Json::UInt64)startTime;
    pl["stopTime"] = (Json::UInt64)stopTime;
    pl["stopMethod"] = stopMethod;
    pl["priority"] = priority;

    playlists.append(pl);

    result["playlists"] = playlists;

    return result;
}

const std::shared_ptr<httpserver::http_response> Player::render_GET(const httpserver::http_request& req) {
    int plen = req.get_path_pieces().size();
    std::string p1 = req.get_path_pieces()[0];

    Json::Value result;

    if ((plen == 1) || ((plen == 2) && (req.get_path_pieces()[1] == "status"))) {
        result = GetStatusJSON();
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(SaveJsonToString(result, "  "), 200, "application/json"));
    }

    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
}

const std::shared_ptr<httpserver::http_response> Player::render_POST(const httpserver::http_request& req) {
    int plen = req.get_path_pieces().size();
    std::string p1 = req.get_path_pieces()[0];

    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
}

const std::shared_ptr<httpserver::http_response> Player::render_PUT(const httpserver::http_request& req) {
    int plen = req.get_path_pieces().size();
    std::string p1 = req.get_path_pieces()[0];

    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
}
