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
    virtual uint8_t* createTestData(PixelString* ps, int cycleCount, uint8_t* inChannelData) const override;

    static PixelCountPixelStringTester INSTANCE_BYSTRING;
    static PixelCountPixelStringTester INSTANCE_BYPORT;
};
