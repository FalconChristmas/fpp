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

#include "OverrideZeroOutputProcessor.h"

OverrideZeroOutputProcessor::OverrideZeroOutputProcessor(const Json::Value& config) {
    description = config["description"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    value = config["value"].asInt();
    ProcessModelConfig(config, model, start, count);
    LogInfo(VB_CHANNELOUT, "Override Zero Channel Range:   %d-%d Value: %d, Model: %s\n",
            start + 1, start + count, value, model.c_str());
}

OverrideZeroOutputProcessor::~OverrideZeroOutputProcessor() {
}

void OverrideZeroOutputProcessor::ProcessData(unsigned char* channelData) const {
    for (int x = 0; x < count; x++) {
        if (channelData[x + start] == 0) {
            channelData[x + start] = value;
        }
    }
}
