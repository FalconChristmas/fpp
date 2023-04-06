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

#include "Sensors.h"

class I2CUtils;

class ADS7828Sensor : public SensorSource {
public:
    ADS7828Sensor(Json::Value& config);
    ~ADS7828Sensor();

    void update(bool forceUpdate = false);

    virtual int32_t getValue(int id) { return values[id]; };

private:
    I2CUtils *i2c = nullptr;
    int i2cBus = 0;
    int i2cAddress = 0x48;
    bool internalRefVoltage = true;
    int bits = 12;
    std::mutex updateMutex;

    std::array<int32_t, 8> values;

    class BufferedData {
    public:
        uint64_t timestamp;
        int32_t  value[8];
    };
    std::list<BufferedData> bufferValues;
};