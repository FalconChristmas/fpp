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

#include "PlaylistEntryVolume.h"
#include "mediaoutput/mediaoutput.h"

/*
 *
 */
PlaylistEntryVolume::PlaylistEntryVolume(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_volume(0) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryVolume::PlaylistEntryVolume()\n");

    m_type = "volume";
    m_deprecated = 1;
}

/*
 *
 */
PlaylistEntryVolume::~PlaylistEntryVolume() {
}

/*
 *
 */
int PlaylistEntryVolume::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryVolume::Init()\n");

    std::string vol = config["volume"].asString();
    m_volAdjust = ((vol[0] == '-') || (vol[0] == '+'));
    m_volume = std::atoi(vol.c_str());

    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryVolume::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryVolume::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    if (!m_volAdjust) {
        setVolume(m_volume);
    } else {
        int vol = getVolume();
        vol += m_volume;
        if (vol < 0) {
            vol = 0;
        } else if (vol > 100) {
            vol = 100;
        }
        setVolume(vol);
    }

    PlaylistEntryBase::StartPlaying();

    FinishPlay();

    return 1;
}

/*
 *
 */
void PlaylistEntryVolume::Dump(void) {
    LogDebug(VB_PLAYLIST, "Volume: %d\n", m_volume);
}

/*
 *
 */
Json::Value PlaylistEntryVolume::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    result["volume"] = m_volume;

    return result;
}
