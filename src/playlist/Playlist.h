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
#include <list>
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
    Json::Value LoadJSON(const char* filename);
    int Load(Json::Value& config);
    int Load(const char* filename);
    PlaylistEntryBase* LoadPlaylistEntry(Json::Value entry);
    int LoadJSONIntoPlaylist(std::vector<PlaylistEntryBase*>& playlistPart, const Json::Value& entries);

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

    int Play(const char* filename, const int position = -1, const int repeat = -1, const int scheduleEntry = -1, const int endPosition = -1);

    void InsertPlaylistAsNext(const std::string& filename, const int startPosition = -1, const int endPos = -1);
    void InsertPlaylistImmediate(const std::string& filename, const int startPosition = -1, const int endPos = -1);

    void SetPosition(int position);
    void SetRepeat(int repeat);

    void RandomizeMainPlaylist();

    void Dump(void);

    void NextItem(void);
    void RestartItem(void);
    void PrevItem(void);

    void GetCurrentStatus(Json::Value& result);
    Json::Value GetCurrentEntry(void);
    Json::Value GetInfo(void);
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
    uint64_t GetCurrentPosInMS(int& pos, uint64_t& posms);
    uint64_t GetPosStartInMS(int pos);
    int FindPosForMS(uint64_t& ms); //ms will be updated with how far into Pos it would be
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

    volatile PlaylistStatus m_status;

    Playlist* m_parent;
    std::string m_filename;
    std::string m_name;
    std::string m_desc;
    int m_repeat;
    int m_loop;
    int m_loopCount;
    int m_random;
    int m_blankBetweenSequences;
    int m_blankBetweenIterations;
    int m_blankAtEnd;
    long long m_startTime;
    int m_subPlaylistDepth;
    int m_scheduleEntry;
    int m_forceStop;
    int m_stopAtPos;

    time_t m_fileTime;
    Json::Value m_config;
    time_t m_configTime;
    Json::Value m_playlistInfo;

    std::string m_currentState;
    std::string m_currentSectionStr;
    int m_sectionPosition;
    int m_startPosition;

    std::string m_insertedPlaylist;
    int m_insertedPlaylistPosition;
    int m_insertedPlaylistEndPosition;

    std::string startNewPlaylistFilename;
    int startNewPlaylistPosition = 0;
    int startNewPlaylistRepeat = 0;
    int startNewPlaylistScheduleEntry = 0;
    int startNewPlaylistEndPosition = 0;

    std::recursive_mutex m_playlistMutex;

    std::vector<PlaylistEntryBase*> m_leadIn;
    std::vector<PlaylistEntryBase*> m_mainPlaylist;
    std::vector<PlaylistEntryBase*> m_leadOut;
    std::vector<PlaylistEntryBase*>* m_currentSection;
};

// Temporary singleton during conversion
extern Playlist* playlist;
