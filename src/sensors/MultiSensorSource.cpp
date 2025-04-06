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

#include "MultiSensorSource.h"

MultiSensorSource::MultiSensorSource(Json::Value& config) :
    SensorSource(config) {
    for (int x = 0; x < config["sources"].size(); x++) {
        std::string sourcename = config["sources"][x]["source"].asString();
        uint32_t chan = config["sources"][x]["channels"].asInt();
        SensorSource* src = Sensors::INSTANCE.getSensorSource(sourcename);
        if (src) {
            sources.push_back(src);
            channels.push_back(chan);
        }
    }
}
MultiSensorSource::~MultiSensorSource() {
}

void MultiSensorSource::Init(std::map<int, std::function<bool(int)>>& callbacks) {
    std::map<int, std::function<bool(int)>> cb;
    for (auto& a : sources) {
        if (a) {
            a->Init(cb);
        }
    }
}

void MultiSensorSource::lockToGroup(int i) {
    for (auto& a : sources) {
        if (a) {
            a->lockToGroup(i);
        }
    }
}

void MultiSensorSource::update(bool forceInstant) {
    for (auto& a : sources) {
        if (a) {
            a->update(forceInstant);
        }
    }
}
void MultiSensorSource::enable(int id) {
    for (int x = 0; x < sources.size(); x++) {
        if (id < channels[x]) {
            sources[x]->enable(id);
            return;
        }
        id -= channels[x];
    }
}
int32_t MultiSensorSource::getValue(int id) {
    for (int x = 0; x < sources.size(); x++) {
        if (id < channels[x]) {
            return sources[x]->getValue(id);
        }
        id -= channels[x];
    }
    return 0;
}
