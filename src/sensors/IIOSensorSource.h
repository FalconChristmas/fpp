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

#include <array>
#include <mutex>

#include "Sensors.h"

class IIOSensorSource : public SensorSource {
public:
    IIOSensorSource(Json::Value& config);
    virtual ~IIOSensorSource();

    virtual void Init(std::map<int, std::function<bool(int)>>& callbacks) override;

    virtual void update(bool forceInstant = false) override;
    virtual void enable(int id) override;
    virtual int32_t getValue(int id) override;

    virtual bool isOK() const override;

private:
    void update(bool forceInstant, bool fromSelect);
    std::vector<int32_t> values;
    std::mutex updateMutex;

    int iioDevNumber = 0;
    bool usingBuffers = true;
    int iioDevFile = -1;
    std::vector<int> channelMapping;

    uint16_t* readBuffer = nullptr;
    size_t readBufferSize = 0;
    float vScale = 0;
};
