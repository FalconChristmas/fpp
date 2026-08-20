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

#include "fpp-json.h"
#include "fpphttp.h" // drogon/HTTP helpers used here; no longer pulled transitively (see fpphttp_types.h)

#include <cstdlib>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

#include "Events.h"
#include "common.h"
#include "log.h"
#include "commands/Commands.h"

#include "Player.h"

// Playlists retired by the main loop are destroyed HERE, on the main thread,
// never by whichever thread happened to drop the last snapshot: ~Playlist runs
// Cleanup(), which parks every entry on Playlist.cpp's PL_ENTRY_CLEANUPS — an
// unsynchronized list that only the main thread drains.  So the shared_ptr
// deleter does not delete; it parks the raw pointer here, and
// Playlist::Process() calls DrainRetiredPlaylists() at exactly the point in the
// frame where the old drain ran `delete p`.
//
// Declared ahead of Player::INSTANCE so it is constructed first and therefore
// destroyed last — ~Player drains it during static destruction.
static std::mutex s_retiredPlaylistLock;
static std::vector<Playlist*> s_retiredPlaylists;

static void RetirePlaylist(Playlist* p) {
    std::lock_guard<std::mutex> lk(s_retiredPlaylistLock);
    s_retiredPlaylists.push_back(p);
}

Player Player::INSTANCE;

Player::Player() :
    lastCheckTime(std::time(nullptr)),
    origStartTime(0),
    origStopTime(0),
    startTime(0),
    stopTime(0),
    stopMethod(0),
    priority(1000),
    allowScheduleOverride(true),
    forceStopped(false) {
}

Player::~Player() {
    // Static destruction: nothing will call Process() again, so drop the owning
    // references and run the deferred deletes inline.  The entries those
    // destructors park on PL_ENTRY_CLEANUPS are left for the process to
    // reclaim, exactly as the old `delete playlist` did.
    m_playlist.store(nullptr);
    std::vector<std::shared_ptr<Playlist>> owned;
    {
        std::lock_guard<std::mutex> lk(m_ownedPlaylistsLock);
        owned.swap(m_ownedPlaylists);
    }
    owned.clear();
    DrainRetiredPlaylists();
}

std::shared_ptr<Playlist> Player::CreatePlaylist(Playlist* parent) {
    std::shared_ptr<Playlist> pl(new Playlist(parent), RetirePlaylist);
    std::lock_guard<std::mutex> lk(m_ownedPlaylistsLock);
    m_ownedPlaylists.push_back(pl);
    return pl;
}

void Player::SetCurrentPlaylist(Playlist* p) {
    std::shared_ptr<Playlist> owned;
    {
        std::lock_guard<std::mutex> lk(m_ownedPlaylistsLock);
        for (auto& sp : m_ownedPlaylists) {
            if (sp.get() == p) {
                owned = sp;
                break;
            }
        }
    }
    if (!owned) {
        // Invariant: only an instance this player created and has not yet
        // released can become current.  Getting here means a raw Playlist*
        // outlived its ownership — publishing it (or publishing null) would
        // hand every reader a dangling or absent playlist, so keep the current
        // one and make the breakage visible instead.
        LogErr(VB_PLAYLIST, "Player::SetCurrentPlaylist() called with an unowned Playlist %p — keeping the current playlist\n",
               (void*)p);
        return;
    }
    m_playlist.store(std::move(owned));
}

void Player::ReleasePlaylist(Playlist* p) {
    // Declared before the lock so the reference is dropped — and the deleter
    // run, if this was the last one — after m_ownedPlaylistsLock is released.
    std::shared_ptr<Playlist> dropped;
    {
        std::lock_guard<std::mutex> lk(m_ownedPlaylistsLock);
        for (auto it = m_ownedPlaylists.begin(); it != m_ownedPlaylists.end(); ++it) {
            if (it->get() == p) {
                dropped = std::move(*it);
                m_ownedPlaylists.erase(it);
                break;
            }
        }
    }
}

void Player::DrainRetiredPlaylists() {
    while (true) {
        Playlist* p = nullptr;
        {
            std::lock_guard<std::mutex> lk(s_retiredPlaylistLock);
            if (s_retiredPlaylists.empty())
                return;
            p = s_retiredPlaylists.back();
            s_retiredPlaylists.pop_back();
        }
        // Deliberately unlocked: ~Playlist runs arbitrary entry destructors.
        delete p;
    }
}

void Player::Init() {
    /*
   * Start Playlist Callback
   */
    std::function<void(const std::string&, const std::string&)>
        playlist_callback = [](const std::string& topic_in,
                               const std::string& payload) {
            std::string emptyStr;
            std::string topic = topic_in;
            topic.replace(0, 14, emptyStr); // Replace until /#

            int pos = topic.find("/");
            if (pos == std::string::npos) {
                LogWarn(VB_PLAYLIST, "Ignoring Invalid playlist topic: playlist/%s\n",
                        topic.c_str());
                return;
            }

            std::string newPlaylistName = topic.substr(0, pos);
            std::string topicEnd = topic.substr(pos);

            if (topicEnd == "/start") {
                pos = 0;
                if (!payload.empty()) {
                    pos = std::atoi(payload.c_str());
                }

                LogDebug(VB_CONTROL, "Starting Playlist '%s' with message '%s'\n",
                         newPlaylistName.c_str(), payload.c_str());
                Player::INSTANCE.StartPlaylist(newPlaylistName, -1, pos);
                LogDebug(VB_CONTROL, "Call to Player::INSTANCE.StartPlaylist complete\n");
            } else {
                // Runs on the MQTT thread; the snapshot pins the instance for
                // the call.
                std::shared_ptr<Playlist> pl = Player::INSTANCE.PlaylistSnapshot();
                if (pl)
                    pl->MQTTHandler(topic, payload);
            };

            LogDebug(VB_CONTROL, "exit playlist_callback (MQTT)\n");
        };
    Events::AddCallback("/set/playlist/#", playlist_callback);

    // The resident playlist.  Created here rather than in the constructor
    // because Playlist's own constructor reaches for Events, PluginManager and
    // CommandManager, which are not yet built during static initialization.
    // From this point on PlaylistSnapshot() never returns null: the resident
    // instance is only ever replaced (SwitchToInsertedPlaylist, SetIdle's walk
    // back up the parent chain), never cleared.
    std::shared_ptr<Playlist> pl = CreatePlaylist(nullptr);
    SetCurrentPlaylist(pl.get());
}

int Player::StartPlaylist(const std::string& name, const int repeat,
                          const int startPosition, const int endPosition, const int manualPriority,
                          const bool allowScheduleOverride) {
    if ((GetStatus() == FPP_STATUS_IDLE) ||
        (name != playlistName)) {
        playlistName = name;
        startTime = std::time(nullptr);
        stopTime = 0;
        origStopTime = 0;
        stopMethod = 0;
        priority = manualPriority;
        this->allowScheduleOverride = allowScheduleOverride;
    }

    forceStopped = false;
    forceStoppedPlaylist = "";

    std::shared_ptr<Playlist> pl = PlaylistSnapshot();
    LogDebug(VB_PLAYLIST, "Manually starting %srepeating playlist '%s'\n",
             (repeat == -1 ? pl->GetRepeat() : repeat) ? "" : "non-",
             playlistName.c_str());

    return pl->Play(playlistName.c_str(), startPosition, repeat, -1, endPosition);
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
             stopMethod == 0 ? "Graceful Stop" : stopMethod == 1 ? "Hard Stop"
                                             : stopMethod == 2   ? "Graceful Stop After Loop"
                                                                 : "",
             (int)(stopTime - std::time(nullptr)));

    return PlaylistSnapshot()->Play(playlistName.c_str(), position, repeat, scheduleEntry);
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

    char timeBuf[32];
    LogDebug(VB_PLAYLIST, "New end time is now: %s", ctime_r(&stopTime, timeBuf));

    return 1;
}

void Player::InsertPlaylistAsNext(const std::string& filename, const int startPosition, const int endPos) {
    PlaylistSnapshot()->InsertPlaylistAsNext(filename, startPosition, endPos);
}

void Player::InsertPlaylistImmediate(const std::string& filename, const int startPosition, const int endPos) {
    PlaylistSnapshot()->InsertPlaylistImmediate(filename, startPosition, endPos);
}

int Player::StopNow(int forceStop) {
    forceStopped = (bool)forceStop;
    if (forceStopped)
        forceStoppedPlaylist = playlistName;
    else
        forceStoppedPlaylist = "";
    return PlaylistSnapshot()->StopNow(forceStop);
}

int Player::StopGracefully(int forceStop, int afterCurrentLoop) {
    forceStopped = (bool)forceStop;
    if (forceStopped)
        forceStoppedPlaylist = playlistName;
    else
        forceStoppedPlaylist = "";

    return PlaylistSnapshot()->StopGracefully(forceStop, afterCurrentLoop);
}

int Player::Process() {
    std::time_t procTime = std::time(nullptr);

    // See if we need to stop a scheduled playlist
    if ((PlaylistSnapshot()->getPlaylistStatus() == FPP_STATUS_PLAYLIST_PLAYING) &&
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
                         stopMethod == 0 ? "Stopping Gracefully" : stopMethod == 1 ? "Hard Stop"
                                                               : stopMethod == 2   ? "Stopping Gracefully After Loop"
                                                                                   : "",
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

    // Deliberately a second snapshot rather than one hoisted to the top of the
    // method: the stop handling above can legitimately change which playlist is
    // current (a hard stop of an inserted playlist unwinds one level and hands
    // the player back to the parent), and this tick must process whichever one
    // is current now — which is what the raw global did.
    return PlaylistSnapshot()->Process();
}

void Player::ProcessMedia() {
    PlaylistSnapshot()->ProcessMedia();
}

int Player::IsPlaying() {
    return PlaylistSnapshot()->IsPlaying();
}

std::string Player::GetPlaylistName() {
    return PlaylistSnapshot()->GetPlaylistName();
}

PlaylistStatus Player::GetStatus() {
    return PlaylistSnapshot()->getPlaylistStatus();
}

int Player::GetRepeat() {
    return PlaylistSnapshot()->GetRepeat();
}

int Player::GetStopMethod() {
    return stopMethod;
}

int Player::GetPosition() {
    return PlaylistSnapshot()->GetPosition();
}

Json::Value Player::GetInfo(void) {
    return PlaylistSnapshot()->GetInfo();
}

int Player::GetScheduleEntry() {
    return PlaylistSnapshot()->GetScheduleEntry();
}

uint64_t Player::GetFileTime() {
    return PlaylistSnapshot()->GetFileTime();
}

Json::Value Player::GetConfig() {
    return PlaylistSnapshot()->GetConfig();
}

Json::Value Player::GetMqttStatusJSON() {
    return PlaylistSnapshot()->GetMqttStatusJSON();
}

int Player::WasScheduled() {
    return PlaylistSnapshot()->WasScheduled();
}

int Player::FindPosForMS(uint64_t& ms, bool itemDefinedOnly) {
    return PlaylistSnapshot()->FindPosForMS(ms, itemDefinedOnly);
}

void Player::GetFilenamesForPos(int pos, std::string& seq, std::string& med) {
    PlaylistSnapshot()->GetFilenamesForPos(pos, seq, med);
}

int Player::Load(const std::string filename) {
    return PlaylistSnapshot()->Load(filename.c_str());
}

int Player::Start() {
    return PlaylistSnapshot()->Start();
}

void Player::RestartItem() {
    PlaylistSnapshot()->RestartItem();
}

void Player::NextItem() {
    PlaylistSnapshot()->NextItem();
}

void Player::PrevItem() {
    PlaylistSnapshot()->PrevItem();
}

void Player::Pause() {
    PlaylistSnapshot()->Pause();
}

void Player::Resume() {
    PlaylistSnapshot()->Resume();
}

int Player::Cleanup(void) {
    return PlaylistSnapshot()->Cleanup();
}

void Player::GetCurrentStatus(Json::Value& result) {
    PlaylistSnapshot()->GetCurrentStatus(result);
}

Json::Value Player::GetStatusJSON() {
    Json::Value result;
    Json::Value playlists(Json::arrayValue);
    Json::Value pl;

    // One snapshot for the whole object: six separate loads could straddle a
    // playlist switch and report a mix of two playlists in one status blob.
    std::shared_ptr<Playlist> cur = PlaylistSnapshot();
    pl = cur->GetInfo();
    pl["details"] = cur->GetConfig();
    pl["status"] = (int)cur->getPlaylistStatus();
    pl["scheduled"] = cur->WasScheduled() ? true : false;
    pl["position"] = cur->GetPosition();
    pl["lastModified"] = (Json::UInt64)cur->GetFileTime();

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

// --------------------------------------------------------------------------
// OpenAPI docs for the /player/* endpoints handled below.
// --------------------------------------------------------------------------

/**
 * Get the player status. Equivalent to /api/player/status.
 *
 * @route GET /api/player
 * @response 200 Player status JSON.
 */

/**
 * Get the player status (playlist, sequence, timing, mode, etc.).
 *
 * @route GET /api/player/status
 * @response 200 Player status JSON.
 */

/**
 * Get information about the currently playing playlist.
 *
 * @route GET /api/player/current
 * @response 200 Object with a `playlist` member describing the current playlist.
 */
HttpResponsePtr Player::render_GET(const HttpRequestPtr& req) {
    auto pieces = getPathPieces(req->path());
    int plen = pieces.size();

    Json::Value result;

    if ((plen == 1) || ((plen == 2) && (pieces[1] == "status"))) {
        result = GetStatusJSON();
        return makeStringResponse(SaveJsonToString(result, "  "), 200, "application/json");
    } else if ((plen == 2) && (pieces[1] == "current")) {
        Json::Value result;
        Json::Value pl;
        pl = PlaylistSnapshot()->GetInfo();
        result["playlist"] = pl;
        return makeStringResponse(SaveJsonToString(result, "  "), 200, "application/json");
    }

    return makeStringResponse("Not Found", 404, "text/plain");
}

HttpResponsePtr Player::render_POST(const HttpRequestPtr& req) {
    auto pieces = getPathPieces(req->path());
    int plen = pieces.size();

    return makeStringResponse("Not Found", 404, "text/plain");
}

HttpResponsePtr Player::render_PUT(const HttpRequestPtr& req) {
    auto pieces = getPathPieces(req->path());
    int plen = pieces.size();

    return makeStringResponse("Not Found", 404, "text/plain");
}
