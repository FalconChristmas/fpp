/*
 *   Playlist Entry Base Class for Falcon Player (FPP)
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

#include "common.h"
#include "log.h"
#include "PlaylistEntryBase.h"

/*
 *
 */
PlaylistEntryBase::PlaylistEntryBase(PlaylistEntryBase *parent)
  : m_enabled(1),
	m_isStarted(0),
	m_isPlaying(0),
	m_isFinished(0),
	m_playOnce(0),
	m_playCount(0),
	m_isPrepped(0),
	m_deprecated(0),
	m_parent(parent)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryBase::PlaylistEntryBase()\n");
	m_type = "base";
}

/*
 *
 */
PlaylistEntryBase::~PlaylistEntryBase()
{
}

/*
 *
 */
int PlaylistEntryBase::Init(Json::Value &config)
{
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

	return 1;
}

/*
 *
 */
int PlaylistEntryBase::CanPlay(void)
{
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
int PlaylistEntryBase::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBase::StartPlaying()\n");

	m_isStarted = 1;
	m_isPlaying = 1;
	m_isFinished = 0;
	m_playCount++;

	if ((logLevel & LOG_DEBUG) && (logMask & VB_PLAYLIST))
		Dump();

	return 1;
}

/*
 *
 */
void PlaylistEntryBase::FinishPlay(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBase::FinishPlay()\n");
	
	m_isStarted = 1;
	m_isPlaying = 0;
	m_isFinished = 1;
}

/*
 *
 */
int PlaylistEntryBase::IsStarted(void)
{
	return m_isStarted;
}

/*
 *
 */
int PlaylistEntryBase::IsPlaying(void)
{
	if (m_isStarted && m_isPlaying)
		return 1;

	return 0;
}

/*
 *
 */
int PlaylistEntryBase::IsFinished(void)
{
	if (m_isStarted && !m_isPlaying && m_isFinished)
		return 1;

	return 0;
}

/*
 *
 */
int PlaylistEntryBase::Prep(void)
{
	m_isPrepped = 1;

	return 1;
}

/*
 *
 */
int PlaylistEntryBase::Process(void)
{
	return 1;
}

/*
 *
 */
int PlaylistEntryBase::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBase::Stop()\n");

	FinishPlay();

	return 1;
}

/*
 *
 */
int PlaylistEntryBase::HandleSigChild(pid_t pid)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBase::HandleSigChild()\n");

	return 0;
}

/*
 *
 */
void PlaylistEntryBase::Dump(void)
{
	LogDebug(VB_PLAYLIST, "---- Playlist Entry ----\n");
	LogDebug(VB_PLAYLIST, "Entry Type: %s\n", m_type.c_str());
	LogDebug(VB_PLAYLIST, "Entry Note: %s\n", m_note.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryBase::GetMqttStatus(void)
{
	Json::Value result;
	result["type"]       = m_type;
	result["playOnce"]   = m_playOnce;

	return result;
}

Json::Value PlaylistEntryBase::GetConfig(void)
{
	Json::Value result = m_config;

	result["type"]       = m_type;
	result["enabled"]    = m_enabled;
	result["isStarted"]  = m_isStarted;
	result["isPlaying"]  = m_isPlaying;
	result["isFinished"] = m_isFinished;
	result["playOnce"]   = m_playOnce;
	result["playCount"]  = m_playCount;
	result["deprecated"] = m_deprecated;

	return result;
}

/*
 *
 */
std::string PlaylistEntryBase::ReplaceMatches(std::string in)
{
	std::string out = in;

	LogDebug(VB_PLAYLIST, "In: '%s'\n", in.c_str());

	replaceAll(out, "%t", m_type);

	LogDebug(VB_PLAYLIST, "Out: '%s'\n", out.c_str());

	return out;
}

