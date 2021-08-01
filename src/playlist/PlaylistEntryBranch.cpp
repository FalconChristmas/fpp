/*
 *   Playlist Entry Branch Class for Falcon Player (FPP)
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

#include "PlaylistEntryBranch.h"
#include "mqtt.h"

PlaylistEntryBranch::PlaylistEntryBranch(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_sHour(0),
    m_sMinute(0),
    m_sSecond(0),
    m_sDaySecond(-1),
    m_sHourSecond(-1),
    m_eHour(0),
    m_eMinute(0),
    m_eSecond(0),
    m_eDaySecond(-1),
    m_eHourSecond(-1),
    m_iterationStart(1),
    m_iterationCount(1),
    m_trueNextBranchType(PlaylistBranchType::NoBranch),
    m_trueNextItem(0),
    m_falseNextBranchType(PlaylistBranchType::NoBranch),
    m_falseNextItem(0),
    m_nextBranchType(PlaylistBranchType::NoBranch) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBranch::PlaylistEntryBranch()\n");
    m_type = "branch";
}

PlaylistEntryBranch::~PlaylistEntryBranch() {
}

static inline int asInt(Json::Value& v) {
    if (v.isIntegral()) {
        return v.asInt();
    }
    std::string s = v.asString();
    return std::atoi(s.c_str());
}

static inline PlaylistEntryBase::PlaylistBranchType toBranchType(Json::Value& v) {
    std::string s = v.asString();
    if (s == "Index") {
        return PlaylistEntryBase::PlaylistBranchType::Index;
    }
    if (s == "Playlist") {
        return PlaylistEntryBase::PlaylistBranchType::Playlist;
    }
    if (s == "Offset") {
        return PlaylistEntryBase::PlaylistBranchType::Offset;
    }
    if (s == "None") {
        return PlaylistEntryBase::PlaylistBranchType::NoBranch;
    }
    return PlaylistEntryBase::PlaylistBranchType::Index;
}
/*
 *
 */
int PlaylistEntryBranch::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBranch::Init()\n");

    if (config.isMember("trueNextBranchType")) {
        m_trueNextBranchType = toBranchType(config["trueNextBranchType"]);
    }
    if (config.isMember("falseNextBranchType")) {
        m_falseNextBranchType = toBranchType(config["falseNextBranchType"]);
    }

    if (config.isMember("trueNextSection")) {
        m_trueNextSection = config["trueNextSection"].asString();
    }

    if (config.isMember("trueNextItem")) {
        m_trueNextItem = asInt(config["trueNextItem"]);
        if (m_trueNextBranchType == PlaylistBranchType::Index) {
            m_trueNextItem--;
        }
    }

    if (config.isMember("trueBranchPlaylist")) {
        m_trueBranchPlaylist = config["trueBranchPlaylist"].asString();
    }

    if (config.isMember("falseNextSection")) {
        m_falseNextSection = config["falseNextSection"].asString();
    }

    if (config.isMember("falseNextItem")) {
        m_falseNextItem = asInt(config["falseNextItem"]);
        if (m_falseNextBranchType == PlaylistBranchType::Index) {
            m_falseNextItem--;
        }
    }

    if (config.isMember("falseBranchPlaylist")) {
        m_falseBranchPlaylist = config["falseBranchPlaylist"].asString();
    }

    if (config.isMember("branchType"))
        m_branchTest = config["branchType"].asString();
    else
        m_branchTest = config["branchTest"].asString();

    if (config.isMember("startTime")) {
        m_startTime = config["startTime"].asString();

        if (m_startTime != "") {
            std::vector<std::string> parts = split(m_startTime, ':');
            m_sHour = atoi(parts[0].c_str());
            m_sMinute = atoi(parts[1].c_str());
            m_sSecond = atoi(parts[2].c_str());
        }
    }

    if (config.isMember("endTime")) {
        m_endTime = config["endTime"].asString();

        if (m_endTime != "") {
            std::vector<std::string> parts = split(m_endTime, ':');
            m_eHour = atoi(parts[0].c_str());
            m_eMinute = atoi(parts[1].c_str());
            m_eSecond = atoi(parts[2].c_str());
        }
    }

    if (config.isMember("iterationStart"))
        m_iterationStart = config["iterationStart"].asInt();

    if (config.isMember("iterationCount"))
        m_iterationCount = config["iterationCount"].asInt();

    if (config.isMember("mqttTopic"))
        m_mqttTopic = config["mqttTopic"].asString();

    if (config.isMember("mqttMessage"))
        m_mqttMessage = config["mqttMessage"].asString();

    // compInfo is deprecated and should be removed at some point.  The fields
    // are now parsed from the startTime and endTime string fields.
    if (config.isMember("compInfo")) {
        Json::Value ci = config["compInfo"];

        if (m_branchTest == "Time") {
            if (ci.isMember("startHour"))
                m_sHour = ci["startHour"].asInt();

            if (ci.isMember("startMinute"))
                m_sMinute = ci["startMinute"].asInt();

            if (ci.isMember("startSecond"))
                m_sSecond = ci["startSecond"].asInt();

            if (ci.isMember("endHour"))
                m_eHour = ci["endHour"].asInt();

            if (ci.isMember("endMinute"))
                m_eMinute = ci["endMinute"].asInt();

            if (ci.isMember("endSecond"))
                m_eSecond = ci["endSecond"].asInt();
        }
    }

    m_sHourSecond = (m_sMinute * 60) + m_sSecond;
    m_eHourSecond = (m_eMinute * 60) + m_eSecond;

    if (m_sHour >= 0)
        m_sDaySecond = (m_sHour * 3600) + (m_sMinute * 60) + m_sSecond;

    if (m_eHour >= 0)
        m_eDaySecond = (m_eHour * 3600) + (m_eMinute * 60) + m_eSecond;

    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryBranch::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryBranch::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    if (m_branchTest == "Time") {
        time_t currTime = time(NULL);
        struct tm now;

        localtime_r(&currTime, &now);

        int daySecond = (now.tm_hour * 3600) + (now.tm_min * 60) + now.tm_sec;

        if ((m_sDaySecond >= 0) && (m_eDaySecond >= 0)) {
            if ((daySecond >= m_sDaySecond) && (daySecond < m_eDaySecond))
                SetNext(1);
            else
                SetNext(0);
        } else if (m_sDaySecond >= 0) {
            if (daySecond >= m_sDaySecond)
                SetNext(1);
            else
                SetNext(0);
        } else if (m_sDaySecond >= 0) {
            if (daySecond < m_eDaySecond)
                SetNext(1);
            else
                SetNext(0);
        } else { // Just compare minutes & seconds
            int hourSecond = (now.tm_min * 60) + now.tm_sec;

            if ((hourSecond >= m_sHourSecond) && (hourSecond < m_eHourSecond))
                SetNext(1);
            else
                SetNext(0);
        }

        LogDebug(VB_PLAYLIST, "Now: %02d:%02d:%02d, Start: %02d:%02d:%02d, End: %02d:%02d:%02d, NS: %s, NI: %d\n", now.tm_hour, now.tm_min, now.tm_sec, m_sHour, m_sMinute, m_sSecond, m_eHour, m_eMinute, m_eSecond, m_nextSection.c_str(), m_nextItem);
    } else if (m_branchTest == "Loop") {
        int curLoop = m_parentPlaylist->GetLoopNumber();

        LogDebug(VB_PLAYLIST, "Current Loop is %d, we want every %d iteration(s) starting at %d\n", curLoop, m_iterationCount, m_iterationStart);
        if ((curLoop >= m_iterationStart) &&
            (((curLoop - m_iterationStart) % m_iterationCount) == 0))
            SetNext(1);
        else
            SetNext(0);
    } else if (m_branchTest == "MQTT") {
        if ((mqtt) && (mqtt->CacheCheckMessage(m_mqttTopic, m_mqttMessage))) {
            SetNext(1);
        } else {
            SetNext(0);
        }
    } else {
        SetNext(0);
    }

    PlaylistEntryBase::StartPlaying();

    FinishPlay();

    return 1;
}

/*
 *
 */
void PlaylistEntryBranch::SetNext(int isTrue) {
    if (isTrue) {
        m_nextBranchType = m_trueNextBranchType;
        m_nextSection = m_trueNextSection;
        m_nextItem = m_trueNextItem;
        m_nextBranchPlaylist = m_trueBranchPlaylist;
    } else {
        m_nextBranchType = m_falseNextBranchType;
        m_nextSection = m_falseNextSection;
        m_nextItem = m_falseNextItem;
        m_nextBranchPlaylist = m_falseBranchPlaylist;
    }
}

/*
 *
 */
void PlaylistEntryBranch::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Branch Test       : %s\n", m_branchTest.c_str());

    if (m_branchTest == "Time") {
        LogDebug(VB_PLAYLIST, "Start Time        : %s\n", m_startTime.c_str());
        LogDebug(VB_PLAYLIST, "End Time          : %s\n", m_endTime.c_str());
        LogDebug(VB_PLAYLIST, "Start Hour        : %d\n", m_sHour);
        LogDebug(VB_PLAYLIST, "Start Minute      : %d\n", m_sMinute);
        LogDebug(VB_PLAYLIST, "Start Second      : %d\n", m_sSecond);
        LogDebug(VB_PLAYLIST, "Start Day Second  : %d\n", m_sDaySecond);
        LogDebug(VB_PLAYLIST, "Start Hour Second : %d\n", m_sHourSecond);
        LogDebug(VB_PLAYLIST, "End Hour          : %d\n", m_eHour);
        LogDebug(VB_PLAYLIST, "End Minute        : %d\n", m_eMinute);
        LogDebug(VB_PLAYLIST, "End Second        : %d\n", m_eSecond);
        LogDebug(VB_PLAYLIST, "End Day Second    : %d\n", m_eDaySecond);
        LogDebug(VB_PLAYLIST, "End Hour Second   : %d\n", m_eHourSecond);
    } else if (m_branchTest == "Loop") {
        LogDebug(VB_PLAYLIST, "Iteration Start   : %d\n", m_iterationStart);
        LogDebug(VB_PLAYLIST, "Iteration Count   : %d\n", m_iterationCount);
    } else if (m_branchTest == "MQTT") {
        LogDebug(VB_PLAYLIST, "MQTT Topic        : %s\n", m_mqttTopic.c_str());
        LogDebug(VB_PLAYLIST, "MQTT Message      : %s\n", m_mqttMessage.c_str());
    }

    switch (m_trueNextBranchType) {
    case PlaylistEntryBase::PlaylistBranchType::Index:
        LogDebug(VB_PLAYLIST, "True Next Section : %s\n", m_trueNextSection.c_str());
        LogDebug(VB_PLAYLIST, "True Next Item    : %d\n", m_trueNextItem);
        break;
    case PlaylistEntryBase::PlaylistBranchType::Offset:
        LogDebug(VB_PLAYLIST, "True Item Offset  : %d\n", m_trueNextItem);
        break;
    case PlaylistEntryBase::PlaylistBranchType::Playlist:
        LogDebug(VB_PLAYLIST, "True Branch PList : %s\n", m_trueBranchPlaylist.c_str());
        break;
    default:
        LogDebug(VB_PLAYLIST, "ERROR Uknown True branch type: %d\n", (int)m_trueNextBranchType);
        break;
    }

    switch (m_falseNextBranchType) {
    case PlaylistEntryBase::PlaylistBranchType::Index:
        LogDebug(VB_PLAYLIST, "False Next Section: %s\n", m_falseNextSection.c_str());
        LogDebug(VB_PLAYLIST, "False Next Item   : %d\n", m_falseNextItem);
        break;
    case PlaylistEntryBase::PlaylistBranchType::Offset:
        LogDebug(VB_PLAYLIST, "False Item Offset : %d\n", m_falseNextItem);
        break;
    case PlaylistEntryBase::PlaylistBranchType::Playlist:
        LogDebug(VB_PLAYLIST, "False Branch PList: %s\n", m_falseBranchPlaylist.c_str());
        break;
    default:
        LogDebug(VB_PLAYLIST, "ERROR Uknown False branch type: %d\n", (int)m_falseNextBranchType);
        break;
    }
}

/*
 *
 */
Json::Value PlaylistEntryBranch::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    result["branchTest"] = m_branchTest;
    if (m_startTime != "") {
        result["startTime"] = m_startTime;
    } else {
        char timeStr[9];
        snprintf(timeStr, 9, "%02d:%02d:%02d", m_sHour, m_sMinute, m_sSecond);
        result["startTime"] = timeStr;
    }

    if (m_endTime != "") {
        result["endTime"] = m_endTime;
    } else {
        char timeStr[9];
        snprintf(timeStr, 9, "%02d:%02d:%02d", m_eHour, m_eMinute, m_eSecond);
        result["endTime"] = timeStr;
    }

    return result;
}
