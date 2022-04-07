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

class RemapOutputProcessor;

class PlaylistEntryRemap : public PlaylistEntryBase {
public:
    PlaylistEntryRemap(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    virtual ~PlaylistEntryRemap();

    virtual int Init(Json::Value& config) override;

    virtual int StartPlaying(void) override;

    virtual void Dump(void) override;

    virtual Json::Value GetConfig(void) override;

private:
    std::string m_action;

    int m_srcChannel;
    int m_dstChannel;
    int m_channelCount;
    int m_loops;
    int m_reverse;
};
