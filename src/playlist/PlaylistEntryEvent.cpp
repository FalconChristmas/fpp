/*
 *   Playlist Entry Event Class for Falcon Player (FPP)
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

#include <stdio.h>

#include "events.h"
#include "log.h"
#include "PlaylistEntryEvent.h"

/*
 *
 */
PlaylistEntryEvent::PlaylistEntryEvent()
  : m_majorID(0),
	m_minorID(0),
	m_blocking(0)
{
	m_type = "event";
}

/*
 *
 */
PlaylistEntryEvent::~PlaylistEntryEvent()
{
}

/*
 *
 */
int PlaylistEntryEvent::Init(Json::Value &config)
{
	if (config["majorID"].asInt())
		m_majorID = config["majorID"].asInt();

	if (config["minorID"].asInt())
		m_minorID = config["minorID"].asInt();

	if (config["blocking"].asInt())
		m_blocking = config["blocking"].asInt();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryEvent::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryEffect::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	if (TriggerEvent((unsigned char)m_majorID, (unsigned char)m_minorID))
		return PlaylistEntryBase::StartPlaying();

	FinishPlay();
	return 0;
}

/*
 *
 */
int PlaylistEntryEvent::Process(void)
{
	if (!m_blocking)
	{
		FinishPlay();
		return PlaylistEntryBase::Process();
	}

	// FIXME, check for blocking here
	PlaylistEntryBase::Process();
	return 0;

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryEvent::Stop(void)
{
	if (!m_blocking)
	{
		FinishPlay();
		return PlaylistEntryBase::Stop();
	}

	// FIXME PLAYLIST
	// FIXME, check for blocking here
	PlaylistEntryBase::Stop();
	return 0;

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryEvent::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Major ID: %d\n", m_majorID);
	LogDebug(VB_PLAYLIST, "Minor ID: %d\n", m_minorID);
	LogDebug(VB_PLAYLIST, "Blocking: %d\n", m_blocking);
}

