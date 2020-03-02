/*
 *   Playlist Entry Event Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include "log.h"
#include "PlaylistEntryCommand.h"

/*
 *
 */
PlaylistEntryCommand::PlaylistEntryCommand(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryCommand::PlaylistEntryCommand()\n");

	m_type = "command";
}

/*
 *
 */
PlaylistEntryCommand::~PlaylistEntryCommand()
{
}

/*
 *
 */
int PlaylistEntryCommand::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryCommand::Init()\n");
    m_command = config;
	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryCommand::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryCommand::StartPlaying()\n");

	if (!CanPlay())
	{
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
int PlaylistEntryCommand::Process(void)
{
    LogDebug(VB_PLAYLIST, "Process()\n");
    if (m_result->isDone()) {
        FinishPlay();
    }
	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryCommand::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryCommand::Stop()\n");

    FinishPlay();
    return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryCommand::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Command: %s\n", m_command["command"].asString().c_str());
    for (int x = 0; x < m_command["args"].size(); x++) {
        LogDebug(VB_PLAYLIST, "  \"%s\"\n", m_command["args"][x].asString().c_str());
    }
}

/*
 *
 */
Json::Value PlaylistEntryCommand::GetConfig(void)
{
	return m_command;
}

