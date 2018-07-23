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

#include "ColorOrderOutputProcessor.h"
#include "log.h"

ColorOrderOutputProcessor::ColorOrderOutputProcessor(const Json::Value &config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    order = config["colorOrder"].asInt();
    LogInfo(VB_CHANNELOUT, "Color Order:   %d-%d => %d\n",
            start, start + (count*3) - 1,
            order);
    
    //channel numbers need to be 0 based
    --start;
}

ColorOrderOutputProcessor::~ColorOrderOutputProcessor() {
    
}

void ColorOrderOutputProcessor::ProcessData(unsigned char *channelData) const {
    int cur = start;
    for (int x = 0; x < count; x++, cur += 3) {
        int a = channelData[cur];
        int b = channelData[cur + 1];
        int c = channelData[cur + 2];
        switch (order) {
            case 132:
                channelData[cur + 1] = c;
                channelData[cur + 2] = b;
                break;
            case 213:
                channelData[cur] = b;
                channelData[cur + 1] = a;
                break;
            case 231:
                channelData[cur] = b;
                channelData[cur + 1] = c;
                channelData[cur + 2] = a;
                break;
            case 312:
                channelData[cur] = c;
                channelData[cur + 1] = a;
                channelData[cur + 2] = b;
                break;
            case 321:
                channelData[cur] = c;
                channelData[cur + 2] = a;
                break;
        }
    }
}
