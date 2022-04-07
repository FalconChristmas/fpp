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

#include "PlaylistEntryMQTT.h"

/*
 *
 */
PlaylistEntryMQTT::PlaylistEntryMQTT(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMQTT::PlaylistEntryMQTT()\n");

    m_type = "mqtt";
    m_deprecated = 1;
}

/*
 *
 */
PlaylistEntryMQTT::~PlaylistEntryMQTT() {
}

/*
 *
 */
int PlaylistEntryMQTT::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMQTT::Init()\n");

    if (!config.isMember("topic")) {
        LogErr(VB_PLAYLIST, "Missing topic entry\n");
        return 0;
    }

    if (!config.isMember("message")) {
        LogErr(VB_PLAYLIST, "Missing message entry\n");
        return 0;
    }

    m_topic = config["topic"].asString();
    m_message = config["message"].asString();

    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryMQTT::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryMQTT::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    PlaylistEntryBase::StartPlaying();

    if (mqtt)
        mqtt->PublishRaw(m_topic.c_str(), m_message.c_str());

    FinishPlay();

    return 1;
}

/*
 *
 */
void PlaylistEntryMQTT::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Topic  : %s\n", m_topic.c_str());
    LogDebug(VB_PLAYLIST, "Message: %s\n", m_message.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryMQTT::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    result["topic"] = m_topic;
    result["message"] = m_message;

    return result;
}
