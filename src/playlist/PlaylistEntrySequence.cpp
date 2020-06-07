/*
 *   Playlist Entry Sequence Class for Falcon Player (FPP)
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

#include "fpp-pch.h"
#include "PlaylistEntrySequence.h"
#include "fseq/FSEQFile.h"

#include "channeloutput/channeloutputthread.h"

PlaylistEntrySequence::PlaylistEntrySequence(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_duration(0),
    m_prepared(false),
    m_adjustTiming(true),
    m_pausedFrame(-1)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::PlaylistEntrySequence()\n");

	m_type = "sequence";
}

PlaylistEntrySequence::~PlaylistEntrySequence()
{
}

/*
 *
 */
int PlaylistEntrySequence::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::Init()\n");

	if (!config.isMember("sequenceName"))
	{
		LogErr(VB_PLAYLIST, "Missing sequenceName entry\n");
		return 0;
	}

	m_sequenceName = config["sequenceName"].asString();
    m_pausedFrame = -1;
	return PlaylistEntryBase::Init(config);
}


int PlaylistEntrySequence::PreparePlay(int frame) {
    if (sequence->OpenSequenceFile(m_sequenceName, frame) <= 0) {
        LogErr(VB_PLAYLIST, "Error opening sequence %s\n", m_sequenceName.c_str());
        return 0;
    }
    m_prepared = true;
    m_duration = sequence->m_seqMSDuration;
    m_sequenceFrameTime = sequence->GetSeqStepTime();
    return 1;
}

/*
 *
 */
int PlaylistEntrySequence::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::StartPlaying()\n");

	if (!CanPlay()) {
        m_prepared = false;
		FinishPlay();
		return 0;
	}
    
    if (!m_prepared) {
        PreparePlay();
    }
    m_pausedFrame = -1;
    sequence->StartSequence();
    m_startTme = GetTimeMS();
    LogDebug(VB_PLAYLIST, "Started Sequence, ID: %s\n", m_sequenceName.c_str());

	if (mqtt)
		mqtt->Publish("playlist/sequence/status", m_sequenceName);

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntrySequence::Process(void)
{
	if (!sequence->IsSequenceRunning()) {
		FinishPlay();
        m_prepared = false;

		if (mqtt)
			mqtt->Publish("playlist/sequence/status", "");
    } else if (m_adjustTiming) {
        long long now = GetTimeMS();
        int total = (now - m_startTme);
        int frame = total / sequence->GetSeqStepTime();
        CalculateNewChannelOutputDelayForFrame(frame);
    }

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntrySequence::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::Stop()\n");
    
	sequence->CloseSequenceFile();
    m_prepared = false;
	if (mqtt)
		mqtt->Publish("playlist/sequence/status", "");

	return PlaylistEntryBase::Stop();
}

uint64_t PlaylistEntrySequence::GetLengthInMS() {
    if (m_duration == 0) {
        std::string n = getSequenceDirectory();
        n += "/";
        n += m_sequenceName;
        if (FileExists(n)) {
            FSEQFile* fs = FSEQFile::openFSEQFile(n);
            m_duration = fs->getTotalTimeMS();
            delete fs;
        }
    }
    return m_duration;
}
uint64_t PlaylistEntrySequence::GetElapsedMS() {
    if (m_prepared) {
        return sequence->m_seqMSElapsed;
    }
    return 0;
}


/*
 *
 */
void PlaylistEntrySequence::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Sequence Filename: %s\n", m_sequenceName.c_str());
}

/*
 *
 */
Json::Value PlaylistEntrySequence::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["sequenceName"]     = m_sequenceName;
    if (IsPaused()) {
        int pos = m_pausedFrame * m_sequenceFrameTime;
        result["secondsElapsed"]   = pos / 1000;
        result["secondsRemaining"] = (m_duration - pos) / 1000;
    } else {
        result["secondsElapsed"]   = sequence->m_seqMSElapsed / 1000;
        result["secondsRemaining"] = sequence->m_seqMSRemaining / 1000;
    }

	return result;
}
Json::Value PlaylistEntrySequence::GetMqttStatus(void) {
	Json::Value result = PlaylistEntryBase::GetMqttStatus();
	result["sequenceName"]     = m_sequenceName;
    if (IsPaused()) {
        int pos = m_pausedFrame * m_sequenceFrameTime;
        result["secondsElapsed"]   = pos / 1000;
        result["secondsRemaining"] = (m_duration - pos) / 1000;
        result["secondsTotal"] = m_duration / 1000;
    } else {
        result["secondsElapsed"]   = sequence->m_seqMSElapsed / 1000;
        result["secondsRemaining"] = sequence->m_seqMSRemaining / 1000;
        result["secondsTotal"] = sequence->m_seqMSDuration / 1000;
    }
	return result;
}

void PlaylistEntrySequence::Pause() {
    m_pausedFrame = sequence->m_seqMSElapsed / sequence->GetSeqStepTime();
    sequence->CloseSequenceFile();
    sequence->SendBlankingData();
    m_prepared = false;
}
bool PlaylistEntrySequence::IsPaused() {
    return m_pausedFrame != -1;
}
void PlaylistEntrySequence::Resume() {
    if (m_pausedFrame >= 0) {
        PreparePlay(m_pausedFrame);
        sequence->StartSequence();
        m_startTme = GetTimeMS() - m_pausedFrame*sequence->GetSeqStepTime();
        LogDebug(VB_PLAYLIST, "Started Sequence, ID: %s\n", m_sequenceName.c_str());
        m_pausedFrame = -1;
        if (mqtt) {
            mqtt->Publish("playlist/sequence/status", m_sequenceName);
        }
    }
}
