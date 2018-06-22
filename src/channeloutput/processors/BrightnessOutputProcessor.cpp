/*
 *   RemapOutputProcessor class for Falcon Player (FPP)
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
#include <cmath>

#include "BrightnessOutputProcessor.h"
#include "log.h"

BrightnessOutputProcessor::BrightnessOutputProcessor(const Json::Value &config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    brightness = config["brightness"].asInt();
    gamma = config["gamma"].asFloat();
    LogInfo(VB_CHANNELOUT, "Brightness:   %d-%d => Brightness:%d   Gamma: %f\n",
            start, start + count - 1,
            brightness, gamma);
    
    
    float bf = brightness;
    float maxB = bf * 2.55f;
    for (int x = 0; x < 256; x++) {
        float f = x;
        f = maxB * pow(f / 255.0f, gamma);
        if (f > 255.0) {
            f = 255.0;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        table[x] = round(f);
    }

    
    //channel numbers need to be 0 based
    --start;
}

BrightnessOutputProcessor::~BrightnessOutputProcessor() {
    
}

void BrightnessOutputProcessor::ProcessData(unsigned char *channelData) const {
    for (int x = 0; x < count; x++) {
        channelData[start + x] = table[channelData[start + x]];
    }
}
