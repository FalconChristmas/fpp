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

#include "PlaylistEntryBase.h"

/*
 *
 */
PlaylistEntryBase::PlaylistEntryBase(Playlist* playlist, PlaylistEntryBase* parent) :
    m_enabled(1),
    m_isStarted(0),
    m_isPlaying(0),
    m_isFinished(0),
    m_playOnce(0),
    m_playCount(0),
    m_isPrepped(0),
    m_deprecated(0),
    m_parent(parent),
    m_parentPlaylist(playlist),
    m_playlistPosition(-1) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBase::PlaylistEntryBase()\n");
    m_type = "base";
}

/*
 *
 */
PlaylistEntryBase::~PlaylistEntryBase() {
}

/*
 *
 */
int PlaylistEntryBase::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBase::Init(): '%s'\n",
             config["type"].asString().c_str());

    if (config.isMember("enabled"))
        m_enabled = config["enabled"].asInt();

    if (config.isMember("note"))
        m_note = config["note"].asString();

    m_isStarted = 0;
    m_isPlaying = 0;
    m_isFinished = 0;
    m_config = config;

    if (config.isMember("timecode")) {
        std::string v = config["timecode"].asString();
        if (v == "Default") {
            m_timeCode = -1;
        } else {
            try {
                std::vector<std::string> sparts = split(v, ':');
                if (sparts.size() == 2) {
                    m_timeCode = std::stoi(sparts[0]) * 60 * 60;
                    m_timeCode += std::stoi(sparts[1]) * 60;
                } else if (sparts.size() == 3) {
                    m_timeCode = std::stoi(sparts[0]) * 60 * 60;
                    m_timeCode += std::stoi(sparts[1]) * 60;
                    m_timeCode += std::stoi(sparts[2]);
                }
                m_timeCode *= 1000; // seconds -> milliseconds
            } catch (...) {
                LogWarn(VB_PLAYLIST, "%s not a valid time code\n", v.c_str());
                m_timeCode = -1;
            }
        }
    }
    return 1;
}

/*
 *
 */
int PlaylistEntryBase::CanPlay(void) {
    if (m_playOnce && (m_playCount > 0)) {
        LogDebug(VB_PLAYLIST, "%s item exceeds play count\n", m_type.c_str());
        return 0;
    }

    if (!m_enabled) {
        LogDebug(VB_PLAYLIST, "%s item disabled\n", m_type.c_str());
        return 0;
    }

    return 1;
}

/*
 *
 */
int PlaylistEntryBase::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBase::StartPlaying()\n");

    m_isStarted = 1;
    m_isPlaying = 1;
    m_isFinished = 0;
    m_playCount++;

    if (WillLog(LOG_DEBUG, VB_PLAYLIST))
        Dump();

    return 1;
}

/*
 *
 */
void PlaylistEntryBase::FinishPlay(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBase::FinishPlay()\n");

    m_isStarted = 1;
    m_isPlaying = 0;
    m_isFinished = 1;
}

/*
 *
 */
int PlaylistEntryBase::IsStarted(void) {
    return m_isStarted;
}

/*
 *
 */
int PlaylistEntryBase::IsPlaying(void) {
    if (m_isStarted && m_isPlaying)
        return 1;

    return 0;
}

/*
 *
 */
int PlaylistEntryBase::IsFinished(void) {
    if (m_isStarted && !m_isPlaying && m_isFinished)
        return 1;

    return 0;
}

/*
 *
 */
int PlaylistEntryBase::Prep(void) {
    m_isPrepped = 1;

    return 1;
}

/*
 *
 */
int PlaylistEntryBase::Process(void) {
    return 1;
}

/*
 *
 */
int PlaylistEntryBase::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBase::Stop()\n");

    FinishPlay();

    return 1;
}

/*
 *
 */
void PlaylistEntryBase::Dump(void) {
    LogDebug(VB_PLAYLIST, "---- Playlist Entry ----\n");
    LogDebug(VB_PLAYLIST, "Entry Type: %s\n", m_type.c_str());
    LogDebug(VB_PLAYLIST, "Entry Note: %s\n", m_note.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryBase::GetMqttStatus(void) {
    Json::Value result;
    result["type"] = m_type;
    result["playOnce"] = m_playOnce;

    return result;
}

Json::Value PlaylistEntryBase::GetConfig(void) {
    Json::Value result = m_config;

    result["type"] = m_type;
    result["note"] = m_note;
    result["enabled"] = m_enabled;
    result["isStarted"] = m_isStarted;
    result["isPlaying"] = m_isPlaying;
    result["isFinished"] = m_isFinished;
    result["playOnce"] = m_playOnce;
    result["playCount"] = m_playCount;
    result["deprecated"] = m_deprecated;

    return result;
}

/*
 *
 */
std::string PlaylistEntryBase::ReplaceMatches(std::string in) {
    std::string out = in;

    LogDebug(VB_PLAYLIST, "In: '%s'\n", in.c_str());

    replaceAll(out, "%t", m_type);

    LogDebug(VB_PLAYLIST, "Out: '%s'\n", out.c_str());

    return out;
}
