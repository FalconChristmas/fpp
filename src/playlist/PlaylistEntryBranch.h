#pragma once
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

#include <string>

#include "Playlist.h"
#include "PlaylistEntryBase.h"

class PlaylistEntryBranch : public PlaylistEntryBase {
public:
    PlaylistEntryBranch(Playlist* playlist, PlaylistEntryBase* parent = NULL);
    virtual ~PlaylistEntryBranch();

    virtual int Init(Json::Value& config) override;

    virtual int StartPlaying(void) override;
    virtual int Process(void) override;

    void SetNext(int isTrue);

    virtual void Dump(void) override;

    virtual Json::Value GetConfig(void) override;

    virtual PlaylistBranchType GetNextBranchType() override { return m_nextBranchType; }
    virtual std::string GetNextSection(void) override { return m_nextSection; }
    virtual int GetNextItem(void) override { return m_nextItem; }
    virtual std::string GetNextData(void) override { return m_nextBranchPlaylist; }

private:
    std::string m_branchTest;

    // Time comparison
    std::string m_startTime;
    std::string m_endTime;
    int m_sHour;
    int m_sMinute;
    int m_sSecond;
    int m_sDaySecond;
    int m_sHourSecond;
    int m_eHour;
    int m_eMinute;
    int m_eSecond;
    int m_eDaySecond;
    int m_eHourSecond;

    // Loop Number
    int m_iterationStart;
    int m_iterationCount;

    // MQTT Topic Message
    std::string m_mqttTopic;
    std::string m_mqttMessage;

    PlaylistBranchType m_trueNextBranchType;
    std::string m_trueNextSection;
    int m_trueNextItem;
    std::string m_trueBranchPlaylist;

    PlaylistBranchType m_falseNextBranchType;
    std::string m_falseNextSection;
    int m_falseNextItem;
    std::string m_falseBranchPlaylist;

    PlaylistBranchType m_nextBranchType;
    std::string m_nextSection;
    int m_nextItem;
    std::string m_nextBranchPlaylist;
};
