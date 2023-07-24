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

#include "ColorOrderOutputProcessor.h"

ColorOrderOutputProcessor::ColorOrderOutputProcessor(const Json::Value& config) {
    description = config["desription"].asString();
    active = config["active"].asInt() ? true : false;
    start = config["start"].asInt();
    count = config["count"].asInt();
    order = config["colorOrder"].asInt();
    LogInfo(VB_CHANNELOUT, "Color Order:   %d-%d => %d\n",
            start, start + (count * 3) - 1,
            order);

    //channel numbers need to be 0 based
    --start;
}

ColorOrderOutputProcessor::~ColorOrderOutputProcessor() {
}

void ColorOrderOutputProcessor::ProcessData(unsigned char* channelData) const {
    int cur = start;
    for (int x = 0; x < count; x++, cur += 3) {
        int a = channelData[cur];
        int b = channelData[cur + 1];
        int c = channelData[cur + 2];
        switch (order) {
        case 132:
            channelData[cur + 1] = c;
            channelData[cur + 2] = b;
            break;
        case 213:
            channelData[cur] = b;
            channelData[cur + 1] = a;
            break;
        case 231:
            channelData[cur] = b;
            channelData[cur + 1] = c;
            channelData[cur + 2] = a;
            break;
        case 312:
            channelData[cur] = c;
            channelData[cur + 1] = a;
            channelData[cur + 2] = b;
            break;
        case 321:
            channelData[cur] = c;
            channelData[cur + 2] = a;
            break;
        }
    }
}
