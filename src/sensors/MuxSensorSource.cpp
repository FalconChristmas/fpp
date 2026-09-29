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

#include "fpp-json.h"
#include <cstdint>
#include <thread>

#include "../log.h"

#include "MuxSensorSource.h"

// Bounds for user-editable sensors.json. The largest realistic mux is a
// handful of select pins (2^N groups) over an 8/16-channel ADC; anything
// near these caps is corrupt or malicious. Capping keeps the per-update
// loops and the enable()/getValue() vectors bounded.
static constexpr int kMaxChannelsPerMux = 1024;
static constexpr int kMaxMuxCount = 1024;
static constexpr int kMaxTotalMuxChannels = 4096;
static constexpr int kMaxMuxSwitchDelayMs = 5000;
static constexpr int kMaxMuxSwitchReadCount = 100;

MuxSensorSource::MuxSensorSource(Json::Value& config) :
    SensorSource(config) {
    std::string sourcename = config["source"].asString();
    source = Sensors::INSTANCE.getSensorSource(sourcename);
    channelsPerMux = config["channels"].asInt();
    muxCount = config["muxCount"].asInt();
    if (channelsPerMux <= 0 || muxCount <= 0 ||
        channelsPerMux > kMaxChannelsPerMux || muxCount > kMaxMuxCount ||
        (int64_t)channelsPerMux * (int64_t)muxCount > kMaxTotalMuxChannels) {
        // User-editable sensors.json: a zero/negative value would divide by
        // zero in enable()/getValue(), and a huge value would hang the
        // per-update loops or OOM the vectors. Fail creation instead --
        // addSensorSources warns and drops sources where isOK() is false.
        LogErr(VB_GENERAL, "MuxSensorSource '%s' has invalid channels (%d) or muxCount (%d), ignoring\n",
               getID().c_str(), (int)channelsPerMux, (int)muxCount);
        source = nullptr;
        return;
    }
    if (source == nullptr) {
        // Unknown "source" name. The object would be dropped by isOK() below,
        // but return before touching GPIO so a doomed object has no HW side
        // effects.
        LogErr(VB_GENERAL, "MuxSensorSource '%s' references unknown source '%s', ignoring\n",
               getID().c_str(), sourcename.c_str());
        return;
    }
    for (int x = 0; x < config["muxPins"].size(); x++) {
        const PinCapabilities& pc = PinCapabilities::getPinByName(config["muxPins"][x].asString());
        if (pc.ptr() == nullptr) {
            // Unknown pin name yields the null pin (ptr() == nullptr); using it
            // below would crash. Fail creation the same way as above.
            LogErr(VB_GENERAL, "MuxSensorSource '%s' references unknown pin '%s', ignoring\n",
                   getID().c_str(), config["muxPins"][x].asString().c_str());
            source = nullptr;
            pins.clear();
            return;
        }
        pins.push_back(pc.ptr());
    }
    for (auto p : pins) {
        p->configPin("gpio", true, sourcename + "-Mux");
        p->setValue(0);
    }
    if (config.isMember("muxSwitchDelay")) {
        muxSwitchDelay = config["muxSwitchDelay"].asInt();
        if (muxSwitchDelay < 0) {
            muxSwitchDelay = 0;
        } else if (muxSwitchDelay > kMaxMuxSwitchDelayMs) {
            muxSwitchDelay = kMaxMuxSwitchDelayMs;
        }
    }
    if (config.isMember("muxSwitchReadCount")) {
        muxSwitchReadCount = config["muxSwitchReadCount"].asInt();
        if (muxSwitchReadCount < 0) {
            muxSwitchReadCount = 0;
        } else if (muxSwitchReadCount > kMaxMuxSwitchReadCount) {
            muxSwitchReadCount = kMaxMuxSwitchReadCount;
        }
    }
}
MuxSensorSource::~MuxSensorSource() {
    for (auto p : pins) {
        p->releasePin();
    }
}

void MuxSensorSource::Init(std::map<int, std::function<bool(int)>>& callbacks) {
    std::map<int, std::function<bool(int)>> cb;
    if (!source) {
        return;
    }
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
    if (!source || channelsPerMux <= 0 || muxCount <= 0) {
        return;
    }
    if (curMux < 0 || curMux >= muxCount) {
        curMux = 0;
    }
    int cm = curMux;
    int64_t start64 = (int64_t)cm * (int64_t)channelsPerMux;
    for (int x = 0; x < channelsPerMux; x++) {
        int64_t idx = start64 + x;
        if (idx < 0 || (uint64_t)idx >= enabled.size() || (uint64_t)idx >= current.size()) {
            continue;
        }
        if (enabled[(size_t)idx] && !current[(size_t)idx]) {
            getValues();
        }
    }
    if ((curMux + 1) >= muxCount) {
        curMux = 0;
    } else {
        curMux = 1 + curMux;
    }
    updateCount = 0;
    cm = curMux;
    start64 = (int64_t)cm * (int64_t)channelsPerMux;
    for (int x = 0; x < channelsPerMux; x++) {
        int64_t idx = start64 + x;
        if (idx < 0 || (uint64_t)idx >= current.size()) {
            continue;
        }
        current[(size_t)idx] = false;
    }
    setGroupPins();
}
void MuxSensorSource::setGroupPins() {
    if (!source) {
        return;
    }
    int tmp = curMux;
    for (auto& a : pins) {
        a->setValue(tmp & 0x1 ? 1 : 0);
        tmp >>= 1;
    }
    if (muxSwitchDelay > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(muxSwitchDelay));
    }
    for (int x = 0; x < muxSwitchReadCount; x++) {
        source->update(true);
    }
}
void MuxSensorSource::lockToGroup(int i) {
    if (!source || channelsPerMux <= 0 || muxCount <= 0) {
        return;
    }
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
    if (!source || channelsPerMux <= 0 || muxCount <= 0) {
        return;
    }
    if (curMux < 0 || curMux >= muxCount) {
        return;
    }
    int cm = curMux;
    int64_t start = (int64_t)cm * (int64_t)channelsPerMux;
    for (int x = 0; x < channelsPerMux; x++) {
        int64_t idx = start + x;
        if (idx < 0 || (uint64_t)idx >= enabled.size() ||
            (uint64_t)idx >= values.size() || (uint64_t)idx >= current.size()) {
            continue;
        }
        if (enabled[(size_t)idx]) {
            values[(size_t)idx] = source->getValue(x);
            current[(size_t)idx] = true;
        }
    }
}

void MuxSensorSource::update(bool forceInstant) {
    if (!source || channelsPerMux <= 0 || muxCount <= 0) {
        return;
    }
    if (!updatingByCallback) {
        source->update(forceInstant || (updateCount == 0));
        updateCount = 1 + updateCount;
        if (updateCount == 4) {
            nextMux();
        } else {
            int cm = curMux;
            int64_t start = (int64_t)cm * (int64_t)channelsPerMux;
            for (int x = 0; x < channelsPerMux; x++) {
                int64_t idx = start + x;
                if (idx < 0 || (uint64_t)idx >= current.size()) {
                    continue;
                }
                current[(size_t)idx] = false;
            }
        }
    }
}
void MuxSensorSource::enable(int id) {
    if (!source || channelsPerMux <= 0 || muxCount <= 0 || id < 0) {
        return;
    }
    // Valid mux channels are 0 .. channelsPerMux*muxCount-1. Anything else is
    // a corrupt sensors.json entry: ignore it instead of growing the vectors
    // without bound (OOM) via resize(id+1).
    int64_t total = (int64_t)channelsPerMux * (int64_t)muxCount;
    if ((int64_t)id >= total) {
        return;
    }

    int i = id % channelsPerMux;
    source->enable(i);
    if ((size_t)id >= values.size()) {
        size_t need = (size_t)id + 1;
        values.resize(need);
        enabled.resize(need);
        current.resize(need);
    }
    enabled[(size_t)id] = true;
    current[(size_t)id] = false;
    values[(size_t)id] = 0;
}
int32_t MuxSensorSource::getValue(int id) {
    if (!source || channelsPerMux <= 0 || id < 0 || (size_t)id >= values.size()) {
        return 0;
    }
    if ((size_t)id >= enabled.size() || (size_t)id >= current.size()) {
        return values[(size_t)id];
    }
    if (enabled[(size_t)id] && !current[(size_t)id]) {
        int i = id / channelsPerMux;
        if (i == curMux) {
            getValues();
        }
    }
    return values[(size_t)id];
}
