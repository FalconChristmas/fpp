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

#include <atomic>
#include "fpp-json.h"
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "PlaylistEntryBase.h"

enum PlaylistStatus {
    FPP_STATUS_IDLE = 0,
    FPP_STATUS_PLAYLIST_PLAYING,
    FPP_STATUS_STOPPING_GRACEFULLY,
    FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP,
    FPP_STATUS_STOPPING_NOW,
    FPP_STATUS_PLAYLIST_PAUSED
};

class Playlist {
public:
    Playlist(Playlist* parent = NULL);
    ~Playlist();

    PlaylistStatus getPlaylistStatus();

    // New methods
    Json::Value LoadJSON(const std::string& filename);
    int Load(Json::Value& config);
    int Load(const std::string& filename);
    PlaylistEntryBase* LoadPlaylistEntry(Json::Value entry);
    int LoadJSONIntoPlaylist(std::vector<PlaylistEntryBase*>& playlistPart, const Json::Value& entries, int startPos, int& maxEntries);

    int Start(void);
    int StopNow(int forceStop = 0);
    int StopGracefully(int forceStop = 0, int afterCurrentLoop = 0);
    void SetIdle(bool exit = true);

    void Pause();
    void Resume();

    int IsPlaying(void);

    int Process(void);
    void ProcessMedia(void);
    int Cleanup(void);

    // Refresh the lock-free snapshot the crash handler reads (see
    // PlaylistDumpCrashState).  Called at the points where the section or
    // position can change; cheap enough to not need gating.
    void UpdateCrashSnapshot(void);

    int Play(const std::string& filename, const int position = -1, const int repeat = -1, const int scheduleEntry = -1, const int endPosition = -1);

    void InsertPlaylistAsNext(const std::string& filename, const int startPosition = -1, const int endPos = -1);
    void InsertPlaylistImmediate(const std::string& filename, const int startPosition = -1, const int endPos = -1);

    void SetPosition(int position);
    void SetRepeat(int repeat);

    void RandomizeMainPlaylist();

    void Dump(void);

    // Helper method for global pause between sequences
    void StartPlayingWithGlobalPause(PlaylistEntryBase* entry);

    void NextItem(void);
    void RestartItem(void);
    void PrevItem(void);

    void GetCurrentStatus(Json::Value& result);
    Json::Value GetCurrentEntry(void);
    Json::Value GetInfo(void);
    void GetInfo(Json::Value& v);
    std::string GetPlaylistName(void) { return m_name; }
    int GetRepeat(void) { return m_repeat; }
    int GetPosition(void);
    int GetSize(void);
    int GetLoopNumber(void) { return (m_loop + 1); }
    std::string GetConfigStr(void);
    Json::Value GetConfig(void);
    uint64_t GetFileTime(void) { return (Json::UInt64)m_fileTime; }
    int GetForceStop(void) { return m_forceStop; }
    int WasScheduled(void) { return (m_scheduleEntry != -1) ? 1 : 0; }
    int GetScheduleEntry(void) { return m_scheduleEntry; }

    // these Positions are 0 based and includes the leadIn entries
    uint64_t GetCurrentPosInMS();
    uint64_t GetCurrentPosInMS(int& pos, uint64_t& posms, bool itemDefinedOnly = false);
    uint64_t GetPosStartInMS(int pos);
    int FindPosForMS(uint64_t& ms, bool itemDefinedOnly = false); // ms will be updated with how far into Pos it would be
    void GetFilenamesForPos(int pos, std::string& seq, std::string& med);

    int MQTTHandler(std::string topic, std::string msg);

    int FileHasBeenModified(void);
    std::string ReplaceMatches(std::string in);
    Json::Value GetMqttStatusJSON(); // Returns Status as JSON

private:
    void GetParentPlaylistNames(std::list<std::string>& names);
    int ReloadPlaylist(void);
    void ReloadIfNeeded(void);
    void SwitchToMainPlaylist(void);
    void SwitchToLeadOut(void);

    bool WillStopAfterCurrent();
    Playlist* SwitchToInsertedPlaylist(bool isStopping = false);

    // The entry at the current section position, or nullptr if there is none.
    // "None" is a normal state, not an error: m_sectionPosition is allowed to
    // sit at exactly m_currentSection->size() while the playlist is between
    // items, and m_currentSection is null until a playlist is loaded.  Every
    // access to the current entry goes through here so that single guard has
    // one home.  Caller must hold m_playlistMutex.
    PlaylistEntryBase* CurrentEntry();

    // Written under m_playlistMutex, but read by unlocked getters from other
    // threads (status paths, the scheduler) -- atomic so those reads are
    // well-defined; volatile never made them so.
    std::atomic<PlaylistStatus> m_status;

    Playlist* m_parent;
    std::string m_filename;
    std::string m_name;
    std::string m_desc;
    int m_repeat;
    int m_loop;
    int m_loopCount;
    int m_random;
    int m_blankAtEnd;
    long long m_startTime;
    int m_subPlaylistDepth;
    int m_scheduleEntry;
    int m_forceStop;
    int m_stopAtPos;
    int m_loadStartPos;
    int m_loadEndPos;

    time_t m_fileTime;
    Json::Value m_config;
    time_t m_configTime;
    Json::Value m_playlistInfo;

    std::string m_currentState;
    std::string m_currentSectionStr;
    int m_sectionPosition;
    int m_startPosition;

    // Global pause between sequences feature
    int m_globalPauseBetweenSequencesMS;
    bool m_isInGlobalPause;
    bool m_shouldStartGlobalPause;
    long long m_globalPauseStartTime;

    std::string m_insertedPlaylist;
    int m_insertedPlaylistPosition;
    int m_insertedPlaylistEndPosition;

    // NOTE: the old startNewPlaylist* members (deferred re-entrant start
    // requests) moved to file-scope state in Playlist.cpp — the request
    // belongs to the player, not to one Playlist instance, since the active
    // instance can be swapped/queued for cleanup between defer and replay.

    // The body of Play(); public Play() defers re-entrant calls, PlayImpl
    // executes.  Also called directly by SwitchToInsertedPlaylist(), which
    // must start the inserted child inline within the parent's transition.
    int PlayImpl(const std::string& filename, const int position, const int repeat, const int scheduleEntry, const int endPosition);
    // The body of StopNow(); same public-defers / Impl-executes split.
    int StopNowImpl(int forceStop = 0);

    std::recursive_mutex m_playlistMutex;

    std::vector<PlaylistEntryBase*> m_leadIn;
    std::vector<PlaylistEntryBase*> m_mainPlaylist;
    std::vector<PlaylistEntryBase*> m_leadOut;
    std::vector<PlaylistEntryBase*>* m_currentSection;
};

// Source-compatibility façade for the old `extern Playlist* playlist;` global.
//
// The player now owns its playlists through shared_ptr (see Player.h), so
// nothing in-tree touches this: FPP's own code calls Player::INSTANCE, which
// snapshots the current playlist per call.  External plugins, however, were
// written against a raw global and dereference it as `playlist->Method(...)`.
//
// That expression still compiles, through two chained operator->s: PlaylistRef
// hands back a PlaylistHandle holding a snapshot, and the handle's operator->
// yields the raw pointer.  The handle is a temporary of the full expression, so
// the snapshot — and therefore the instance — outlives the call even if the
// main loop retires that playlist mid-call.  That makes the old plugin
// expression lifetime-safe rather than merely still-compiling.
//
// ABI is NOT preserved (this used to be a pointer object); plugins are
// recompiled against these headers at update, which is what makes that fine.
//
// Before Player::Init() the snapshot is empty and `playlist->` dereferences
// null, exactly as the old NULL-initialized global did.  It is deliberately not
// papered over with a dummy instance: constructing a Playlist needs Events,
// PluginManager and CommandManager, so a plugin calling this from its own
// static initialization has a startup-order bug that a stand-in would hide.
class PlaylistHandle {
public:
    Playlist* operator->() const { return p.get(); }
    std::shared_ptr<Playlist> p;
};

class PlaylistRef {
public:
    PlaylistHandle operator->() const;
    // So `if (playlist)` in external code still compiles and still means
    // "is there a playlist to talk to".
    explicit operator bool() const;
};

extern PlaylistRef playlist;

// Write the last-known playing state to fd for a crash report.
//
// Reads a snapshot of plain scalars rather than the live Playlist, on purpose:
// the crash handler cannot take m_playlistMutex (the faulting thread may hold
// it, and blocking there hangs the process instead of producing a report), and
// it cannot walk a std::string or vector that another thread is mid-update.
// A field may be one update stale or torn; that is worth far more than nothing.
void PlaylistDumpCrashState(int fd);
