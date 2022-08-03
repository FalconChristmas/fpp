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

class PlaylistEntrySequence : public PlaylistEntryBase {
public:
    PlaylistEntrySequence(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    virtual ~PlaylistEntrySequence();

    virtual int Init(Json::Value& config) override;

    int PreparePlay(int frame = 0);
    virtual int StartPlaying(void) override;
    virtual int Process(void) override;
    virtual int Stop(void) override;

    virtual void Pause() override;
    virtual bool IsPaused() override;
    virtual void Resume() override;

    virtual void Dump(void) override;

    virtual Json::Value GetConfig(void) override;
    virtual Json::Value GetMqttStatus(void) override;

    virtual uint64_t GetLengthInMS() override;
    virtual uint64_t GetElapsedMS() override;

    void disableAdjustTiming() { m_adjustTiming = false; }

    const std::string &GetSequenceName(void) const { return m_sequenceName; }

private:
    long long m_startTme;
    bool m_adjustTiming;
    bool m_prepared;
    int m_duration;
    int m_sequenceFrameTime;
    std::string m_sequenceName;

    int m_pausedFrame;
};
