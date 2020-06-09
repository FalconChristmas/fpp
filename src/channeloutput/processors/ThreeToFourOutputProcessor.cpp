/*
 *   ThreeToFourOutputProcessor class for Falcon Player (FPP)
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "ThreeToFourOutputProcessor.h"
#include "log.h"

ThreeToFourOutputProcessor::ThreeToFourOutputProcessor(const Json::Value &config) {
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

void ThreeToFourOutputProcessor::ProcessData(unsigned char *channelData) const {
    int curDest = start + (count - 1) * 4;
    int curSrc = start + (count - 1) * 3;
    
    for (int x = 0; x < count; x++, curSrc -= 3, curDest -= 4) {
        int r = channelData[curSrc];
        int g = channelData[curSrc + 1];
        int b = channelData[curSrc + 2];
        int w = 0;
        switch (algorithm) {
            case 1:  // r == g == b -> w
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
                        if (whiteness < 0) whiteness = 0;
                        else if (whiteness > minc) whiteness = minc;
                        w = whiteness;
                        r -= whiteness;
                        g -= whiteness;
                        b -= whiteness;
                    }
                }
                break;
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
