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

#include "../../log.h"

#include "ThreeToFourOutputProcessor.h"

ThreeToFourOutputProcessor::ThreeToFourOutputProcessor(const Json::Value& config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();

    order = config["colorOrder"].asInt();
    algorithm = config["algorithm"].asInt();

    //channel numbers need to be 0 based
    --start;
}

ThreeToFourOutputProcessor::~ThreeToFourOutputProcessor() {
}

void ThreeToFourOutputProcessor::ProcessData(unsigned char* channelData) const {
    int curDest = start + (count - 1) * 4;
    int curSrc = start + (count - 1) * 3;

    for (int x = 0; x < count; x++, curSrc -= 3, curDest -= 4) {
        int r = channelData[curSrc];
        int g = channelData[curSrc + 1];
        int b = channelData[curSrc + 2];
        int w = 0;
        switch (algorithm) {
        case 1: // r == g == b -> w
            if (r == g && r == b) {
                w = r;
                r = 0;
                g = 0;
                b = 0;
            }
            break;
        case 2: {
            int maxc = std::max(r, std::max(g, b));
            if (maxc == 0) {
                w = 0;
            } else {
                int minc = std::min(r, std::min(g, b));
                // find colour with 100% hue
                float multiplier = 255.0f / maxc;
                float h0 = r * multiplier;
                float h1 = g * multiplier;
                float h2 = b * multiplier;

                float maxW = std::max(h0, std::max(h1, h2));
                float minW = std::min(h0, std::min(h1, h2));
                int whiteness = ((maxW + minW) / 2.0f - 127.5f) * (255.0f / 127.5f) / multiplier;
                if (whiteness < 0)
                    whiteness = 0;
                else if (whiteness > minc)
                    whiteness = minc;
                w = whiteness;
                r -= whiteness;
                g -= whiteness;
                b -= whiteness;
            }
        } break;
        default: // no white
            break;
        }
        switch (order) {
        case 4123:
            channelData[curDest] = w;
            channelData[curDest + 1] = r;
            channelData[curDest + 2] = g;
            channelData[curDest + 3] = b;
            break;
        case 1234:
        default:
            channelData[curDest] = r;
            channelData[curDest + 1] = g;
            channelData[curDest + 2] = b;
            channelData[curDest + 3] = w;
            break;
        }
    }
}
