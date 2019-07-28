/*
 *   Playlist Entry Both Class for Falcon Player (FPP)
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

#include "log.h"
#include "PlaylistEntryBoth.h"
#include "mediadetails.h"
#include "Sequence.h"

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
    

    if (!m_mediaEntry->Init(config)) {
        delete m_mediaEntry;
        m_mediaEntry = nullptr;
    }
    m_mediaName = m_mediaEntry->GetMediaName();

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

    if (m_mediaEntry && !m_mediaEntry->PreparePlay()) {
        delete m_mediaEntry;
		m_mediaEntry = nullptr;
    }
    if (!m_mediaEntry) {
        LogDebug(VB_PLAYLIST, "Skipping media playlist entry, likely blacklisted audio: %s\n", m_mediaName.c_str());
    }

	if (!m_sequenceEntry->StartPlaying())
	{
        if (m_mediaEntry) {
            m_mediaEntry->Stop();
        }
		return 0;
	}
    if (m_mediaEntry && !m_mediaEntry->StartPlaying()) {
        delete m_mediaEntry;
        m_mediaEntry = nullptr;
        m_sequenceEntry->Stop();
        return 0;
    }

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryBoth::Process(void)
{
	if (m_mediaEntry) m_mediaEntry->Process();
	m_sequenceEntry->Process();

	if (m_mediaEntry && m_mediaEntry->IsFinished())
	{
		FinishPlay();
		m_sequenceEntry->Stop();
	}

	if (m_sequenceEntry->IsFinished())
	{
		FinishPlay();
		if (m_mediaEntry) m_mediaEntry->Stop();
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

	if (m_mediaEntry) m_mediaEntry->Stop();
	m_sequenceEntry->Stop();

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryBoth::Dump(void)
{
	PlaylistEntryBase::Dump();

	if (m_mediaEntry) m_mediaEntry->Dump();
	m_sequenceEntry->Dump();
}

/*
 *
 */
Json::Value PlaylistEntryBoth::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

    if (m_mediaEntry) {
        result["media"] = m_mediaEntry->GetConfig();
    } else {
        //fake it so the display will display the times
        //where the sequence is
        result["media"] = m_sequenceEntry->GetConfig();
    }
	result["sequence"] = m_sequenceEntry->GetConfig();

	return result;
}

Json::Value PlaylistEntryBoth::GetMqttStatus(void)
{
	Json::Value result = PlaylistEntryBase::GetMqttStatus();
    	if (m_mediaEntry) {
        	result["secondsRemaining"] = m_mediaEntry->m_secondsRemaining;
        	result["secondsTotal"] = m_mediaEntry->m_secondsTotal;
        	result["secondsElapsed"] = m_mediaEntry->m_secondsElapsed;
		result["mediaName"] = m_mediaEntry->GetMediaName();

	}
	if (m_sequenceEntry) {
		result["sequenceName"]     = m_sequenceEntry->GetSequenceName();
        	result["secondsRemaining"] = sequence->m_seqSecondsRemaining;
        	result["secondsTotal"] = sequence->m_seqDuration;
        	result["secondsElapsed"] = sequence->m_seqSecondsElapsed;
	}

    result["mediaTitle"] = MediaDetails::INSTANCE.title;
    result["mediaArtist"] = MediaDetails::INSTANCE.artist;


	return result;
}
