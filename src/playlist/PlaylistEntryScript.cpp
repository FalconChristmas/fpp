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

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "../common.h"
#include "../log.h"

#include "PlaylistEntryScript.h"
#include "scripts.h"

/*
 *
 */
PlaylistEntryScript::PlaylistEntryScript(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_blocking(0),
    m_scriptProcess(0) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryScript::PlaylistEntryScript()\n");

    m_type = "script";
}

/*
 *
 */
PlaylistEntryScript::~PlaylistEntryScript() {
}

/*
 *
 */
int PlaylistEntryScript::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryScript::Init()\n");

    if (!config.isMember("scriptName")) {
        LogErr(VB_PLAYLIST, "Missing scriptName entry\n");
        return 0;
    }

    m_scriptFilename = config["scriptName"].asString();
    m_scriptArgs = config["scriptArgs"].asString();
    m_blocking = config["blocking"].asBool();

    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryScript::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryScript::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    m_startTime = GetTime();
    PlaylistEntryBase::StartPlaying();
    m_scriptProcess = RunScript(m_scriptFilename, m_scriptArgs);
    if (!m_blocking) {
        m_scriptProcess = 0;
        FinishPlay();
        return 0;
    }

    return PlaylistEntryBase::StartPlaying();
}
int PlaylistEntryScript::Process(void) {
    if (m_scriptProcess && !isChildRunning()) {
        m_scriptProcess = 0;
        FinishPlay();
    }
    return PlaylistEntryBase::Process();
}
/*
 *
 */
int PlaylistEntryScript::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryScript::Stop()\n");

    if (m_scriptProcess) {
        kill(m_scriptProcess, SIGTERM);
        FinishPlay();
    }

    return PlaylistEntryBase::Stop();
}

bool PlaylistEntryScript::isChildRunning() {
    if (m_scriptProcess > 0) {
        int status = 0;
        if (waitpid(m_scriptProcess, &status, WNOHANG)) {
            return false;
        } else {
            return true;
        }
    }
    return false;
}

/*
 *
 */
void PlaylistEntryScript::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Script Filename: %s\n", m_scriptFilename.c_str());
    LogDebug(VB_PLAYLIST, "Blocking       : %d\n", m_blocking);
}

uint64_t PlaylistEntryScript::GetElapsedMS() {
    long long t = GetTime();
    t -= m_startTime;
    t /= 1000;
    return t;
}

/*
 *
 */
Json::Value PlaylistEntryScript::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    long long t = GetTime();
    t -= m_startTime;
    t /= 1000;
    t /= 1000;
    result["scriptFilename"] = m_scriptFilename;
    result["blocking"] = m_blocking;
    result["secondsElapsed"] = (Json::UInt64)t;

    return result;
}

Json::Value PlaylistEntryScript::GetMqttStatus(void) {
    Json::Value result = PlaylistEntryBase::GetMqttStatus();

    long long t = GetTime();
    t -= m_startTime;
    t /= 1000;
    t /= 1000;
    result["scriptFilename"] = m_scriptFilename;
    result["blocking"] = m_blocking;
    result["secondsElapsed"] = (Json::UInt64)t;

    return result;
}
