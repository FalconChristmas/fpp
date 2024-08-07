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

#include <pthread.h>

#include <string>

class Playlist;

class PlaylistEntryBase {
public:
    PlaylistEntryBase(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    virtual ~PlaylistEntryBase();

    virtual int Init(Json::Value& config);

    virtual int StartPlaying(void);
    virtual int IsStarted(void);
    virtual int IsPlaying(void);
    virtual int IsFinished(void);

    virtual int Prep(void);
    virtual int Process(void);
    virtual int Stop(void);

    virtual void Pause() {}
    virtual bool IsPaused() { return false; }
    virtual void Resume() {}

    virtual int HandleSigChild(pid_t pid);

    virtual void Dump(void);

    virtual Json::Value GetConfig(void);

    virtual Json::Value GetMqttStatus(void);

    virtual std::string ReplaceMatches(std::string in);

    virtual uint64_t GetLengthInMS() { return 0; }
    virtual uint64_t GetElapsedMS() { return 0; }

    std::string GetType(void) { return m_type; }
    int IsPrepped(void) { return m_isPrepped; }

    int64_t GetTimeCode() const { return m_timeCode; }

    enum class PlaylistBranchType {
        NoBranch,
        Index,
        Offset,
        Playlist
    };
    virtual PlaylistBranchType GetNextBranchType() { return PlaylistBranchType::NoBranch; }
    virtual std::string GetNextSection(void) { return ""; }
    virtual int GetNextItem(void) { return 0; }
    virtual std::string GetNextData(void) { return ""; }

    void SetPositionInPlaylist(int i) { m_playlistPosition = i; }
    int GetPositionInPlaylist() { return m_playlistPosition; }

protected:
    int CanPlay(void);
    void FinishPlay(void);

    std::string m_type;
    std::string m_note;
    int m_enabled;
    int m_isStarted;
    int m_isPlaying;
    int m_isFinished;
    int m_playOnce;
    int m_playCount;
    int m_isPrepped;
    int m_deprecated;
    int m_playlistPosition;

    Json::Value m_config;
    PlaylistEntryBase* m_parent;
    Playlist* m_parentPlaylist;

    int64_t m_timeCode = -1;
};
