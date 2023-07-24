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

#include <mutex>
#include <string>

#include "PlaylistEntryBase.h"
#include "PlaylistEntryMedia.h"
#include "PlaylistEntrySequence.h"

class PlaylistEntryBoth : public PlaylistEntryBase {
public:
    PlaylistEntryBoth(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    virtual ~PlaylistEntryBoth();

    virtual int Init(Json::Value& config) override;

    virtual int StartPlaying(void) override;
    virtual int Process(void) override;
    virtual int Stop(void) override;

    virtual void Dump(void) override;

    virtual Json::Value GetConfig(void) override;
    virtual Json::Value GetMqttStatus(void) override;

    std::string GetSequenceName(void) { return m_sequenceName; }
    std::string GetMediaName(void) { return m_mediaName; }

    virtual uint64_t GetLengthInMS() override;
    virtual uint64_t GetElapsedMS() override;

    virtual void Pause() override;
    virtual bool IsPaused() override;
    virtual void Resume() override;

private:
    int m_duration;

    std::string m_sequenceName;
    std::string m_mediaName;

    PlaylistEntryMedia* m_mediaEntry;
    PlaylistEntrySequence* m_sequenceEntry;

    std::recursive_mutex m_mutex;
};
