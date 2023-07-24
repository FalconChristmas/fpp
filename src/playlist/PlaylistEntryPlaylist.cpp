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

#include "../common.h"
#include "../log.h"

#include "PlaylistEntryPlaylist.h"

PlaylistEntryPlaylist::PlaylistEntryPlaylist(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_playlist(NULL) {
    m_type = "playlist";
}

PlaylistEntryPlaylist::~PlaylistEntryPlaylist() {
    if (m_playlist) {
        m_playlist->Cleanup();
        delete m_playlist;
    }
}

/*
 *
 */
int PlaylistEntryPlaylist::Init(Json::Value& config) {
    m_playlist = new Playlist(NULL);

    if (!m_playlist) {
        LogErr(VB_PLAYLIST, "Error creating sub playlist\n");
        return 0;
    }

    if ((config.isMember("name")) &&
        (!config["name"].asString().empty())) {
        m_playlistName = config["name"].asString();

        if (m_playlistName.empty()) {
            LogErr(VB_PLAYLIST, "Empty sub playlist name\n");
            return 0;
        }
    } else if (config.isMember("playlist")) {
        m_playlist->Load(config["playlist"]);
        m_playlist->RandomizeMainPlaylist();
    }

    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryPlaylist::StartPlaying(void) {
    if (!m_playlist)
        return 0;

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    if (!m_playlistName.empty()) {
        if (!m_playlist->Load(m_playlistName.c_str())) {
            return 0;
        }
        m_playlist->RandomizeMainPlaylist();
    }

    if (!m_playlist->Start())
        return 0;

    return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryPlaylist::Process(void) {
    if (!m_playlist->Process())
        FinishPlay();

    return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryPlaylist::Stop(void) {
    m_playlist->StopNow();

    return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryPlaylist::Dump(void) {
    LogDebug(VB_PLAYLIST, "Playlist Name: %s\n", m_playlistName.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryPlaylist::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();
    Json::Value playlistConfig = m_playlist->GetConfig();

    MergeJsonValues(result, playlistConfig);

    return result;
}
