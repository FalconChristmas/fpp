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

#include "PortNumberStringTester.h"
#include "../PixelString.h"

uint8_t* OutputPortNumberPixelStringTester::createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t &newLen) {
    newLen = ps->m_outputChannels;
    uint8_t* data = ps->m_outputBuffer;
    uint8_t* out = data;
    uint32_t inCh = 0;
    unsigned char clr[3];
    switch (cycleCount % 3) {
    case 0:
        clr[2] = clr[1] = 0;
        clr[0] = 255;        
        break;
    case 1:
        clr[0] = clr[2] = 0;
        clr[1] = 255;        
        break;
    default:
        clr[0] = clr[1] = 0;
        clr[2] = 255;
        break;
    }
    int curPixel = 0;
    for (auto& vs : ps->m_virtualStrings) {
        if (vs.receiverNum == -1) {
            for (int x = 0; x < vs.pixelCount; ++x) {
                if (curPixel <= ps->m_portNumber) {
                    uint8_t brightness = vs.brightnessMap[255];
                    for (int y = 0; y < vs.channelsPerNode(); ++y) {
                        *out = brightness;
                        ++out;
                        ++inCh;
                    }
                } else {
                    if (vs.channelsPerNode() == 4 && !vs.whiteOffset) {
                        ++out;
                    }
                    for (int y = 0; y < std::min(vs.channelsPerNode(), 3); ++y) {
                        *out = vs.brightnessMap[clr[y]];
                        out++;
                    }
                    if (vs.channelsPerNode() == 4 && vs.whiteOffset) {
                        ++out;
                    }
                    inCh += vs.channelsPerNode();
                }
                ++curPixel;
            }
        } else {
            fillInSmartReceiver(ps, vs, inChannelData, inCh, out);
        }
    }
    return data;
}

OutputPortNumberPixelStringTester OutputPortNumberPixelStringTester::INSTANCE;
