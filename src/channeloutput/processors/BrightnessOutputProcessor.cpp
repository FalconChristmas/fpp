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

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

#include "../../log.h"

#include "BrightnessOutputProcessor.h"

BrightnessOutputProcessor::BrightnessOutputProcessor(const Json::Value& config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    brightness = config["brightness"].asInt();
    gamma = config["gamma"].asFloat();

    if (config.isMember("model")) {
        model = config["model"].asString();
        if (model != "&lt;Use Start Channel&gt;") {
            auto m_model = PixelOverlayManager::INSTANCE.getModel(model);
            if (!m_model) {
                LogErr(VB_CHANNELOUT, "Invalid Pixel Overlay Model: '%s'\n", model.c_str());
            } else {
                int m_channel = m_model->getChannelCount();
                LogDebug(VB_CHANNELOUT, "Before Model applied Brightness:   %d-%d => Brightness:%d   Gamma: %f   Model: %s model\n",
                   start, start + count - 1,
                   brightness, gamma, model.c_str(),m_channel);
                int offset = start;
                start = m_model->getStartChannel() + start + 1;
                if (count > m_channel) {
                   count = m_channel - offset;
                   LogWarn(VB_CHANNELOUT, "Output processor tried to go past end channel of model.  Restricting to %d channels\n", count);
               } else if (count < m_channel) {
                   LogInfo(VB_CHANNELOUT, "Output processor tried to use less channels (%d) than overlay model has (%d).  This may be intentional\n",count, m_channel);
               }
            }
        }
    } else {
        model = "";
    }

    LogInfo(VB_CHANNELOUT, "Brightness:   %d-%d => Brightness:%d   Gamma: %f   Model: %s\n",
            start, start + count - 1,
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

    // channel numbers need to be 0 based
    --start;
}

BrightnessOutputProcessor::~BrightnessOutputProcessor() {
}

void BrightnessOutputProcessor::ProcessData(unsigned char* channelData) const {
    for (int x = 0; x < count; x++) {
        channelData[start + x] = table[channelData[start + x]];
    }
}
