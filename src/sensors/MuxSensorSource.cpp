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

#include "MuxSensorSource.h"

MuxSensorSource::MuxSensorSource(Json::Value& config) :
    SensorSource(config) {
    std::string sourcename = config["source"].asString();
    source = Sensors::INSTANCE.getSensorSource(sourcename);
    channelsPerMux = config["channels"].asInt();
    muxCount = config["muxCount"].asInt();
    for (int x = 0; x < config["muxPins"].size(); x++) {
        pins.push_back(PinCapabilities::getPinByName(config["muxPins"][x].asString()).ptr());
    }
    for (auto p : pins) {
        p->configPin("gpio");
        p->setValue(0);
    }
}
MuxSensorSource::~MuxSensorSource() {
}

void MuxSensorSource::Init(std::map<int, std::function<bool(int)>>& callbacks) {
    std::map<int, std::function<bool(int)>> cb;
    source->Init(cb);
    updatingByCallback = false;
    if (!cb.empty()) {
        updatingByCallback = true;
        for (auto& c : cb) {
            auto call = c.second;
            callbacks[c.first] = [call, this](int i) {
                call(i);
                if (updateCount > 0) {
                    getValues();
                }
                updateCount = 1 + updateCount;
                if (updateCount == 4) {
                    nextMux();
                }
                return false;
            };
        }
    }
}
void MuxSensorSource::nextMux() {
    if (lockedToGroup) {
        return;
    }
    int start = curMux * channelsPerMux;
    for (int x = 0; x < channelsPerMux; x++) {
        if (enabled[x + start] && !current[x + start]) {
            getValues();
        }
    }
    if ((curMux + 1) >= muxCount) {
        curMux = 0;
    } else {
        curMux = 1 + curMux;
    }
    updateCount = 0;
    start = curMux * channelsPerMux;
    for (int x = 0; x < channelsPerMux; x++) {
        current[x + start] = false;
    }
    setGroupPins();
}
void MuxSensorSource::setGroupPins() {
    int tmp = curMux;
    for (auto& a : pins) {
        a->setValue(tmp & 0x1 ? 1 : 0);
        tmp >>= 1;
    }
}
void MuxSensorSource::lockToGroup(int i) {
    if (i >= 0 && i < muxCount) {
        lockedToGroup = true;
        if (curMux != i) {
            curMux = i;
            updateCount = 0;
        }
        setGroupPins();
    } else if (lockedToGroup) {
        lockedToGroup = false;
        nextMux();
    }
}

void MuxSensorSource::getValues() {
    int start = curMux * channelsPerMux;
    for (int x = 0; x < channelsPerMux; x++) {
        if (enabled[x + start]) {
            values[x + start] = source->getValue(x);
            current[x + start] = true;
        }
    }
}

void MuxSensorSource::update(bool forceInstant) {
    if (!updatingByCallback) {
        source->update(forceInstant || (updateCount == 0));
        updateCount = 1 + updateCount;
        if (updateCount == 4) {
            nextMux();
        } else {
            int start = curMux * channelsPerMux;
            for (int x = 0; x < channelsPerMux; x++) {
                current[x + start] = false;
            }
        }
    }
}
void MuxSensorSource::enable(int id) {
    int i = id % channelsPerMux;
    source->enable(i);
    if (id >= values.size()) {
        values.resize(id + 1);
        enabled.resize(id + 1);
        current.resize(id + 1);
    }
    enabled[id] = true;
    current[id] = false;
    values[id] = 0;
}
int32_t MuxSensorSource::getValue(int id) {
    if (enabled[id] && !current[id]) {
        int i = id / channelsPerMux;
        if (i == curMux) {
            getValues();
        }
    }
    return values[id];
}
