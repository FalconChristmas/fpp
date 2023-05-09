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

#include "SingleChase.h"

/*
 *
 */
TestPatternSingleChase::TestPatternSingleChase() :
    m_chaseSize(0),
    m_chaseValue(255) {
    LogExcess(VB_CHANNELOUT, "TestPatternSingleChase::TestPatternSingleChase()\n");
    m_testPatternName = "SingleChase";
}

/*
 *
 */
TestPatternSingleChase::~TestPatternSingleChase() {
    LogExcess(VB_CHANNELOUT, "TestPatternSingleChase::~TestPatternSingleChase()\n");
}

/*
 *
 */
int TestPatternSingleChase::Init(Json::Value config) {
    m_configChanged = 0;

    if (m_chaseSize != config["chaseSize"].asInt()) {
        m_chaseSize = config["chaseSize"].asInt();
        m_configChanged = 1;
    }

    if (m_chaseValue != config["chaseValue"].asInt()) {
        m_chaseValue = config["chaseValue"].asInt();
        m_configChanged = 1;
    }

    return TestPatternBase::Init(config);
}

/*
 *
 */
int TestPatternSingleChase::SetupTest(void) {
    uint8_t* c = (uint8_t*)m_testData;
    bzero(m_testData, m_channelCount);

    for (int i = 0; i < m_channelCount; i += m_chaseSize) {
        *c = m_chaseValue;
        c += m_chaseSize;
    }

    return TestPatternBase::SetupTest();
}

/*
 *
 */
void TestPatternSingleChase::CycleData(void) {
    memmove(m_testData + 1, m_testData, m_channelCount - 1);

    if (m_testData[m_chaseSize] == m_chaseValue)
        m_testData[0] = m_chaseValue;
    else
        m_testData[0] = 0x00;
}

/*
 *
 */
void TestPatternSingleChase::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "TestPatternSingleChase::DumpConfig\n");
    LogDebug(VB_CHANNELOUT, "    chaseSize   : %d\n", m_chaseSize);
    LogDebug(VB_CHANNELOUT, "    chaseValue  : %d (0x%x)\n",
             (unsigned char)m_chaseValue, (unsigned char)m_chaseValue);

    TestPatternBase::DumpConfig();
}
