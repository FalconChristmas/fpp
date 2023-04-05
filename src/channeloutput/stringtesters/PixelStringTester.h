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

#include "../PixelString.h"

class PixelStringTester {
protected:
    PixelStringTester() {}
    virtual ~PixelStringTester() {}

    void fillInSmartReceiver(PixelString* ps, VirtualString& vs, uint8_t* inChannelData, uint32_t &offset, uint8_t* &outChannelData) const;
    
public:
    virtual uint8_t* createTestData(PixelString* string, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t &newLen) = 0;

    static PixelStringTester* getPixelStringTester(int i);
};
