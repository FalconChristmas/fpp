#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "PixelStringTester.h"

class PixelCountPixelStringTester : public PixelStringTester {
protected:
    PixelCountPixelStringTester(bool byString) :
        PixelStringTester(), m_byString(byString) {}

    const bool m_byString;

public:
    virtual uint8_t* createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t &newLen) override;

    static PixelCountPixelStringTester INSTANCE_BYSTRING;
    static PixelCountPixelStringTester INSTANCE_BYPORT;
};


class CurrentBasedPixelCountPixelStringTester : public PixelStringTester {
protected:
    CurrentBasedPixelCountPixelStringTester() :
        PixelStringTester() {}

    int currentState = 0;
    uint64_t startTimeMS = 0;
    uint64_t currentTimeMS = 0;
    uint32_t curOutCount = 0xFFFFFFFF;
    uint32_t changeCount = 0;
    uint32_t noChangeCount = 0;

    int32_t valuesIdx = 0;
    int32_t lastPort = 0xFFFF;
    int32_t lastIdx = -1;
    std::vector<float> lastValues;
    std::vector<float> baseValues;
    std::vector<int32_t> lastPixelIdx;
    std::vector<int> testingPort;

    int firstPort = 0xFFFF;
public:
    virtual uint8_t* createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t &newLen) override;

    static CurrentBasedPixelCountPixelStringTester INSTANCE;
};
