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

#include <thread>

#include "../common.h"
#include "../log.h"

#include "PlaylistEntryBoth.h"
#include "mediadetails.h"
#include "../MultiSync.h"

PlaylistEntryBoth::PlaylistEntryBoth(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_duration(0),
    m_mediaEntry(NULL),
    m_sequenceEntry(NULL) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBoth::PlaylistEntryBoth()\n");

    m_type = "both";
}

PlaylistEntryBoth::~PlaylistEntryBoth() {
}

/*
 *
 */
int PlaylistEntryBoth::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBoth::Init()\n");

    m_sequenceEntry = new PlaylistEntrySequence(m_parentPlaylist, this);
    if (!m_sequenceEntry)
        return 0;

    m_mediaEntry = new PlaylistEntryMedia(m_parentPlaylist, this);
    if (!m_mediaEntry)
        return 0;

    if (!m_mediaEntry->Init(config)) {
        delete m_mediaEntry;
        m_mediaEntry = nullptr;
    }
    m_mediaName = m_mediaEntry->GetMediaName();

    // the media will drive the timing, not the fseq
    m_sequenceEntry->disableAdjustTiming();

    if (!m_sequenceEntry->Init(config))
        return 0;

    m_sequenceName = m_sequenceEntry->GetSequenceName();
    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryBoth::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBoth::StartPlaying()\n");

    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);
    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    if (m_mediaEntry && !m_mediaEntry->PreparePlay()) {
        delete m_mediaEntry;
        m_mediaEntry = nullptr;
    }
    if (!m_mediaEntry) {
        LogDebug(VB_PLAYLIST, "Skipping media playlist entry, likely blacklisted audio: %s\n", m_mediaName.c_str());
    }
    if (!m_sequenceEntry->PreparePlay()) {
        LogDebug(VB_PLAYLIST, "Problems starting sequence: %s\n", m_sequenceEntry->GetSequenceName().c_str());
    }

    if (multiSync->isMultiSyncEnabled() && m_mediaEntry && m_mediaEntry->m_openTime) {
        long long st = GetTimeMS() - m_mediaEntry->m_openTime;
        if (st < PlaylistEntryMedia::m_openStartDelay) {
            std::this_thread::sleep_for(std::chrono::milliseconds(PlaylistEntryMedia::m_openStartDelay - st - 2));
        }
    }

    if (!m_sequenceEntry->StartPlaying()) {
        LogDebug(VB_PLAYLIST, "Could not start sequence: %s\n", m_sequenceEntry->GetSequenceName().c_str());
        if (m_mediaEntry) {
            m_mediaEntry->Stop();
        }
        FinishPlay();
        return 0;
    }
    if (m_mediaEntry && !m_mediaEntry->StartPlaying()) {
        LogDebug(VB_PLAYLIST, "Could not start media: %s\n", m_mediaName.c_str());
        delete m_mediaEntry;
        m_mediaEntry = nullptr;
        m_sequenceEntry->Stop();
        FinishPlay();
        return 0;
    }

    return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryBoth::Process(void) {
    LogExcess(VB_PLAYLIST, "PlaylistEntryBoth::Process()\n");
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);

    if (m_mediaEntry)
        m_mediaEntry->Process();
    m_sequenceEntry->Process();

    if (m_mediaEntry && m_mediaEntry->IsFinished()) {
        FinishPlay();
        m_sequenceEntry->Stop();
    }

    if (m_sequenceEntry->IsFinished()) {
        FinishPlay();
        if (m_mediaEntry)
            m_mediaEntry->Stop();
    }

    // FIXME PLAYLIST, handle sync in here somehow

    return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryBoth::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBoth::Stop()\n");
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);

    if (m_mediaEntry)
        m_mediaEntry->Stop();
    m_sequenceEntry->Stop();

    return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryBoth::Dump(void) {
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);
    PlaylistEntryBase::Dump();

    if (m_mediaEntry)
        m_mediaEntry->Dump();
    m_sequenceEntry->Dump();
}

uint64_t PlaylistEntryBoth::GetLengthInMS() {
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);
    uint64_t m = m_mediaEntry ? m_mediaEntry->GetLengthInMS() : 0;
    uint64_t s = m_sequenceEntry ? m_sequenceEntry->GetLengthInMS() : 0;
    if (m && m < s) {
        //media drives it, when media completes, we stop
        return m;
    }
    return s;
}
uint64_t PlaylistEntryBoth::GetElapsedMS() {
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);
    if (m_mediaEntry) {
        return m_mediaEntry->GetElapsedMS();
    }
    if (m_sequenceEntry) {
        return m_sequenceEntry->GetElapsedMS();
    }
    return 0;
}

/*
 *
 */
Json::Value PlaylistEntryBoth::GetConfig(void) {
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);
    Json::Value result = PlaylistEntryBase::GetConfig();

    if (m_mediaEntry) {
        result["media"] = m_mediaEntry->GetConfig();
    } else {
        //fake it so the display will display the times
        //where the sequence is
        result["media"] = m_sequenceEntry->GetConfig();
    }
    result["sequence"] = m_sequenceEntry->GetConfig();

    return result;
}

Json::Value PlaylistEntryBoth::GetMqttStatus(void) {
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);
    Json::Value result = PlaylistEntryBase::GetMqttStatus();
    if (m_mediaEntry) {
        uint64_t l = m_mediaEntry->GetLengthInMS();
        uint64_t e = m_mediaEntry->GetElapsedMS();
        result["secondsRemaining"] = (int)((l - e) / 1000);
        result["secondsTotal"] = (int)(l / 1000);
        result["secondsElapsed"] = (int)(e / 1000);
        result["mediaName"] = m_mediaEntry->GetMediaName();
    }
    if (m_sequenceEntry) {
        Json::Value se = m_sequenceEntry->GetMqttStatus();
        result["sequenceName"] = se["sequenceName"];
        result["secondsRemaining"] = se["secondsRemaining"];
        result["secondsTotal"] = se["secondsTotal"];
        result["secondsElapsed"] = se["secondsElapsed"];
    }

    result["mediaTitle"] = MediaDetails::INSTANCE.title;
    result["mediaArtist"] = MediaDetails::INSTANCE.artist;

    return result;
}

void PlaylistEntryBoth::Pause() {
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);
    if (m_mediaEntry)
        m_mediaEntry->Pause();
    m_sequenceEntry->Pause();
}
bool PlaylistEntryBoth::IsPaused() {
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);
    if (m_mediaEntry) {
        return m_mediaEntry->IsPaused();
    }
    return m_sequenceEntry->IsPaused();
}
void PlaylistEntryBoth::Resume() {
    std::unique_lock<std::recursive_mutex> seqLock(m_mutex);
    if (m_mediaEntry)
        m_mediaEntry->Resume();
    m_sequenceEntry->Resume();
}
