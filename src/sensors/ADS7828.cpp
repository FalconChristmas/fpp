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
    for (int x = 0; x < 8; x++) {
        enabled[x] = false;
    }
}

ADS7828Sensor::~ADS7828Sensor() {
    delete i2c;
}

int32_t ADS7828Sensor::getValue(int ch) {
    if (enabled[ch] && i2c->isOk()) {
        std::unique_lock<std::mutex> lock(updateMutex);
        uint8_t baseCmd = 0x80 | (internalRefVoltage ? 0xC : 0x4);
        uint8_t cmd = baseCmd | (((ch >> 1) | (ch & 0x01) << 2) << 4);
        constexpr int samples = 4;
        uint16_t b2[samples] = { 0, 0, 0, 0 };
        int c = i2c->readI2CBlockData(cmd, (uint8_t*)b2, samples * 2 - 2);
        while (c < (samples * 2)) {
            c += i2c->readI2CBlockData(cmd, (uint8_t*)&b2[c / 2], samples * 2 - c);
        }
        uint32_t sum = 0;
        for (int x = 0; x < samples; x++) {
            sum += bswap_16(b2[x]);
        }
        return sum / samples;
    }
    return 0;
};