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
#include "../../overlays/PixelOverlay.h"
#include "../../overlays/PixelOverlayModel.h"

#include <math.h>
#include <stdio.h>

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

#include "../../log.h"

#include "ScaleValueOutputProcessor.h"
#include "OutputProcessor.h"

ScaleValueOutputProcessor::ScaleValueOutputProcessor(const Json::Value& config) {
    description = config["description"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    scale = config["scale"].asFloat();

    ProcessModelConfig(config, model, start, count);

    LogInfo(VB_CHANNELOUT, "Scale Channel Value:   %d-%d => Scale: %f Model: %s\n",
            start + 1, start + count,
            scale, model.c_str());

    float sf = scale;
    for (int x = 0; x < 256; x++) {
        float f = x * scale;
        if (f > 255.0) {
            f = 255.0;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        table[x] = round(f);
    }
}

ScaleValueOutputProcessor::~ScaleValueOutputProcessor() {
}

void ScaleValueOutputProcessor::ProcessData(unsigned char* channelData) const {
    for (int x = 0; x < count; x++) {
        channelData[start + x] = table[channelData[start + x]];
    }
}
