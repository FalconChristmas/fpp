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
    reverse = config["reverse"].asInt();
    LogInfo(VB_CHANNELOUT, "Remapped Channels:   %d-%d => %d-%d (%d total channels copied in %d loop(s))\n",
            sourceChannel, sourceChannel + count - 1,
            destChannel, destChannel + (count * loops) - 1, (count * loops), loops);
    
    //channel numbers need to be 0 based
    --destChannel;
    --sourceChannel;
}

RemapOutputProcessor::RemapOutputProcessor(int src, int dst, int c, int l, int r) {
    active = true;
    sourceChannel = src;
    destChannel = dst;
    count = c;
    loops = l;
    reverse = r;
}


RemapOutputProcessor::~RemapOutputProcessor() {
    
}
void RemapOutputProcessor::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    int min = std::min(sourceChannel, destChannel);
    int max = std::max(sourceChannel, destChannel);
    max += loops * count - 1;
    addRange(min, max);
}

void RemapOutputProcessor::ProcessData(unsigned char *channelData) const {
    switch (reverse) {
        case 0: // No reverse
                for (int l = 0; l < loops; l++) {
                    if (count > 1) {
                        memcpy(channelData + destChannel + (l * count),
                               channelData + sourceChannel,
                               count);
                    } else {
                        channelData[destChannel + l] = channelData[sourceChannel];
                    }
                }
                break;

        case 1: // Reverse channels individually
                for (int l = 0; l < loops; l++) {
                    if (count > 1) {
                        if (!l) { // First loop, reverse while copying
                            for (int c = 0; c < count; c++) {
                                channelData[destChannel + c] = channelData[sourceChannel + count - 1 - c];
                            }
                        } else { // Subsequent loops, just copy first reversed block for speed
                            memcpy(channelData + destChannel + (l * count),
                                   channelData + destChannel,
                                   count);
                        }
                    } else { // Can't reverse 1 channel so just copy
                        channelData[destChannel + l] = channelData[sourceChannel];
                    }
                }
                break;

        case 2: // Reverse as a string of RGB pixels
                for (int l = 0; l < loops; l++) {
                    if (count > 1) {
                        if (!l) { // First loop, reverse pixels while copying
                            for (int c = 0; c < count;) {
                                channelData[destChannel + c + 0] = channelData[sourceChannel + count - 1 - c - 2];
                                channelData[destChannel + c + 1] = channelData[sourceChannel + count - 1 - c - 1];
                                channelData[destChannel + c + 2] = channelData[sourceChannel + count - 1 - c - 0];
                                c += 3;
                            }
                        } else { // Subsequent loops, just copy first reversed block for speed
                            memcpy(channelData + destChannel + (l * count),
                                   channelData + destChannel,
                                   count);
                        }
                    } else {
                        // Shouldn't ever get here, can't reverse pixels if only 1 channel
                        channelData[destChannel + l] = channelData[sourceChannel];
                    }
                }
                break;

        case 3: // Reverse as a string of RGBW pixels
                for (int l = 0; l < loops; l++) {
                    if (count > 1) {
                        if (!l) { // First loop, reverse pixels while copying
                            for (int c = 0; c < count;) {
                                channelData[destChannel + c + 0] = channelData[sourceChannel + count - 1 - c - 3];
                                channelData[destChannel + c + 1] = channelData[sourceChannel + count - 1 - c - 2];
                                channelData[destChannel + c + 2] = channelData[sourceChannel + count - 1 - c - 1];
                                channelData[destChannel + c + 3] = channelData[sourceChannel + count - 1 - c - 0];
                                c += 4;
                            }
                        } else { // Subsequent loops, just copy first reversed block for speed
                            memcpy(channelData + destChannel + (l * count),
                                   channelData + destChannel,
                                   count);
                        }
                    } else {
                        // Shouldn't ever get here, can't reverse pixels if only 1 channel
                        channelData[destChannel + l] = channelData[sourceChannel];
                    }
                }
                break;
    }
}
