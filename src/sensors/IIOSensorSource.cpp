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

#include <cmath>
#include <fcntl.h>
#include <unistd.h>

#include "../Warnings.h"
#include "../common.h"

#include "IIOSensorSource.h"

IIOSensorSource::IIOSensorSource(Json::Value& config) :
    SensorSource(config) {
    if (config.isMember("devId")) {
        iioDevNumber = config["devId"].asInt();
    }
    if (config.isMember("useBuffers")) {
        usingBuffers = config["useBuffers"].asBool();
    } else {
        usingBuffers = FileExists("/sys/bus/iio/devices/iio:device" + std::to_string(iioDevNumber) + "/buffer/enable") && FileExists("/dev/iio:device" + std::to_string(iioDevNumber));
    }

    if (config.isMember("vScale")) {
        vScale = config["vScale"].asFloat();
    } else {
#ifdef PLATFORM_BB64
        std::string vs = GetFileContents("/sys/bus/iio/devices/iio:device" + std::to_string(iioDevNumber) + "/in_voltage_scale");
        vScale = std::atof(vs.c_str());
        // original reference is 1.8V which for 12bit would be 0.439453125.
        // If the cape doesn't specify a reference scale, we'll adjust and assume
        // the params are based on 1.8 ref, but we're reading to 3.3V
        vScale = vScale / 0.439453125;
#else
        vScale = 1.0f;
#endif
    }

    // usingBuffers = false;
    std::string base = "/sys/bus/iio/devices/iio:device" + std::to_string(iioDevNumber) + "/in_voltage";
    int max = -1;
    for (int x = 0; x < 16; x++) {
        if (FileExists(base + std::to_string(x) + "_raw")) {
            max = x;
        }
    }
    if (max >= 0) {
        ++max;
        channelMapping.resize(max);
        values.resize(max);
        for (int x = 0; x < max; x++) {
            channelMapping[x] = -1;
            values[x] = 0;
        }
    } else {
        WarningHolder::AddWarning("Could not enable IIOSensorSource");
    }
}
IIOSensorSource::~IIOSensorSource() {
    if (usingBuffers) {
        char buf[256];
        snprintf(buf, 256, "/sys/bus/iio/devices/iio:device%d/buffer/enable", iioDevNumber);
        int f = open(buf, O_WRONLY);
        write(f, "0\n", 2);
        close(f);

        close(iioDevFile);
    } else {
        for (int x = 0; x < channelMapping.size(); x++) {
            if (channelMapping[x] != -1) {
                close(channelMapping[x]);
            }
        }
    }
    if (readBuffer) {
        delete[] readBuffer;
    }
}
bool IIOSensorSource::isOK() const {
    return FileExists("/dev/iio:device0");
}

void IIOSensorSource::Init(std::map<int, std::function<bool(int)>>& callbacks) {
    if (usingBuffers) {
        if (iioDevFile >= 0) {
            return;
        }
        char buf[256];
        snprintf(buf, 256, "/sys/bus/iio/devices/iio:device%d/buffer/length", iioDevNumber);
        int f = open(buf, O_WRONLY);
        write(f, "8\n", 2);
        close(f);

        snprintf(buf, 256, "/sys/bus/iio/devices/iio:device%d/buffer/enable", iioDevNumber);
        f = open(buf, O_WRONLY);
        write(f, "1\n", 2);
        close(f);

        snprintf(buf, 256, "/dev/iio:device%d", iioDevNumber);
        iioDevFile = open(buf, O_RDONLY | O_NONBLOCK);

        int curIdx = 0;
        for (int x = 0; x < channelMapping.size(); x++) {
            snprintf(buf, 256, "/sys/bus/iio/devices/iio:device%d/scan_elements/in_voltage%d_en", iioDevNumber, x);
            int f = open(buf, O_RDONLY);
            read(f, buf, 240);
            if (buf[0] == '1') {
                channelMapping[curIdx] = x;
                curIdx++;
            }
            close(f);
        }
        channelMapping.resize(curIdx);
        callbacks[iioDevFile] = [this](int) {
            update(false, true);
            return false;
        };
        readBufferSize = channelMapping.size() * 64 * 2;
        readBuffer = new uint16_t[readBufferSize];
    }
}
void IIOSensorSource::update(bool forceInstant) {
    update(forceInstant, false);
}

void IIOSensorSource::update(bool forceInstant, bool fromSelect) {
    std::unique_lock<std::mutex> lock(updateMutex);
    if (usingBuffers) {
        int i = read(iioDevFile, readBuffer, readBufferSize * sizeof(uint16_t));
        if (i > 1) {
            int countRead = i / sizeof(uint16_t);
            std::vector<uint32_t> sums(channelMapping.size());
            std::vector<uint16_t> mins(channelMapping.size());
            std::vector<uint16_t> maxes(channelMapping.size());
            int samples = 0;
            for (int x = 0; x < channelMapping.size(); x++) {
                maxes[x] = 0;
                mins[x] = 0xFFFF;
                sums[x] = 0;
            }

            for (int y = 0; y < countRead; y += channelMapping.size()) {
                samples++;
                for (int x = 0; x < channelMapping.size(); x++) {
                    uint16_t v = readBuffer[y + x];
                    sums[x] += v;
                    maxes[x] = std::max(maxes[x], v);
                    mins[x] = std::min(mins[x], v);
                }
            }
            if (samples > 3) {
                for (int x = 0; x < channelMapping.size(); x++) {
                    sums[x] -= mins[x];
                    sums[x] -= maxes[x];
                    float f = sums[x];
                    f /= (float)(samples - 2);
                    f *= vScale;

                    int idx = channelMapping[x];
                    values[idx] = f;
                }
            } else {
                for (int x = 0; x < channelMapping.size(); x++) {
                    float f = sums[x];
                    f /= (float)(samples);
                    f *= vScale;
                    int idx = channelMapping[x];
                    values[idx] = f;
                }
            }
        }
    } else {
        char buf[24];
        for (int x = 0; x < channelMapping.size(); x++) {
            if (channelMapping[x] >= 0) {
                float s = 0;
                for (int m = 0; m < 3; m++) {
                    lseek(channelMapping[x], 0, SEEK_SET);
                    int r = read(channelMapping[x], buf, 23);
                    buf[r] = 0;
                    s += std::atoi(buf);
                }
                s /= 3.0;
                s *= vScale;
                values[x] = std::round(s);
            }
        }
    }
}
void IIOSensorSource::enable(int id) {
    if (usingBuffers) {
        char buf[256];
        snprintf(buf, 256, "/sys/bus/iio/devices/iio:device%d/scan_elements/in_voltage%d_en", iioDevNumber, id);
        if (FileExists(buf)) {
            int f = open(buf, O_WRONLY);
            write(f, "1\n", 2);
            close(f);
        }
    } else {
        char buf[256];
        snprintf(buf, 256, "/sys/bus/iio/devices/iio:device%d/in_voltage%d_raw", iioDevNumber, id);
        channelMapping[id] = open(buf, O_RDONLY);
    }
}

int32_t IIOSensorSource::getValue(int id) {
    if (id >= values.size()) {
        return 0;
    }
    return values[id];
};
