/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
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

#include "ClampValueOutputProcessor.h"

ClampValueOutputProcessor::ClampValueOutputProcessor(const Json::Value& config) {
    description = config["description"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    value = config["value"].asInt();
    ProcessModelConfig(config, model, start, count);
    LogInfo(VB_CHANNELOUT, "Clamp Channel Value:   %d-%d => %d, Model: %s\n",
            start, start + (count -1),
            value, model.c_str());
}

ClampValueOutputProcessor::~ClampValueOutputProcessor() {
}

void ClampValueOutputProcessor::ProcessData(unsigned char* channelData) const {
    const int maxVal = value;
    for (int x = 0; x < count; x++) {
        unsigned char& val = channelData[start+x-1];
        if (val > maxVal) {
          val = static_cast<unsigned char>(maxVal);
          LogExcess(VB_CHANNELOUT, "Clamping Channel Value from %d to %d\n",
            maxVal, val);
        }
    }
}
