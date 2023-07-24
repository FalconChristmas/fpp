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

#include "PlaylistEntryPause.h"

/*
 *
 */
PlaylistEntryPause::PlaylistEntryPause(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_duration(0.0f),
    m_startTime(0),
    m_endTime(0),
    m_pausedRemaining(0) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryPause::PlaylistEntryPause()\n");

    m_type = "pause";
}

/*
 *
 */
PlaylistEntryPause::~PlaylistEntryPause() {
}

/*
 *
 */
int PlaylistEntryPause::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryPause::Init()\n");

    m_duration = config["duration"].asFloat();
    m_endTime = 0;
    m_finishTime = 0;
    m_pausedRemaining = 0;
    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryPause::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryPause::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }
    m_pausedRemaining = 0;
    // Calculate end time as m_duation number of seconds from now
    m_startTime = GetTimeMS();

    double tmp = m_duration;
    tmp *= 1000.0;
    m_endTime = m_startTime + tmp;

    return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryPause::Process(void) {
    if (!m_isStarted || !m_isPlaying || m_isFinished) {
        return 0;
    }

    long long now = GetTimeMS();
    if (m_isStarted && m_isPlaying && (now >= m_endTime)) {
        m_finishTime = GetTimeMS();
        FinishPlay();
    }

    return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryPause::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryPause::Stop()\n");

    m_finishTime = GetTimeMS();
    FinishPlay();
    m_pausedRemaining = 0;
    return PlaylistEntryBase::Stop();
}

uint64_t PlaylistEntryPause::GetLengthInMS() {
    float f = m_duration; //duration is in seconds
    f *= 1000;
    return f;
}
uint64_t PlaylistEntryPause::GetElapsedMS() {
    long long now = GetTimeMS();
    if (m_isStarted && m_isPlaying) {
        return now - m_startTime;
    }
    return 0;
}

/*
 *
 */
void PlaylistEntryPause::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Duration:    %f\n", m_duration);
    LogDebug(VB_PLAYLIST, "Cur Time:    %lld\n", GetTimeMS());
    LogDebug(VB_PLAYLIST, "Start Time:  %lld\n", m_startTime);
    LogDebug(VB_PLAYLIST, "End Time:    %lld\n", m_endTime);
    LogDebug(VB_PLAYLIST, "Finish Time: %lld\n", m_finishTime);
}

/*
 *
 */
Json::Value PlaylistEntryPause::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    long long now = GetTimeMS();
    if (IsPaused()) {
        m_endTime = now + m_pausedRemaining;
        float d = m_duration * 1000;
        m_startTime = m_endTime - d;
    }

    result["duration"] = m_duration;
    result["startTime"] = (Json::UInt64)m_startTime;
    result["endTime"] = (Json::UInt64)m_endTime;
    result["finishTime"] = (Json::UInt64)m_finishTime;

    if (m_isPlaying)
        result["remaining"] = (Json::UInt64)((m_endTime - now) / 1000);
    else
        result["remaining"] = (Json::UInt64)0;

    return result;
}

void PlaylistEntryPause::Pause() {
    long long now = GetTimeMS();
    m_pausedRemaining = m_endTime - now;
}
bool PlaylistEntryPause::IsPaused() {
    return m_pausedRemaining != 0;
}
void PlaylistEntryPause::Resume() {
    long long now = GetTimeMS();
    m_endTime = now + m_pausedRemaining;
    m_pausedRemaining = 0;
}
