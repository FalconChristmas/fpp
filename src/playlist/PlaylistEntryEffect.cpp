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

#include "../log.h"

#include "PlaylistEntryEffect.h"
#include "effects.h"

/*
 *
 */
PlaylistEntryEffect::PlaylistEntryEffect(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_startChannel(0),
    m_repeat(0),
    m_blocking(0),
    m_effectID(-1) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::PlaylistEntryEffect()\n");

    m_type = "effect";
}

/*
 *
 */
PlaylistEntryEffect::~PlaylistEntryEffect() {
}

/*
 *
 */
int PlaylistEntryEffect::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::Init()\n");

    if (!config.isMember("mediaFilename")) {
        LogDebug(VB_PLAYLIST, "Missing effectName entry\n");
        return 0;
    }

    m_effectName = config["effectName"].asString();

    if (config["startChannel"].asInt())
        m_startChannel = config["startChannel"].asInt();

    if (config["repeat"].asInt())
        m_repeat = config["repeat"].asInt();

    if (config["blocking"].asInt())
        m_blocking = config["blocking"].asInt();

    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryEffect::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    m_effectID = StartEffect(m_effectName.c_str(), m_startChannel, m_repeat);

    if (m_effectID == -1) {
        FinishPlay();
        return 0;
    }

    if (!m_blocking)
        m_effectID = -1;

    return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryEffect::Process(void) {
    int result = 0;

    if (!m_blocking) {
        FinishPlay();
        return 1;
    }

    // FIXME
    //	result = IsEffectRunning(m_effectID);

    if (!result) {
        FinishPlay();
        m_effectID = -1;
    }

    return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryEffect::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::Stop()\n");

    if (!m_isPlaying)
        return 1;

    if (m_effectID == -1)
        return 1;

    // FIXME
    //	if (IsEffectRunning(m_effectID))
    //		StopEffect(m_effectID);

    return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryEffect::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Effect Name  : %s\n", m_effectName.c_str());
    LogDebug(VB_PLAYLIST, "Start Channel: %d\n", m_startChannel);
    LogDebug(VB_PLAYLIST, "Repeat       : %d\n", m_repeat);
    LogDebug(VB_PLAYLIST, "Blocking     : %d\n", m_blocking);
}

/*
 *
 */
Json::Value PlaylistEntryEffect::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    result["effectName"] = m_effectName;
    result["startChannel"] = m_startChannel;
    result["repeat"] = m_repeat;
    result["blocking"] = m_blocking;

    return result;
}
