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

#include "fpp-pch.h"

#include "PixelStringTester.h"

#include "PixelCountStringTester.h"
#include "PixelFadeStringTester.h"
#include "PortNumberStringTester.h"

void PixelStringTester::fillInSmartReceiver(PixelString* ps, VirtualString& vs, uint8_t* inChannelData, uint32_t& offset, uint8_t*& outChannelData) const {
    for (int x = 0; x < vs.pixelCount * vs.channelsPerNode(); ++x) {
        int cn = ps->m_outputMap[offset];
        uint8_t* brightness = ps->m_brightnessMaps[offset++];
        *outChannelData = brightness[inChannelData[cn]];
        ++outChannelData;
    }
}

PixelStringTester* PixelStringTester::getPixelStringTester(int i) {
    static std::vector<PixelStringTester*> TESTERS = {
        nullptr, // 0 position is OFF
        &OutputPortNumberPixelStringTester::INSTANCE,
        &PixelCountPixelStringTester::INSTANCE_BYPORT,
        &PixelCountPixelStringTester::INSTANCE_BYSTRING,
        &PixelFadeStringTester::INSTANCE_RED,
        &PixelFadeStringTester::INSTANCE_GREEN,
        &PixelFadeStringTester::INSTANCE_BLUE,
        &PixelFadeStringTester::INSTANCE_WHITE
    };
    if (i < TESTERS.size()) {
        return TESTERS[i];
    }
    if (i = 999) {
        return &CurrentBasedPixelCountPixelStringTester::INSTANCE;
    }
    return nullptr;
}