#pragma once
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

    int FindPosForMS(uint64_t& ms, bool itemDefinedOnly); //ms will be updated with how far into Pos it would be
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
    void GetCurrentStatus(Json::Value& result);

    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request& req) override;
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request& req) override;
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_PUT(const httpserver::http_request& req) override;

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
