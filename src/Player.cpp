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

#include <atomic>
#include <chrono>
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

// A count: a command holds one around StartPlaylist(), whose PlayImpl() holds
// its own.
static std::atomic<int> s_switchesInFlight{ 0 };
// When the count last left 0.  A command's scope also covers the presets and
// plugin callbacks its stop and start fire, and a script plugin's callback is
// waited for with no timeout: past this long, the switch no longer holds the
// scheduler back.
static std::atomic<int64_t> s_switchSinceMs{ 0 };
static std::atomic<bool> s_switchOverdueLogged{ false };
static constexpr int64_t MAX_SWITCH_MS = 5000;

static int64_t SteadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

Player::SwitchScope::SwitchScope() {
    if (s_switchesInFlight++ == 0) {
        s_switchSinceMs = SteadyMs();
        s_switchOverdueLogged = false;
    }
}

Player::SwitchScope::~SwitchScope() {
    s_switchesInFlight--;
}

bool Player::Switching() {
    if (s_switchesInFlight == 0)
        return false;
    int64_t ms = SteadyMs() - s_switchSinceMs;
    if (ms < MAX_SWITCH_MS)
        return true;
    if (!s_switchOverdueLogged.exchange(true))
        LogWarn(VB_PLAYLIST, "A playlist switch is still running after %lld ms (a slow PLAYLIST_STOPPED/STARTED preset or plugin callback?); the scheduler carries on\n", (long long)ms);
    return false;
}

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

std::shared_ptr<Playlist> Player::CreatePlaylist(std::shared_ptr<Playlist> parent) {
    std::shared_ptr<Playlist> pl(new Playlist(std::move(parent)), RetirePlaylist);
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

    if (Events::HasEventHandlers()) {
        // Deprecated topics.  Registered here, once, rather than per Playlist
        // instance: a per-instance callback captured that instance, outlived
        // it, and was re-added (growing the handler's topic list) by every
        // inserted playlist.  Same snapshot dispatch as /set/playlist/#.
        std::function<void(const std::string&, const std::string&)> legacy_callback =
            [](const std::string& t, const std::string& payload) {
                LogDebug(VB_CONTROL, "Received deprecated MQTT Topic: '%s' \n", t.c_str());
                std::string topic = t;
                topic.replace(0, 10, ""); // strip "/playlist/"
                std::shared_ptr<Playlist> pl = Player::INSTANCE.PlaylistSnapshot();
                if (pl)
                    pl->MQTTHandler(topic, payload);
            };
        Events::AddCallback("/playlist/name/set", legacy_callback);
        Events::AddCallback("/playlist/repeat/set", legacy_callback);
        Events::AddCallback("/playlist/sectionPosition/set", legacy_callback);
    }

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
    {
        std::lock_guard<std::mutex> lk(m_stateLock);
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
    }

    std::shared_ptr<Playlist> pl = PlaylistSnapshot();
    LogDebug(VB_PLAYLIST, "Manually starting %srepeating playlist '%s'\n",
             (repeat == -1 ? pl->GetRepeat() : repeat) ? "" : "non-",
             name.c_str());

    return pl->Play(name, startPosition, repeat, -1, endPosition);
}

int Player::StartScheduledPlaylist(const std::string& name, const int position,
                                   const int repeat, const int scheduleEntry, const int scheduledPriority,
                                   const time_t sTime, const time_t eTime, const int method) {
    {
        std::lock_guard<std::mutex> lk(m_stateLock);
        playlistName = name;
        origStartTime = sTime;
        origStopTime = eTime;
        startTime = std::time(nullptr);
        stopTime = eTime;
        stopMethod = method;
        priority = scheduledPriority;
        forceStopped = false;
        forceStoppedPlaylist = "";
    }

    LogDebug(VB_PLAYLIST, "Starting %srepeating playlist '%s' with scheduled '%s' in %d seconds\n",
             repeat ? "" : "non-",
             name.c_str(),
             method == 0 ? "Graceful Stop" : method == 1 ? "Hard Stop"
                                         : method == 2   ? "Graceful Stop After Loop"
                                                         : "",
             (int)(eTime - std::time(nullptr)));

    return PlaylistSnapshot()->Play(name, position, repeat, scheduleEntry);
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

    std::lock_guard<std::mutex> lk(m_stateLock);
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

void Player::RecordForceStop(int forceStop) {
    std::lock_guard<std::mutex> lk(m_stateLock);
    forceStopped = (bool)forceStop;
    if (forceStopped)
        forceStoppedPlaylist = playlistName;
    else
        forceStoppedPlaylist = "";
}

int Player::StopNow(int forceStop) {
    if (InHandedBackStoppedPreset()) {
        LogInfo(VB_PLAYLIST, "Ignoring Stop Now from the PLAYLIST_STOPPED preset of an inserted playlist: it has already stopped, and the playlist it interrupted carries on\n");
        return 1;
    }
    RecordForceStop(forceStop);
    return PlaylistSnapshot()->StopNow(forceStop);
}

int Player::StopGracefully(int forceStop, int afterCurrentLoop) {
    if (InHandedBackStoppedPreset()) {
        LogInfo(VB_PLAYLIST, "Ignoring Stop Gracefully from the PLAYLIST_STOPPED preset of an inserted playlist: it has already stopped, and the playlist it interrupted carries on\n");
        return 1;
    }
    RecordForceStop(forceStop);
    return PlaylistSnapshot()->StopGracefully(forceStop, afterCurrentLoop);
}

void Player::StopNowAllLevels(int forceStop) {
    int levels = PlaylistSnapshot()->GetLevelCount();
    for (int i = 0; i < levels && GetStatus() != FPP_STATUS_IDLE; i++) {
        StopNow(forceStop);
    }
}

std::time_t Player::GetOrigStartTime() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    return origStartTime;
}
std::time_t Player::GetOrigStopTime() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    return origStopTime;
}
std::time_t Player::GetStartTime() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    return startTime;
}
std::time_t Player::GetStopTime() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    return stopTime;
}
void Player::ClearForceStopped() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    forceStopped = false;
}
bool Player::GetForceStopped() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    return forceStopped;
}
std::string Player::GetForceStoppedPlaylist() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    return forceStoppedPlaylist;
}
int Player::GetPriority() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    return priority;
}
bool Player::GetAllowScheduleOverride() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    return allowScheduleOverride;
}

int Player::Process() {
    std::time_t procTime = std::time(nullptr);
    PlaylistStatus status = PlaylistSnapshot()->getPlaylistStatus();

    // See if we need to stop a scheduled playlist.  A paused one counts: it
    // used to be skipped, so a playlist paused during its window never
    // stopped, and as the scheduler waits for idle, every later scheduled show
    // was skipped too.
    bool check = false;
    std::time_t stop = 0;
    std::time_t origStop = 0;
    int method = 0;
    std::string name;
    {
        std::lock_guard<std::mutex> lk(m_stateLock);
        if (((status == FPP_STATUS_PLAYLIST_PLAYING) || (status == FPP_STATUS_PLAYLIST_PAUSED)) &&
            (stopTime) &&
            (lastCheckTime != procTime)) {
            lastCheckTime = procTime;
            check = true;
            stop = stopTime;
            origStop = origStopTime;
            method = stopMethod;
            name = playlistName;
        }
    }

    if (check) {
        if (stop > procTime) {
            bool logSwitch = false;
            int diff = stop - procTime;

            // Print the countdown more frequently as we get closer
            if (((diff > 300) && ((diff % 300) == 0)) ||
                ((diff > 60) && (diff <= 300) && ((diff % 60) == 0)) ||
                ((diff > 10) && (diff <= 60) && ((diff % 10) == 0)) ||
                ((diff <= 10))) {
                logSwitch = true;
            }

            if (logSwitch) {
                LogDebug(VB_PLAYLIST, "Playlist '%s' will switch to '%s' in %d second%s\n",
                         name.c_str(),
                         method == 0 ? "Stopping Gracefully" : method == 1 ? "Hard Stop"
                                                           : method == 2   ? "Stopping Gracefully After Loop"
                                                                           : "",
                         diff,
                         diff == 1 ? "" : "s");
            }
        }

        if (stop <= procTime) {
            // if user shortened the scheduled end time mark as force stopped
            int forceStop = (stop < origStop) ? 1 : 0;

            // A paused playlist just stops at the end of its window: a
            // graceful stop would first resume it, bringing the show back on
            // after its time to finish the interrupted entry.
            if (status == FPP_STATUS_PLAYLIST_PAUSED)
                method = 1;

            switch (method) {
            case 0: // Gracefully
                StopGracefully(forceStop);
                break;
            case 2: // After Loop
                StopGracefully(forceStop, 1);
                break;
            case 1: // Hard Stop
            default:
                StopNowAllLevels(forceStop);
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

void Player::SetRepeat(int repeat) {
    PlaylistSnapshot()->SetRepeat(repeat);
}

int Player::GetStopMethod() {
    std::lock_guard<std::mutex> lk(m_stateLock);
    return stopMethod;
}

int Player::GetPosition() {
    return PlaylistSnapshot()->GetPosition();
}

int Player::GetResumePosition() {
    return PlaylistSnapshot()->GetResumePosition();
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
    // The main loop calls this after an unlocked "now idle" read.  A start from
    // another thread can land between that read and here, so only clean up a
    // playlist that is still idle under its lock; SetIdle() has already run
    // Cleanup() on every transition to idle.
    return PlaylistSnapshot()->CleanupIfIdle();
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
    std::lock_guard<std::mutex> lk(m_stateLock);
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
