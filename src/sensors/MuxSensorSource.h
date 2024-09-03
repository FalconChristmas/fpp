#pragma once
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

#include "Sensors.h"
#include "../util/GPIOUtils.h"
#include <array>
#include <mutex>

class MuxSensorSource : public SensorSource {
public:
    MuxSensorSource(Json::Value& config);
    virtual ~MuxSensorSource();

    virtual void Init(std::map<int, std::function<bool(int)>>& callbacks) override;

    virtual void update(bool forceInstant = false) override;
    virtual void enable(int id) override;
    virtual int32_t getValue(int id) override;

    virtual void lockToGroup(int i) override;

private:
    void setGroupPins();
    void nextMux();
    void getValues();
    std::vector<int32_t> values;
    std::vector<bool> enabled;
    std::vector<bool> current;

    std::vector<const PinCapabilities*> pins;
    SensorSource* source = nullptr;
    int channelsPerMux = 0;
    int muxCount = 0;
    bool updatingByCallback = false;

    volatile int curMux = 0;
    volatile int updateCount = 0;
    volatile bool lockedToGroup = false;
};
