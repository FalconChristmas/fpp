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

#include "RemapOutputProcessor.h"
#include "log.h"

RemapOutputProcessor::RemapOutputProcessor(const Json::Value &config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    sourceChannel = config["source"].asInt();
    destChannel = config["destination"].asInt();
    count = config["count"].asInt();
    loops = config["loops"].asInt();
    LogInfo(VB_CHANNELOUT, "Remapped Channels:   %d-%d => %d-%d (%d total channels copied in %d loop(s))\n",
            sourceChannel, sourceChannel + count - 1,
            destChannel, destChannel + (count * loops) - 1, (count * loops), loops);
    
    //channel numbers need to be 0 based
    --destChannel;
    --sourceChannel;
}

RemapOutputProcessor::RemapOutputProcessor(int src, int dst, int c, int l) {
    active = true;
    sourceChannel = src - 1;
    destChannel = dst - 1;
    count = c;
    loops = l;
}


RemapOutputProcessor::~RemapOutputProcessor() {
    
}

void RemapOutputProcessor::ProcessData(unsigned char *channelData) const {
    for (int l = 0; l < loops; l++) {
        if (count > 1) {
            memcpy(channelData + destChannel + (l * count),
                   channelData + sourceChannel,
                   count);
        } else {
            channelData[destChannel + l] = channelData[sourceChannel];
        }
    }

}
