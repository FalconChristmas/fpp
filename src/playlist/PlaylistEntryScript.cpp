/*
 *   Playlist Entry Script Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2016 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include <stdlib.h>
#include <string.h>

#include "controlsend.h"
#include "log.h"
#include "mpg123.h"
#include "ogg123.h"
#include "omxplayer.h"
#include "PlaylistEntryScript.h"
#include "settings.h"

/*
 *
 */
PlaylistEntryScript::PlaylistEntryScript()
  : m_blocking(0)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryScript::PlaylistEntryScript()\n");

	m_type = "script";
}

/*
 *
 */
PlaylistEntryScript::~PlaylistEntryScript()
{
}

/*
 *
 */
int PlaylistEntryScript::Init(Json::Value &config)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryScript::Init()\n");

	if (!config.isMember("scriptName"))
	{
		LogErr(VB_PLAYLIST, "Missing scriptName entry\n");
		return 0;
	}

	m_scriptFilename = config["scriptName"].asString();
	m_blocking = config["blocking"].asInt();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryScript::StartPlaying(void)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryScript::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	if (m_blocking)
	{
		// FIXME PLAYLIST
		// spawn in foreground

		// do we need this if we have the sigchild handler????
		//FinishPlay();
	}
	else
	{
		// FIXME PLAYLIST
		// spawn in background
	}

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryScript::Stop(void)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryScript::Stop()\n");

	// FIXME PLAYLIST, kill the child if we are in non-blocking mode

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
int PlaylistEntryScript::HandleSigChild(pid_t pid)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryScript::HandleSigChild(%d)\n", pid);

	FinishPlay();

	return 1;
}

/*
 *
 */
void PlaylistEntryScript::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Script Filename: %s\n", m_scriptFilename.c_str());
	LogDebug(VB_PLAYLIST, "Blocking       : %d\n", m_blocking);
}

/*
 *
 */
Json::Value PlaylistEntryScript::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["scriptFilename"]      = m_scriptFilename;
	result["blocking"]            = m_blocking;

	return result;
}
