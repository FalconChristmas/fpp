/*
 *   Playlist Entry Both Class for Falcon Player (FPP)
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

#include "log.h"
#include "PlaylistEntryBoth.h"

PlaylistEntryBoth::PlaylistEntryBoth(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_duration(0),
	m_mediaEntry(NULL),
	m_sequenceEntry(NULL)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBoth::PlaylistEntryBoth()\n");

	m_type = "both";
}

PlaylistEntryBoth::~PlaylistEntryBoth()
{
}

/*
 *
 */
int PlaylistEntryBoth::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBoth::Init()\n");

	m_sequenceEntry = new PlaylistEntrySequence(this);
	if (!m_sequenceEntry)
		return 0;

	m_mediaEntry = new PlaylistEntryMedia(this);
	if (!m_mediaEntry)
		return 0;

	if (!m_mediaEntry->Init(config))
		return 0;

	if (!m_sequenceEntry->Init(config))
		return 0;

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryBoth::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBoth::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	if (!m_mediaEntry->StartPlaying())
		return 0;

	if (!m_sequenceEntry->StartPlaying())
	{
		m_mediaEntry->Stop();
		return 0;
	}

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryBoth::Process(void)
{
	m_mediaEntry->Process();
	m_sequenceEntry->Process();

	if (m_mediaEntry->IsFinished())
	{
		FinishPlay();
		m_sequenceEntry->Stop();
	}

	if (m_sequenceEntry->IsFinished())
	{
		FinishPlay();
		m_mediaEntry->Stop();
	}

	// FIXME PLAYLIST, handle sync in here somehow

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryBoth::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBoth::Stop()\n");

	m_mediaEntry->Stop();
	m_sequenceEntry->Stop();

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryBoth::Dump(void)
{
	PlaylistEntryBase::Dump();

	m_mediaEntry->Dump();
	m_sequenceEntry->Dump();
}

/*
 *
 */
Json::Value PlaylistEntryBoth::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["media"] = m_mediaEntry->GetConfig();
	result["sequence"] = m_sequenceEntry->GetConfig();

	return result;
}

