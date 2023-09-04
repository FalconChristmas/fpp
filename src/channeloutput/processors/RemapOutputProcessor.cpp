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

#include "RemapOutputProcessor.h"

RemapOutputProcessor::RemapOutputProcessor(const Json::Value& config) {
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

    // channel numbers need to be 0 based
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
void RemapOutputProcessor::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    int min = std::min(sourceChannel, destChannel);
    int max = std::max(sourceChannel, destChannel);
    max += loops * count - 1;
    addRange(min, max);
}

void RemapOutputProcessor::ProcessData(unsigned char* channelData) const {
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
                    // Copy the required section of channel data to a temporary buffer
                    unsigned char* tempBuffer = new unsigned char[count];
                    memcpy(tempBuffer, channelData + sourceChannel, count);
                    for (int c = 0; c < count; c++) {
                        channelData[destChannel + c] = tempBuffer[count - 1 - c];
                    }
                    // Delete the temporary buffer
                    delete[] tempBuffer;
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
                    // Copy the required section of channel data to a temporary buffer
                    unsigned char* tempBuffer = new unsigned char[count];
                    memcpy(tempBuffer, channelData + sourceChannel, count);
                    for (int c = 0; c < count - 2;) {
                        channelData[destChannel + c + 0] = tempBuffer[count - 1 - c - 2];
                        channelData[destChannel + c + 1] = tempBuffer[count - 1 - c - 1];
                        channelData[destChannel + c + 2] = tempBuffer[count - 1 - c - 0];
                        c += 3;
                    }
                    // Delete the temporary buffer
                    delete[] tempBuffer;
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
                    // Copy the required section of channel data to a temporary buffer
                    unsigned char* tempBuffer = new unsigned char[count];
                    memcpy(tempBuffer, channelData + sourceChannel, count);
                    for (int c = 0; c < count - 3;) {
                        channelData[destChannel + c + 0] = tempBuffer[count - 1 - c - 3];
                        channelData[destChannel + c + 1] = tempBuffer[count - 1 - c - 2];
                        channelData[destChannel + c + 2] = tempBuffer[count - 1 - c - 1];
                        channelData[destChannel + c + 3] = tempBuffer[count - 1 - c - 0];
                        c += 4;
                    }
                    // Delete the temporary buffer
                    delete[] tempBuffer;
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
