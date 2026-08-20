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

#include "fpp-json.h"

#include "../../Warnings.h"
#include "../../log.h"

#include "FoldOutputProcessor.h"

FoldOutputProcessor::FoldOutputProcessor(const Json::Value& config) {
    description = config["description"].asString();
    active = config["active"].asInt() ? true : false;
    sourceChannel = config["source"].asInt() - 1;
    count = config["count"].asInt();
    node = config["node"].asInt(); // By channel, RGB Pixel, RGBW Pixel

    // node is the UI selector, nodesize the channels per node it selects.  All of
    // the fold index math below is in nodesize, never in the raw selector.
    if (node == 1) {
        nodesize = 3;
    } else if (node == 2) {
        nodesize = 4;
    } else {
        if (node != 0) {
            std::string warning = "Fold output processor has an unknown node option (" +
                                  std::to_string(node) + "), falling back to By Channel";
            LogWarn(VB_CHANNELOUT, "%s\n", warning.c_str());
            WarningHolder::AddWarning(warning);
            node = 0;
        }
        nodesize = 1;
    }
    if (count < 0) {
        std::string warning = "Fold output processor has a negative channel count (" +
                              std::to_string(count) + "), disabling it";
        LogWarn(VB_CHANNELOUT, "%s\n", warning.c_str());
        WarningHolder::AddWarning(warning);
        count = 0;
    }
    LogInfo(VB_CHANNELOUT, "Fold: channels: %d-%d, node size: %d\n",
            sourceChannel, sourceChannel + count, nodesize);
    if (count % nodesize != 0) {
        LogWarn(VB_CHANNELOUT,
                "Channel count (%d) is not a multiple of the node size "
                "(%d channels/node), the dangling channels will be ignored\n",
                count, nodesize);
    }
    tempBuffer.resize(count);
}

FoldOutputProcessor::FoldOutputProcessor(int src, int c, int n) {
    active = true;
    sourceChannel = src;
    count = c < 0 ? 0 : c;
    nodesize = n > 0 ? n : 1;
    node = nodesize == 4 ? 2 : (nodesize == 3 ? 1 : 0);
    tempBuffer.resize(count);
}

FoldOutputProcessor::~FoldOutputProcessor() {
}
void FoldOutputProcessor::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(sourceChannel, sourceChannel + count);
}

void FoldOutputProcessor::ProcessData(unsigned char* channelData) const {
    size_t len = count / nodesize; // Number of whole nodes; trailing channels are left alone
    if (len == 0) {
        return;
    }
    // The fold reads every source node and writes it elsewhere in the same span,
    // so the pre-fold values have to be snapshotted first.
    memcpy(tempBuffer.data(), channelData + sourceChannel, count);
    for (unsigned int node = 0; node < len; ++node) {
        memcpy(
            channelData + sourceChannel +
                (node % 2 ? len - node / 2 - 1 : node / 2) * nodesize,
            tempBuffer.data() + node * nodesize,
            nodesize);
    }
}
