/*
 *   HoldValueOutputProcessor class for Falcon Player (FPP)
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

#include "HoldValueOutputProcessor.h"
#include "log.h"

HoldValueOutputProcessor::HoldValueOutputProcessor(const Json::Value &config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    LogInfo(VB_CHANNELOUT, "Hold Channel Values:   %d-%d\n",
            start, start + count - 1);
    
    //channel numbers need to be 0 based
    --start;
    
    lastValues = new unsigned char[count];
}

HoldValueOutputProcessor::~HoldValueOutputProcessor() {
    delete [] lastValues;
}

void HoldValueOutputProcessor::ProcessData(unsigned char *channelData) const {
    for (int x = 0; x < count; x++) {
        if (channelData[x + start] == 0) {
            channelData[x + start] = lastValues[x];
        } else  {
            lastValues[x] = channelData[x + start];
        }
    }
}
