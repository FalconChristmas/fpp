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
#include <utility>
#include <vector>

#include "../Sequence.h"

class TestPatternBase {
public:
    TestPatternBase();
    virtual ~TestPatternBase();

    std::string Name(void) { return m_testPatternName; }
    int Init(std::string configStr);
    virtual int Init(Json::Value config);
    virtual int SetupTest(void);

    int SetChannelSet(std::string channelSetStr);
    virtual int OverlayTestData(char* channelData);
    void DisableTest(void) { m_testEnabled = 0; }

    virtual void DumpConfig(void);

protected:
    virtual void CycleData(void) {}

    volatile int m_testEnabled;
    std::string m_testPatternName;
    int m_minChannels;
    char* m_testData;
    long long m_nextCycleTime;
    int m_cycleMS;

    std::string m_channelSetStr;
    int m_channelCount;
    int m_configChanged;

    std::vector<std::pair<int, int>> m_channelSet;
};
