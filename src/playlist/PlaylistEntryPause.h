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

#include <string>

#include "PlaylistEntryBase.h"

class PlaylistEntryPause : public PlaylistEntryBase {
public:
    PlaylistEntryPause(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    ~PlaylistEntryPause();

    virtual int Init(Json::Value& config) override;

    virtual int StartPlaying(void) override;
    virtual int Process(void) override;
    virtual int Stop(void) override;

    virtual void Pause() override;
    virtual bool IsPaused() override;
    virtual void Resume() override;

    virtual uint64_t GetLengthInMS() override;
    virtual uint64_t GetElapsedMS() override;

    virtual void Dump(void) override;

    virtual Json::Value GetConfig(void) override;

private:
    float m_duration;
    long long m_startTime;
    long long m_endTime;
    long long m_finishTime;

    long long m_pausedRemaining;
};
