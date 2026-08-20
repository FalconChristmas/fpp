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
#include "fpp-json-fwd.h"
#include "fpphttp_types.h"
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "AtomicSharedPtr.h"
#include "playlist/Playlist.h"

class Player {
public:
    Player();
    ~Player();

    void Init();

    int StartPlaylist(const std::string& name, const int repeat = -1,
                      const int startPosition = -1, const int endPosition = -1,
                      const int manualPriority = 1000, const bool allowScheduleOverride = true);
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
    bool GetAllowScheduleOverride() { return allowScheduleOverride; }
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

    virtual HttpResponsePtr render_GET(const HttpRequestPtr& req);
    virtual HttpResponsePtr render_POST(const HttpRequestPtr& req);
    virtual HttpResponsePtr render_PUT(const HttpRequestPtr& req);

    static Player INSTANCE;

    // ────────────────────────────────────────────────────────────────────
    // Ownership of the current playlist.
    //
    // The player owns every Playlist that can become "the one playing".  A
    // caller on any thread takes a snapshot and calls through it; holding the
    // shared_ptr keeps that instance alive for the whole call even if the main
    // loop retires it mid-flight.  That is what closes the cross-thread
    // use-after-free: a status poll from a drogon thread can no longer be
    // reading an instance the main loop just freed.
    //
    // Take ONE snapshot per call, not one per dereference — two snapshots in
    // one method can straddle a switch and report a torn mix of two playlists.
    // ────────────────────────────────────────────────────────────────────
    std::shared_ptr<Playlist> PlaylistSnapshot() const { return m_playlist.load(); }

    // The rest of this API is main-thread only; it is the lifecycle half that
    // Playlist.cpp's own machinery (PL_CLEANUPS, the m_parent chain,
    // SwitchToInsertedPlaylist) drives.  Raw Playlist* keeps flowing through
    // that machinery unchanged; these calls are where it meets ownership.

    // Construct a Playlist and take ownership of it.  Returns the reference so
    // the caller can use it before publishing it as current.
    std::shared_ptr<Playlist> CreatePlaylist(Playlist* parent);

    // Publish an already-owned instance as the current playlist.
    void SetCurrentPlaylist(Playlist* p);

    // Retire an instance: drop the player's owning reference.  The object
    // itself survives until the last outstanding snapshot releases it.
    void ReleasePlaylist(Playlist* p);

    // Run the destructors deferred by the shared_ptr deleter.  MAIN THREAD
    // ONLY — ~Playlist parks its entries on an unsynchronized list that only
    // the main thread drains, so the deleter never destroys inline.
    void DrainRetiredPlaylists();

private:
    // Every live instance, keyed by nothing but its own identity: the current
    // playlist plus every ancestor still on the m_parent chain.  A parent is
    // reachable only by raw pointer, so this is what keeps it alive.
    AtomicSharedPtr<Playlist> m_playlist;
    std::mutex m_ownedPlaylistsLock;
    std::vector<std::shared_ptr<Playlist>> m_ownedPlaylists;

    std::string playlistName;

    std::time_t lastCheckTime;
    std::time_t origStartTime;
    std::time_t origStopTime;
    std::time_t startTime;
    std::time_t stopTime;
    int stopMethod;
    int priority;
    bool allowScheduleOverride;

    volatile bool forceStopped;
    std::string forceStoppedPlaylist;
};
