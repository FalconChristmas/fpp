/*
 *   Playlist Entry Script Class for Falcon Player (FPP)
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "common.h"
#include "log.h"
#include "PlaylistEntryScript.h"
#include "scripts.h"
#include "settings.h"

/*
 *
 */
PlaylistEntryScript::PlaylistEntryScript(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_blocking(0), m_scriptProcess(0)
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
	m_scriptArgs = config["scriptArgs"].asString();
	m_blocking = config["blocking"].asBool();

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
int PlaylistEntryScript::Process(void)
{
    if (m_scriptProcess && !isChildRunning()) {
        m_scriptProcess = 0;
        FinishPlay();
    }
    return PlaylistEntryBase::Process();
}
/*
 *
 */
int PlaylistEntryScript::Stop(void)
{
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

    long long t = GetTime();
    t -= m_startTime;
    t /= 1000;
    t /= 1000;
	result["scriptFilename"]   = m_scriptFilename;
	result["blocking"]         = m_blocking;
	result["secondsElapsed"]   = (Json::UInt64)t;

	return result;
}

Json::Value PlaylistEntryScript::GetMqttStatus(void)
{
    Json::Value result = PlaylistEntryBase::GetMqttStatus();
    
    long long t = GetTime();
    t -= m_startTime;
    t /= 1000;
    t /= 1000;
    result["scriptFilename"]   = m_scriptFilename;
    result["blocking"]         = m_blocking;
    result["secondsElapsed"]   = (Json::UInt64)t;
    
    return result;
}
