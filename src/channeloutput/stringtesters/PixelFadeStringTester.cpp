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

#include "PixelFadeStringTester.h"
#include "../PixelString.h"

uint8_t* PixelFadeStringTester::createTestData(PixelString* ps, int cycleCount, float percentOfCycle, uint8_t* inChannelData, uint32_t &newLen) {
    newLen = ps->m_outputChannels;
    uint8_t* data = ps->m_outputBuffer;
    memset(data, 0, ps->m_outputChannels);
    uint8_t* out = data;
    uint32_t inCh = 0;

    uint8_t fadeValue;
    if (percentOfCycle < 0.50) {
        fadeValue = percentOfCycle * 508.0 + 1;
    } else {
        fadeValue = 255 - (percentOfCycle - 0.5) * 508.0;
    }
    
    for (auto& vs : ps->m_virtualStrings) {
        if (vs.receiverNum == -1) {
            int offset = -1;
            uint8_t brightness = vs.brightnessMap[fadeValue];

            if (fadeType != 3 || vs.channelsPerNode() < 4) {
                //single channel
                switch (fadeType) {
                case 0:
                    offset = vs.colorOrder.redOffset();
                    break;
                case 1:
                    offset = vs.colorOrder.greenOffset();
                    break;
                case 2:
                    offset = vs.colorOrder.blueOffset();
                    break;
                case 3:
                    offset = -1; //all three channesl for white
                    break;
                }
                if (vs.channelsPerNode() == 4 && vs.whiteOffset == 0) {
                    ++offset;
                }
            } else {
                //white for 4 channel
                offset = vs.whiteOffset;
            }
            for (int x = 0; x < vs.pixelCount; ++x) {
                if (offset == -1) {
                    for (int y = 0; y < vs.channelsPerNode(); ++y) {
                        *out = brightness;
                        ++out;
                        ++inCh;
                    }                    
                } else {
                    out[offset] = brightness;
                    out += vs.channelsPerNode();
                    inCh += vs.channelsPerNode();
                }
            }
        } else {
            fillInSmartReceiver(ps, vs, inChannelData, inCh, out);
        }
    }
    return data;
}

PixelFadeStringTester PixelFadeStringTester::INSTANCE_RED(0);
PixelFadeStringTester PixelFadeStringTester::INSTANCE_GREEN(1);
PixelFadeStringTester PixelFadeStringTester::INSTANCE_BLUE(2);
PixelFadeStringTester PixelFadeStringTester::INSTANCE_WHITE(3);
