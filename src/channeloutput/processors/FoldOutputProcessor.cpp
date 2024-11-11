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

#include "FoldOutputProcessor.h"

FoldOutputProcessor::FoldOutputProcessor(const Json::Value& config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    sourceChannel = config["source"].asInt() - 1;
    count = config["count"].asInt();
    node = config["node"].asInt(); // By channel, RGB Pixel, RGBW Pixel

    nodesize = 1;
    if (node == 1) {
        nodesize = 3;
    } else if (node == 2) {
        nodesize = 4;
    }
    LogInfo(VB_CHANNELOUT, "Fold: channels: %d-%d, node size: %d\n",
            sourceChannel, sourceChannel + count * nodesize, nodesize);
    if (node < 0 || node > 2) {
        LogWarn(VB_CHANNELOUT,
                "Unknown node option (%d), defaulting to 0 (By Channel)\n",
                node);
    }
    if (count % node != 0) {
        LogWarn(VB_CHANNELOUT,
                "Channel count (%d) is not a multiple of the node size "
                "(%d channels/node), the dangling channels will be ignored\n",
                sourceChannel, sourceChannel + count, nodesize, node);
    }
}

FoldOutputProcessor::FoldOutputProcessor(int src, int c, int n) {
    active = true;
    sourceChannel = src;
    count = c;
    nodesize = n;
}

FoldOutputProcessor::~FoldOutputProcessor() {
}
void FoldOutputProcessor::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(sourceChannel, sourceChannel + count);
}

void FoldOutputProcessor::ProcessData(unsigned char* channelData) const {
    size_t nsize = 3; // Node size, 3 bytes for an RGB pixel
    size_t len = count / nodesize;
    // Copy the required section of channel data to a temporary buffer
    unsigned char* tempBuffer = new unsigned char[count];
    memcpy(tempBuffer, channelData + sourceChannel, count);
    for (unsigned int node = 0; node < len; ++node) {
        memcpy(
            channelData + sourceChannel +
                (node % 2 ? len - node / 2 - 1 : node / 2) * nodesize,
            tempBuffer + node * nsize,
            nsize);
    }
    delete[] tempBuffer; // Delete the temporary buffer
}
