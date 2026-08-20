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
#include "../../overlays/PixelOverlay.h"
#include "../../overlays/PixelOverlayModel.h"

#include <math.h>
#include <stdio.h>

#include "fpp-json.h"

#include "../../Warnings.h"
#include "../../log.h"

#include "BrightnessOutputProcessor.h"
#include "OutputProcessor.h"

BrightnessOutputProcessor::BrightnessOutputProcessor(const Json::Value& config) {
    description = config["description"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    brightness = config["brightness"].asInt();
    gamma = config["gamma"].asFloat();

    // A blank UI field arrives as JSON null and a config predating the field
    // has no key at all; both read back as 0 -- and pow(x, 0) is 1, so an
    // unvalidated zero gamma builds a table that drives EVERY input, including
    // dark channels, at the full brightness value.
    if (!config.isMember("brightness") || config["brightness"].isNull()) {
        WarningHolder::AddWarning("Brightness output processor '" + description + "' has no brightness value, using 100");
        brightness = 100;
    } else if (brightness < 0 || brightness > 100) {
        WarningHolder::AddWarning("Brightness output processor '" + description + "' has an invalid brightness (" + std::to_string(brightness) + "), clamping");
        brightness = brightness < 0 ? 0 : 100;
    }
    if (!config.isMember("gamma") || config["gamma"].isNull()) {
        WarningHolder::AddWarning("Brightness output processor '" + description + "' has no gamma value, using 1.0");
        gamma = 1.0f;
    } else if (gamma < 0.01f || gamma > 50.0f) {
        WarningHolder::AddWarning("Brightness output processor '" + description + "' has an invalid gamma (" + std::to_string(gamma) + "), using 1.0");
        gamma = 1.0f;
    }

    ProcessModelConfig(config, model, start, count);

    LogInfo(VB_CHANNELOUT, "Brightness:   %d-%d => Brightness:%d   Gamma: %f   Model: %s\n",
            start + 1, start + count,
            brightness, gamma, model.c_str());

    float bf = brightness;
    float maxB = bf * 2.55f;
    for (int x = 0; x < 256; x++) {
        float f = x;
        f = maxB * pow(f / 255.0f, gamma);
        if (f > 255.0) {
            f = 255.0;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        table[x] = round(f);
    }
}

BrightnessOutputProcessor::~BrightnessOutputProcessor() {
}

void BrightnessOutputProcessor::ProcessData(unsigned char* channelData) const {
    for (int x = 0; x < count; x++) {
        channelData[start + x] = table[channelData[start + x]];
    }
}
