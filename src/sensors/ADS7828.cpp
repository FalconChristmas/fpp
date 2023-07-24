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

#if __has_include(<byteswap.h>)
#include <byteswap.h>
#else
inline uint16_t bswap_16(uint16_t num) {
    return (num >> 8) | (num << 8);
}
#endif

#include "../common.h"
#include "../util/I2CUtils.h"

#include "ADS7828.h"

ADS7828Sensor::ADS7828Sensor(Json::Value& config) :
    SensorSource(config) {
    i2cBus = config["i2cBus"].asInt();
    if (config["i2cAddress"].isString()) {
        std::string ad = config["i2cAddress"].asString();
        i2cAddress = std::stoul(ad, nullptr, 16);
    } else {
        i2cAddress = config["i2cAddress"].asInt();
    }
    i2c = new I2CUtils(i2cBus, i2cAddress);
    if (config["type"].asString() == "ads7830") {
        bits = 8;
    } else {
        bits = 12;
    }
    internalRefVoltage = config["internalRefVoltage"].asBool();
}

ADS7828Sensor::~ADS7828Sensor() {
    delete i2c;
}

void ADS7828Sensor::update(bool forceInstant) {
    if (i2c->isOk()) {
        std::unique_lock<std::mutex> lock(updateMutex);
        uint8_t baseCmd = 0x80 | (internalRefVoltage ? 0xC : 0x4);

        // we'll average all the values in the last 10ms
        uint64_t tm = GetTimeMS() - 10;
        if (forceInstant) {
            bufferValues.clear();
        }
        while (!bufferValues.empty() && bufferValues.front().timestamp < tm) {
            bufferValues.pop_front();
        }
        // make sure we have at least 3 values we can "average", but make sure we at
        // least add the current value
        bool first = true;
        while (first || bufferValues.size() < 3) {
            first = false;
            BufferedData data;
            data.timestamp = GetTimeMS();
            for (int ch = 0; ch < 8; ch++) {
                uint8_t cmd = baseCmd | (((ch >> 1) | (ch & 0x01) << 2) << 4);
                if (bits > 8) {
                    // ads7828 sends the data with the byte order flipped
                    uint32_t d = i2c->readWordData(cmd);
                    data.value[ch] = bswap_16(d);
                } else {
                    data.value[ch] = i2c->readByteData(cmd);
                }
            }
            bufferValues.emplace_back(data);
        }

        std::array<uint32_t, 8> v = { 0, 0, 0, 0, 0, 0, 0, 0 };
        int count = 0;
        for (const auto& it : bufferValues) {
            ++count;
            for (int ch = 0; ch < 8; ch++) {
                v[ch] += it.value[ch];
            }
        }
        for (int ch = 0; ch < 8; ch++) {
            values[ch] = v[ch] / count;
        }
        // printf("%d: %2X  %2X  %2X  %2X  %2X  %2X  %2X  %2X\n", count, values[0], values[1], values[2], values[3], values[4], values[5], values[6], values[7]);
    }
}
