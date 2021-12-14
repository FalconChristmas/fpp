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

#include <jsoncpp/json/json.h>
#include <ctime>
#include <httpserver.hpp>
#include <map>

#include "playlist/Playlist.h"

class Player : public httpserver::http_resource {
public:
    Player();
    ~Player();

    void Init();

    int StartPlaylist(const std::string& name, const int repeat = -1,
                      const int startPosition = -1, const int endPosition = -1,
                      const int manualPriority = 1);
    int StartScheduledPlaylist(const std::string& name, const int position,
                               const int repeat, const int scheduleEntry, const int scheduledPriority,
                               const time_t sTime, const time_t eTime, const int method);

    int AdjustPlaylistStopTime(const int seconds = 300);

    // wrappers for Playlist class methods we'll just call directly for now
    void InsertPlaylistAsNext(const std::string& filename, const int startPosition = -1, const int endPos = -1);
    void InsertPlaylistImmediate(const std::string& filename, const int startPosition = -1, const int endPos = -1);

    int StopNow(int forceStop = 0);
    int StopGracefully(int forceStop = 0, int afterCurrentLoop = 0);

    int Process();
    void ProcessMedia();
    int IsPlaying();

    std::string GetPlaylistName();
    PlaylistStatus GetStatus();
    int GetRepeat();
    std::time_t GetOrigStartTime() { return origStartTime; }
    std::time_t GetOrigStopTime() { return origStopTime; }
    std::time_t GetStartTime() { return startTime; }
    std::time_t GetStopTime() { return stopTime; }
    int GetStopMethod();
    int GetPosition();
    Json::Value GetInfo();
    void ClearForceStopped() { forceStopped = false; }
    bool GetForceStopped() { return forceStopped; }
    std::string GetForceStoppedPlaylist() { return forceStoppedPlaylist; }
    int GetPriority() { return priority; }
    int GetScheduleEntry();
    uint64_t GetFileTime();
    Json::Value GetConfig();
    Json::Value GetMqttStatusJSON();
    int WasScheduled();

    int FindPosForMS(uint64_t& ms); //ms will be updated with how far into Pos it would be
    void GetFilenamesForPos(int pos, std::string& seq, std::string& med);

    int Load(const std::string filename);
    int Start();

    void RestartItem();
    void NextItem();
    void PrevItem();

    void Pause();
    void Resume();

    int Cleanup();

    Json::Value GetStatusJSON();

    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request& req) override;
    virtual const std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request& req) override;
    virtual const std::shared_ptr<httpserver::http_response> render_PUT(const httpserver::http_request& req) override;

    static Player INSTANCE;

private:
    std::string playlistName;

    std::time_t lastCheckTime;
    std::time_t origStartTime;
    std::time_t origStopTime;
    std::time_t startTime;
    std::time_t stopTime;
    int stopMethod;
    int priority;

    volatile bool forceStopped;
    std::string forceStoppedPlaylist;
};
