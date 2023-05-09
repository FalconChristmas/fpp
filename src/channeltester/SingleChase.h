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
#include <vector>

#include "TestPatternBase.h"

class TestPatternSingleChase : public TestPatternBase {
public:
    TestPatternSingleChase();
    virtual ~TestPatternSingleChase();

    virtual int Init(Json::Value config) override;

    virtual int SetupTest(void) override;
    virtual void DumpConfig(void) override;

private:
    void CycleData(void) override;

    int m_chaseSize;
    uint8_t m_chaseValue;
};
