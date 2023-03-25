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

#include "PixelCountStringTester.h"

uint8_t* PixelCountPixelStringTester::createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData) const {
    uint8_t* data = new uint8_t[ps->m_outputChannels];
    uint8_t* out = data;
    uint32_t inCh = 0;
    unsigned char clr[6];
    clr[0] = clr[2] = 0;
    clr[1] = 255;
    clr[3] = clr[5] = 0;
    clr[4] = 255;
    int clrOffset = cycleCount % 3;
    int curPixel = 1;
    for (auto& vs : ps->m_virtualStrings) {
        if (m_byString) {
            clrOffset = cycleCount % 3;
            curPixel = 1;
        }
        if (vs.receiverNum == -1) {
            for (int x = 0; x < vs.pixelCount; ++x) {
                if (curPixel == 50) {
                    uint8_t brightness = vs.brightnessMap[255];
                    for (int y = 0; y < vs.channelsPerNode(); ++y) {
                        *out = brightness;
                        ++out;
                        ++inCh;
                    }
                    if (++clrOffset == 3) {
                        clrOffset = 0;
                    }
                    curPixel = 1;
                } else {
                    if (vs.channelsPerNode() == 4 && !vs.whiteOffset) {
                        ++out;
                    }
                    for (int y = 0; y < std::min(vs.channelsPerNode(), 3); ++y) {
                        *out = vs.brightnessMap[clr[y + clrOffset]];
                        out++;
                    }
                    if (vs.channelsPerNode() == 4 && vs.whiteOffset) {
                        ++out;
                    }
                    inCh += vs.channelsPerNode();
                    if (curPixel % 10 == 0) {
                        if (++clrOffset == 3) {
                            clrOffset = 0;
                        }
                    }
                    ++curPixel;
                }
            }
        } else {
            fillInSmartReceiver(ps, vs, inChannelData, inCh, out);
        }
    }
    return data;
}

PixelCountPixelStringTester PixelCountPixelStringTester::INSTANCE_BYPORT(false);
PixelCountPixelStringTester PixelCountPixelStringTester::INSTANCE_BYSTRING(true);
