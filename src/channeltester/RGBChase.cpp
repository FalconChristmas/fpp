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

#include "fpp-json.h"

#include "../log.h"

#include "RGBChase.h"

/*
 *
 */
TestPatternRGBChase::TestPatternRGBChase() :
    m_colorPatternStr(""),
    m_colorPatternSize(0),
    m_patternOffset(0),
    m_rgbTriplets(0),
    m_lastTripletOffset(0),
    m_lastPatternOffset(0),
    m_channelsPerNode(3) {
    LogExcess(VB_CHANNELOUT, "TestPatternRGBChase::TestPatternRGBChase()\n");

    m_testPatternName = "RGBChase";
}

/*
 *
 */
TestPatternRGBChase::~TestPatternRGBChase() {
    LogExcess(VB_CHANNELOUT, "TestPatternRGBChase::~TestPatternRGBChase()\n");
}

/*
 *
 */
int TestPatternRGBChase::Init(Json::Value config) {
    m_configChanged = 0;

    std::string colorPattern = JsonString(config, "colorPattern");
    if (m_colorPatternStr != colorPattern) {
        m_colorPatternStr = colorPattern;
        m_configChanged = 1;
    }

    int cpn = JsonInt(config, "channelsPerNode", 3);
    if (cpn != 3 && cpn != 4) {
        cpn = 3;
    }
    if (m_channelsPerNode != cpn) {
        m_channelsPerNode = cpn;
        m_configChanged = 1;
    }

    return TestPatternBase::Init(config);
}

/*
 *
 */
int TestPatternRGBChase::SetupTest(void) {
    bzero(m_testData, m_channelCount);

    m_colorPattern.clear();

    char digit = 0;
    for (int i = 0; i < m_colorPatternStr.size(); i += 2) {
        digit = (char)strtol(m_colorPatternStr.substr(i, 2).c_str(), NULL, 16);
        m_colorPattern.push_back(digit);
    }

    // Make sure we have a valid set of color groups (3 channels for RGB, 4 for RGBW)
    const int stride = m_channelsPerNode;
    while (m_colorPattern.size() < stride || m_colorPattern.size() % stride)
        m_colorPattern.push_back(0);

    // Need at least two pixels for a chase; bump before filling so the whole
    // (possibly expanded) range gets initialized
    if (m_channelCount < (2 * stride)) {
        m_channelCount = 2 * stride;
        bzero(m_testData, m_channelCount);
    }

    // A trailing partial pixel (a channel count that is not a multiple of the
    // stride, as an arbitrary set of discrete channels gives) is filled with as
    // much of its color as fits instead of being skipped and left dark. The
    // pattern offset still advances by a whole stride so the wrap point is
    // unchanged for the whole-pixel case.
    char* c = m_testData;
    int offset = 0;
    for (int i = 0; i < m_channelCount; i += stride) {
        int count = std::min(stride, m_channelCount - i);
        for (int j = 0; j < count; j++)
            *(c++) = m_colorPattern[offset + j];

        offset += stride;
        if (offset >= m_colorPattern.size())
            offset = 0;
    }

    m_patternOffset = 0;
    m_colorPatternSize = m_colorPattern.size() / stride;
    m_rgbTriplets = m_channelCount / stride;
    m_lastTripletOffset = (m_rgbTriplets - 1) * stride;
    m_lastPatternOffset = m_lastTripletOffset % m_colorPattern.size();

    return TestPatternBase::SetupTest();
}

/*
 *
 */
void TestPatternRGBChase::CycleData(void) {
    const int stride = m_channelsPerNode;
    memmove(m_testData + stride, m_testData, m_channelCount - stride);

    m_patternOffset -= stride;

    if (m_patternOffset < 0)
        m_patternOffset = m_colorPattern.size() - stride;

    int offset = m_patternOffset;

    for (int j = 0; j < stride; j++)
        m_testData[j] = m_colorPattern[offset++];
}

/*
 *
 */
void TestPatternRGBChase::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "TestPatternRGBChase::DumpConfig\n");
    LogDebug(VB_CHANNELOUT, "    colorPattern    : %s\n", m_colorPatternStr.c_str());
    LogDebug(VB_CHANNELOUT, "    colorPatternSize: %d\n", m_colorPatternSize);

    TestPatternBase::DumpConfig();
}
