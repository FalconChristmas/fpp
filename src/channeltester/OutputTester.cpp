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

#include "OutputTester.h"
#include "channeloutput/ChannelOutputSetup.h"

OutputTester::OutputTester() {
}
OutputTester::~OutputTester() {
}
int OutputTester::Init(Json::Value config) {
    outputTypes.clear();
    testType = config["type"].asInt();
    std::string s = config["outputs"].asString();
    if (s != "--ALL--" && s != "") {
        outputTypes.insert(s);
    }
    if (config.isMember("config")) {
        extraConfig = config["config"];
    }
    return TestPatternBase::Init(config);
}
int OutputTester::SetupTest(void) {
    return TestPatternBase::SetupTest();
}
void OutputTester::CycleData(void) {
    cycleCount++;
    startMS = GetTimeMS();
}
int OutputTester::OverlayTestData(char* channelData) {
    if (m_testEnabled) {
        uint64_t now = GetTimeMS();
        int offset = now - startMS;
        float pct = offset;
        pct /= m_cycleMS;
        OverlayOutputTestData(outputTypes, (unsigned char*)channelData, cycleCount, pct, testType, extraConfig);
    } else {
        OverlayOutputTestData(outputTypes, (unsigned char*)channelData, 0, 0, 0, extraConfig);
    }
    return TestPatternBase::OverlayTestData(channelData);
}
void OutputTester::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "OutputTester::DumpConfig\n");
    LogDebug(VB_CHANNELOUT, "    testType     : %d\n", testType);
    TestPatternBase::DumpConfig();
}
