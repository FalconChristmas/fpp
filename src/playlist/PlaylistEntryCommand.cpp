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

#include "PlaylistEntryCommand.h"

/*
 *
 */
PlaylistEntryCommand::PlaylistEntryCommand(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryCommand::PlaylistEntryCommand()\n");

    m_type = "command";
}

/*
 *
 */
PlaylistEntryCommand::~PlaylistEntryCommand() {
}

/*
 *
 */
int PlaylistEntryCommand::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryCommand::Init()\n");
    m_command = config;
    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryCommand::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryCommand::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    m_result = CommandManager::INSTANCE.run(m_command);
    if (m_result->isDone() && m_result->isError()) {
        // failed, so mark finished
        FinishPlay();
        return 0;
    }
    return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryCommand::Process(void) {
    LogDebug(VB_PLAYLIST, "Process()\n");
    if (m_result->isDone()) {
        FinishPlay();
    }
    return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryCommand::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryCommand::Stop()\n");

    FinishPlay();
    return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryCommand::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Command: %s\n", m_command["command"].asString().c_str());
    for (int x = 0; x < m_command["args"].size(); x++) {
        LogDebug(VB_PLAYLIST, "  \"%s\"\n", m_command["args"][x].asString().c_str());
    }
}

/*
 *
 */
Json::Value PlaylistEntryCommand::GetConfig(void) {
    return m_command;
}
