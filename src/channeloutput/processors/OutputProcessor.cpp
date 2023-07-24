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

#include "OutputProcessor.h"

#include "BrightnessOutputProcessor.h"
#include "ColorOrderOutputProcessor.h"
#include "HoldValueOutputProcessor.h"
#include "OverrideZeroOutputProcessor.h"
#include "RemapOutputProcessor.h"
#include "SetValueOutputProcessor.h"
#include "ThreeToFourOutputProcessor.h"

OutputProcessors::OutputProcessors() {
}
OutputProcessors::~OutputProcessors() {
    for (OutputProcessor* a : processors) {
        delete a;
    }
    processors.clear();
}

void OutputProcessors::ProcessData(unsigned char* channelData) const {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor* a : processors) {
        if (a->isActive()) {
            a->ProcessData(channelData);
        }
    }
}

void OutputProcessors::addProcessor(OutputProcessor* p) {
    if (p == nullptr) {
        return;
    }
    std::lock_guard<std::mutex> lock(processorsLock);
    processors.push_back(p);
}
void OutputProcessors::removeProcessor(OutputProcessor* p) {
    std::lock_guard<std::mutex> lock(processorsLock);
    processors.remove(p);
}
void OutputProcessors::removeAll() {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor* a : processors) {
        delete a;
    }
    processors.clear();
}

void OutputProcessors::loadFromJSON(const Json::Value& config, bool clear) {
    if (clear) {
        removeAll();
    }
    for (Json::Value::const_iterator itr = config.begin(); itr != config.end(); ++itr) {
        std::string name = itr.key().asString();
        if (name == "outputProcessors") {
            Json::Value val = *itr;
            if (val.isArray()) {
                for (int x = 0; x < val.size(); x++) {
                    addProcessor(create(val[x]));
                }
            } else {
                addProcessor(create(val));
            }
        }
    }
}
OutputProcessor* OutputProcessors::create(const Json::Value& config) {
    std::string type = config["type"].asString();
    if (type == "Remap") {
        return new RemapOutputProcessor(config);
    } else if (type == "Brightness") {
        return new BrightnessOutputProcessor(config);
    } else if (type == "Hold Value") {
        return new HoldValueOutputProcessor(config);
    } else if (type == "Set Value") {
        return new SetValueOutputProcessor(config);
    } else if (type == "Reorder Colors") {
        return new ColorOrderOutputProcessor(config);
    } else if (type == "Three to Four") {
        return new ThreeToFourOutputProcessor(config);
    } else if (type == "Override Zero") {
        return new OverrideZeroOutputProcessor(config);
    } else {
        LogErr(VB_CHANNELOUT, "Unknown OutputProcessor type: %s\n", type.c_str());
    }
    return nullptr;
}

OutputProcessor* OutputProcessors::find(std::function<bool(OutputProcessor*)> f) const {
    std::lock_guard<std::mutex> lock(processorsLock);
    for (OutputProcessor* a : processors) {
        if (f(a)) {
            return a;
        }
    }
    return nullptr;
}

void OutputProcessors::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    for (OutputProcessor* a : processors) {
        a->GetRequiredChannelRanges(addRange);
    }
}

OutputProcessor::OutputProcessor() :
    description(),
    active(true) {
}

OutputProcessor::~OutputProcessor() {
}
