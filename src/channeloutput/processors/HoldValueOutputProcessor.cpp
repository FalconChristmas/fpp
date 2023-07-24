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

#include "HoldValueOutputProcessor.h"

HoldValueOutputProcessor::HoldValueOutputProcessor(const Json::Value& config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    LogInfo(VB_CHANNELOUT, "Hold Channel Values:   %d-%d\n",
            start, start + count - 1);

    // channel numbers need to be 0 based
    --start;

    lastValues = new unsigned char[count];
}

HoldValueOutputProcessor::~HoldValueOutputProcessor() {
    delete[] lastValues;
}

void HoldValueOutputProcessor::ProcessData(unsigned char* channelData) const {
    for (int x = 0; x < count; x++) {
        if (channelData[x + start] == 0) {
            channelData[x + start] = lastValues[x];
        } else {
            lastValues[x] = channelData[x + start];
        }
    }
}
