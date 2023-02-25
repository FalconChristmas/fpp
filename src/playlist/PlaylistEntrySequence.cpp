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

#include "PlaylistEntrySequence.h"
#include "fseq/FSEQFile.h"

#include "channeloutput/ChannelOutputSetup.h"
#include "channeloutput/channeloutputthread.h"

#include "Events.h"

PlaylistEntrySequence::PlaylistEntrySequence(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_duration(0),
    m_prepared(false),
    m_adjustTiming(true),
    m_pausedFrame(-1) {
    LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::PlaylistEntrySequence()\n");

    m_type = "sequence";
}

PlaylistEntrySequence::~PlaylistEntrySequence() {
}

/*
 *
 */
int PlaylistEntrySequence::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::Init()\n");

    if (!config.isMember("sequenceName")) {
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
int PlaylistEntrySequence::StartPlaying(void) {
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
    ResetChannelOutputFrameNumber();
    sequence->StartSequence();
    m_startTme = GetTimeMS();
    LogDebug(VB_PLAYLIST, "Started Sequence, ID: %s\n", m_sequenceName.c_str());

    Events::Publish("playlist/sequence/status", m_sequenceName);
    if (m_duration > 0) {
        Events::Publish("playlist/sequence/secondsTotal", m_duration / 1000);
    } else {
        Events::Publish("playlist/sequence/secondsTotal", "");
    }

    return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntrySequence::Process(void) {
    if (!sequence->IsSequenceRunning()) {
        FinishPlay();
        m_prepared = false;

        Events::Publish("playlist/sequence/status", "");
        Events::Publish("playlist/sequence/secondsTotal", "");
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
int PlaylistEntrySequence::Stop(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntrySequence::Stop()\n");

    sequence->CloseSequenceFile();
    m_prepared = false;
    Events::Publish("playlist/sequence/status", "");
    Events::Publish("playlist/sequence/secondsTotal", "");

    return PlaylistEntryBase::Stop();
}

uint64_t PlaylistEntrySequence::GetLengthInMS() {
    if (m_duration == 0) {
        std::string n = FPP_DIR_SEQUENCE("/" + m_sequenceName);
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
void PlaylistEntrySequence::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Sequence Filename: %s\n", m_sequenceName.c_str());
}

/*
 *
 */
Json::Value PlaylistEntrySequence::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    result["sequenceName"] = m_sequenceName;
    if (IsPaused()) {
        int pos = m_pausedFrame * m_sequenceFrameTime;
        result["secondsElapsed"] = pos / 1000;
        result["secondsRemaining"] = (m_duration - pos) / 1000;
    } else {
        result["secondsElapsed"] = sequence->m_seqMSElapsed / 1000;
        result["secondsRemaining"] = sequence->m_seqMSRemaining / 1000;
    }

    return result;
}
Json::Value PlaylistEntrySequence::GetMqttStatus(void) {
    Json::Value result = PlaylistEntryBase::GetMqttStatus();
    result["sequenceName"] = m_sequenceName;
    if (IsPaused()) {
        int pos = m_pausedFrame * m_sequenceFrameTime;
        result["secondsElapsed"] = pos / 1000;
        result["secondsRemaining"] = (m_duration - pos) / 1000;
        result["secondsTotal"] = m_duration / 1000;
    } else {
        result["secondsElapsed"] = sequence->m_seqMSElapsed / 1000;
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
        m_startTme = GetTimeMS() - m_pausedFrame * sequence->GetSeqStepTime();
        LogDebug(VB_PLAYLIST, "Started Sequence, ID: %s\n", m_sequenceName.c_str());
        m_pausedFrame = -1;
        Events::Publish("playlist/sequence/status", m_sequenceName);
    }
}
