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

#include "../common.h"
#include "../log.h"

#include "TestPatternBase.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"

/*
 *
 */
TestPatternBase::TestPatternBase() :
    m_testEnabled(0),
    m_testPatternName("basePattern"),
    m_minChannels(2),
    m_testData(NULL),
    m_nextCycleTime(0),
    m_cycleMS(0),
    m_channelCount(0),
    m_configChanged(0) {
    LogExcess(VB_CHANNELOUT, "TestPatternBase::TestPatternBase()\n");

    // Give room for an extra RGBW Pixel to make coding test patterns easier
    m_testData = new char[FPPD_MAX_CHANNELS + 4];
}

/*
 *
 */
TestPatternBase::~TestPatternBase() {
    LogExcess(VB_CHANNELOUT, "TestPatternBase::~TestPatternBase()\n");

    delete[] m_testData;
}

/*
 *
 */
int TestPatternBase::Init(std::string configStr) {
    Json::Value config;
    if (!LoadJsonFromString(configStr, config)) {
        LogErr(VB_CHANNELOUT,
               "Error parsing Test Pattern config string: '%s'\n",
               configStr.c_str());
        return 0;
    }

    return Init(config);
}

/*
 *
 */
int TestPatternBase::Init(Json::Value config) {
    if (m_cycleMS != config["cycleMS"].asInt()) {
        m_cycleMS = config["cycleMS"].asInt();
        m_nextCycleTime = GetTime() + (m_cycleMS * 1000);
    }

    if (config.isMember("channelSet")) {
        if (m_channelSetStr != config["channelSet"].asString()) {
            m_channelSetStr = config["channelSet"].asString();
            SetChannelSet(m_channelSetStr);
            m_configChanged = 1;
        }
    } else if (m_channelSetStr != "") {
        m_channelSetStr = "";
        m_configChanged = 1;
        m_channelSet.clear();
    }

    if (m_configChanged) {
        SetupTest();
    }

    m_testEnabled = 1;

    DumpConfig();

    return 1;
}

/*
 *
 */
int TestPatternBase::SetChannelSet(std::string channelSetStr) {
    int start = 0;
    int end = 0;

    int max = 0;
    for (auto& a : GetOutputRanges()) {
        int e = a.first + a.second - 1;
        max = std::max(max, e);
    }
    if (max < 8) {
        max = 8;
    }

    m_channelSet.clear();
    m_channelCount = 0;

    std::vector<std::string> ranges = split(channelSetStr, ';');

    for (int r = 0; r < ranges.size(); r++) {
        PixelOverlayModel* model = PixelOverlayManager::INSTANCE.getModel(ranges[r]);
        if (model != nullptr) {
            start = model->getStartChannel();
            end = start + model->getChannelCount() - 1;
        } else {
            std::vector<std::string> parts = split(ranges[r], '-');

            if (parts.size()) {
                start = atoi(parts[0].c_str());
                if (start > 0)
                    start--;

                if (parts.size() > 1)
                    end = atoi(parts[1].c_str());
                else
                    end = start;

                if (end > 0)
                    end--;

                if (end > max)
                    end = max;
            }
        }
        if (end < start) {
            end = start;
        }
        m_channelSet.push_back(std::make_pair(start, end));
        m_channelCount += end - start + 1;
    }

    return 1;
}

/*
 *
 */
int TestPatternBase::SetupTest(void) {
    m_nextCycleTime = GetTime() + (m_cycleMS * 1000);

    return 1;
}

/*
 *
 */
int TestPatternBase::OverlayTestData(char* channelData) {
    int count = 0;
    int copied = 0;

    if (!m_testEnabled) {
        // Clear channels being tested
        for (int s = 0; s < m_channelSet.size(); s++) {
            count = m_channelSet[s].second - m_channelSet[s].first + 1;

            bzero(channelData + m_channelSet[s].first, count);
        }

        return 1;
    }

    for (int s = 0; s < m_channelSet.size(); s++) {
        count = m_channelSet[s].second - m_channelSet[s].first + 1;

        memcpy(channelData + m_channelSet[s].first, m_testData + copied,
               count);

        copied += count;
    }

    if (GetTime() >= m_nextCycleTime) {
        CycleData();
        m_nextCycleTime = GetTime() + (m_cycleMS * 1000);
    }

    return 1;
}

/*
 *
 */
void TestPatternBase::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "TestPatternBase::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "    cycleMS     : %d\n", m_cycleMS);
    LogDebug(VB_CHANNELOUT, "    channelSet  : %s\n", m_channelSetStr.c_str());
    LogDebug(VB_CHANNELOUT, "    channelCount: %d\n", m_channelCount);

    for (int s = 0; s < m_channelSet.size(); s++) {
        LogDebug(VB_CHANNELOUT, "                : %d-%d\n",
                 m_channelSet[s].first + 1, m_channelSet[s].second + 1);
    }
}
