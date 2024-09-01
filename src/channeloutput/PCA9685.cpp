/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include <cinttypes>
#include <cmath>
#include <thread>

#include "PCA9685.h"
#include "Plugin.h"
#include "../common.h"
#include "../log.h"

#include "non-gpl/CapeUtils/CapeUtils.h"

#include "util/GPIOUtils.h"

class PCA9685Plugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    PCA9685Plugin() :
        FPPPlugins::Plugin("PCA9685") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new PCA9685Output(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new PCA9685Plugin();
}
}

/*
 *
 */
PCA9685Output::PCA9685Output(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    i2c(nullptr) {
    LogDebug(VB_CHANNELOUT, "PCA9685Output::PCA9685Output(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
PCA9685Output::~PCA9685Output() {
    LogDebug(VB_CHANNELOUT, "PCA9685Output::~PCA9685Output()\n");
    if (i2c) {
        delete i2c;
    }
}

static PCA9685Output::ZeroType decodeZTE(int i) {
    switch (i) {
    case 0:
        return PCA9685Output::ZeroType::HOLD;
    case 1:
        return PCA9685Output::ZeroType::NORMAL;
    case 2:
        return PCA9685Output::ZeroType::CENTER;
    case 3:
        return PCA9685Output::ZeroType::OFF;
    }
    return PCA9685Output::ZeroType::HOLD;
}

static PCA9685Output::ZeroType decodeZTE(const std::string& i) {
    if (i == "Hold") {
        return PCA9685Output::ZeroType::HOLD;
    }
    if (i == "Min") {
        return PCA9685Output::ZeroType::MIN;
    }
    if (i == "Max") {
        return PCA9685Output::ZeroType::MAX;
    }
    if (i == "Normal") {
        return PCA9685Output::ZeroType::NORMAL;
    }
    if (i == "Center") {
        return PCA9685Output::ZeroType::CENTER;
    }
    return PCA9685Output::ZeroType::OFF;
}
static PCA9685Output::PWMType decodePWMType(const std::string& t) {
    if (t == "LED") {
        return PCA9685Output::PWMType::LED;
    }
    return PCA9685Output::PWMType::SERVO;
}
static PCA9685Output::DataType decodeDataType(const std::string& i) {
    if (i == "Scaled") {
        return PCA9685Output::DataType::SCALED;
    }
    if (i == "2x Absolute") {
        return PCA9685Output::DataType::ABSOLUTE_DOUBLE;
    }
    if (i == "1/2 Absolute") {
        return PCA9685Output::DataType::ABSOLUTE_HALF;
    }
    return PCA9685Output::DataType::ABSOLUTE;
}
static int decodeFrequency(const std::string& fr) {
    int it = atoi(fr.c_str());
    if (it > 0) {
        return it;
    }
    return 50;
}

unsigned short PCA9685Output::PCA9685Port::readValue(unsigned char* channelData, float frequency, int port) {
    uint32_t val = channelData[startChannel];
    if (is16Bit) {
        uint32_t t = channelData[startChannel + 1];
        val = val << 8;
        val += t;
    }
    if (type == PWMType::SERVO) {
        if (val == 0) {
            switch (m_zeroBehavior) {
            case PCA9685Output::ZeroType::HOLD:
                val = m_lastValue;
                break;
            case PCA9685Output::ZeroType::CENTER:
                val = m_center;
                break;
            case PCA9685Output::ZeroType::MIN:
                if (m_reverse) {
                    val = m_max;
                } else {
                    val = m_min;
                }
                break;
            case PCA9685Output::ZeroType::MAX:
                if (m_reverse) {
                    val = m_min;
                } else {
                    val = m_max;
                }
                break;
            }
        } else {
            float z = val;
            DataType dt = m_dataType;
            if (dt == PCA9685Output::DataType::ABSOLUTE_DOUBLE) {
                z /= 2.0f;
                dt == PCA9685Output::DataType::ABSOLUTE;
            } else if (dt == PCA9685Output::DataType::ABSOLUTE) {
                z *= 2.0f;
                dt == PCA9685Output::DataType::ABSOLUTE;
            }
            if (dt == PCA9685Output::DataType::ABSOLUTE) {
                z *= frequency;
                z *= 4096.0f;
                z /= 1000000.0f;
                val = std::round(z);
                if (!is16Bit) {
                    val += m_min;
                }
            } else if (m_dataType == PCA9685Output::DataType::SCALED) {
                if (is16Bit) {
                    if (val >= 32768) {
                        float scale = m_max - m_center + 1;
                        val -= 32768.0f;
                        scale *= val;
                        scale /= 32768.0f;
                        val = std::round(scale);
                        val += m_center;
                    } else {
                        float scale = m_center - m_min + 1;
                        scale *= val;
                        scale /= 32767.0f;
                        val = std::round(scale);
                        val += m_min;
                    }
                } else {
                    if (val >= 128) {
                        float scale = m_max - m_center + 1;
                        val -= 128.0f;
                        scale *= val;
                        scale /= 128.0f;
                        val = std::round(scale);
                        val += m_center;
                    } else {
                        float scale = m_center - m_min + 1;
                        scale *= val;
                        scale /= 128.0f;
                        val = std::round(scale);
                        val += m_min;
                    }
                }
            }
            if (val > m_max) {
                val = m_max;
            }
            if (val < m_min) {
                val = m_min;
            }
            if (m_reverse) {
                int offset = val - m_min;
                val = m_max - offset;
            }
        }
    } else {
        // LED
        float f = val;
        float bf = brightness;
        float maxB = bf * 40.95f;
        if (is16Bit) {
            f = maxB * pow(f / 65535.0f, gamma);
        } else {
            f = maxB * pow(f / 255.0f, gamma);
        }
        val = std::round(f);
    }
    m_nextValue = val;
    return val;
}

/*
 *
 */
int PCA9685Output::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "PCA9685Output::Init(JSON)\n");
    origConfig = config;

    Json::Value portConfigs;

    uint32_t clock = 25000000;
    bool externalClock = false;

    if (config.isMember("deviceID")) {
        // config from "Other" tab
        if (config["deviceID"].isString()) {
            m_deviceID = std::atoi(config["deviceID"].asString().c_str());
        } else {
            m_deviceID = config["deviceID"].asInt();
        }
        m_i2cDevice = config["device"].asString();
        if (m_i2cDevice == "") {
            m_i2cDevice = "i2c-1";
        }
        m_frequency = config["frequency"].asInt();
        portConfigs = config["ports"];
    } else {
        std::string subType = config["subType"].asString();
        Json::Value capeConfig;
        if (!CapeUtils::INSTANCE.getPWMConfig(subType, capeConfig)) {
            LogErr(VB_CHANNELOUT, "Could not get cape config for %s\n", subType.c_str());
            return 0;
        }
        m_i2cDevice = capeConfig["bus"].asString();
        m_deviceID = capeConfig["address"].asInt();
        if (capeConfig.isMember("clock")) {
            clock = capeConfig["clock"].asInt();
            externalClock = true;
        }
        portConfigs = config["outputs"];
        m_frequency = decodeFrequency(config["frequency"].asString());
    }

    i2c = new I2CUtils(m_i2cDevice.c_str(), m_deviceID);
    if (!i2c->isOk()) {
        LogErr(VB_CHANNELOUT, "Error opening I2C device for PCA9685 output\n");
        return 0;
    }

    float fs = clock;
    float ofs = fs;
    fs /= 4096.f;
    fs /= m_frequency;

    int fsi = (int)(fs + 0.5f);
    if (fsi < 3) {
        fsi = 3;
    }

    m_actualFrequency = ofs / (fsi * 4096.0f);

    // printf("%0.3f   fsi:  %d     of: %0.2f\n", fs, fsi, m_actualFrequency);
    LogDebug(VB_CHANNELOUT, "PCA9685 using actual frequency of: %0.2f\n", m_actualFrequency);
    restoreOriginalConfig();

    // Initialize
    int oldmode = i2c->readByteData(0x00);
    int m0 = (oldmode & 0x7f) | 0x10;
    // set sleep bit
    i2c->writeByteData(0x00, m0);

    i2c->writeByteData(0x01, 0x04); // ON STOP
    // i2c->writeByteData(0x01, 0x0C); // ON ACK

    // turn on the external clock
    if (externalClock) {
        oldmode |= 0x40;
        m0 |= 0x40;
        i2c->writeByteData(0x00, m0);
    }

    // set the pre-scaler
    i2c->writeByteData(0xFE, fsi);

    // auto increment bit
    oldmode |= 0x20;
    i2c->writeByteData(0x00, oldmode);

    // clear sleep bit if set to restart oscillator
    oldmode &= 0x6F;
    i2c->writeByteData(0x00, oldmode);

    // sleep for at least 500uS for the oscillator to stabilize
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    return ChannelOutput::Init(config);
}

void PCA9685Output::restoreOriginalConfig() {
    Json::Value portConfigs = origConfig.isMember("deviceID") ? origConfig["ports"] : origConfig["outputs"];
    loadPortConfig(portConfigs);
}
void PCA9685Output::loadPortConfig(const Json::Value& portConfigs) {
    uint32_t sc = m_startChannel;
    bool asUsec = true;
    if (origConfig.isMember("deviceID")) {
        if (origConfig.isMember("asUsec")) {
            asUsec = origConfig["asUsec"].asInt();
        }
    }
    numPorts = portConfigs.size();

    for (int x = 0; x < numPorts; x++) {
        m_ports[x].description = portConfigs[x]["description"].asString();
        m_ports[x].m_min = portConfigs[x]["min"].asInt();
        m_ports[x].m_max = portConfigs[x]["max"].asInt();
        if (portConfigs[x].isMember("center")) {
            m_ports[x].m_center = portConfigs[x]["center"].asInt();
        } else {
            m_ports[x].m_center = (m_ports[x].m_min + m_ports[x].m_max) / 2;
        }
        if (portConfigs[x].isMember("type")) {
            m_ports[x].type = decodePWMType(portConfigs[x]["type"].asString());
        } else {
            m_ports[x].type = PCA9685Output::PWMType::SERVO;
        }

        if (asUsec) {
            float z = m_ports[x].m_min;
            z *= m_actualFrequency;
            z *= 4096;
            z /= 1000000;
            m_ports[x].m_min = std::round(z);

            z = m_ports[x].m_max;
            z *= m_actualFrequency;
            z *= 4096;
            z /= 1000000;
            m_ports[x].m_max = std::round(z);

            z = m_ports[x].m_center;
            z *= m_actualFrequency;
            z *= 4096;
            z /= 1000000;
            m_ports[x].m_center = std::round(z);
            LogDebug(VB_CHANNELOUT, "PCA9685 pulse ranges for output %d:   %d  %d  %d     Steps: %d\n", x, m_ports[x].m_min, m_ports[x].m_center, m_ports[x].m_max, m_ports[x].m_max - m_ports[x].m_min);
            // printf("PCA9685 pulse ranges for output %d:   %d  %d  %d     Steps: %d\n", x, m_ports[x].m_min, m_ports[x].m_center, m_ports[x].m_max, m_ports[x].m_max - m_ports[x].m_min);
        }

        if (m_ports[x].type == PCA9685Output::PWMType::SERVO) {
            if (portConfigs[x].isMember("zeroBehavior")) {
                // co-other config
                m_ports[x].m_zeroBehavior = decodeZTE(portConfigs[x]["zeroBehavior"].asInt());
                int dt = portConfigs[x]["dataType"].asInt();
                switch (dt) {
                case 0:
                    m_ports[x].m_dataType = PCA9685Output::DataType::SCALED;
                    m_ports[x].is16Bit = false;
                    m_ports[x].m_reverse = false;
                    break;
                case 1:
                    m_ports[x].m_dataType = PCA9685Output::DataType::SCALED;
                    m_ports[x].is16Bit = false;
                    m_ports[x].m_reverse = true;
                    break;
                case 2:
                    m_ports[x].m_dataType = PCA9685Output::DataType::SCALED;
                    m_ports[x].is16Bit = true;
                    m_ports[x].m_reverse = false;
                    break;
                case 3:
                    m_ports[x].m_dataType = PCA9685Output::DataType::SCALED;
                    m_ports[x].is16Bit = true;
                    m_ports[x].m_reverse = true;
                    break;
                case 4:
                    m_ports[x].m_dataType = PCA9685Output::DataType::ABSOLUTE;
                    m_ports[x].is16Bit = false;
                    m_ports[x].m_reverse = false;
                    break;
                case 5:
                    m_ports[x].m_dataType = PCA9685Output::DataType::ABSOLUTE;
                    m_ports[x].is16Bit = true;
                    m_ports[x].m_reverse = false;
                    break;
                }
                if (m_ports[x].m_zeroBehavior == PCA9685Output::ZeroType::NORMAL) {
                    if (m_ports[x].m_dataType == PCA9685Output::DataType::ABSOLUTE) {
                        m_ports[x].m_zeroBehavior = PCA9685Output::ZeroType::OFF;
                    } else {
                        m_ports[x].m_zeroBehavior = PCA9685Output::ZeroType::MIN;
                    }
                }
            } else {
                // co-pwm config
                m_ports[x].m_zeroBehavior = decodeZTE(portConfigs[x]["zero"].asString());
                m_ports[x].m_dataType = decodeDataType(portConfigs[x]["dataType"].asString());
                m_ports[x].m_reverse = portConfigs[x]["reverse"].asInt();
                m_ports[x].is16Bit = portConfigs[x]["is16bit"].asInt();
            }
        } else {
            m_ports[x].gamma = portConfigs[x]["gamma"].asFloat();
            m_ports[x].brightness = portConfigs[x]["brightness"].asInt();
            m_ports[x].is16Bit = portConfigs[x]["is16bit"].asInt();
            if (m_ports[x].brightness > 100 || m_ports[x].brightness < 0) {
                m_ports[x].brightness = 100;
            }
            if (m_ports[x].gamma < 0.01) {
                m_ports[x].gamma = 0.01;
            }
            if (m_ports[x].gamma > 50.0f) {
                m_ports[x].gamma = 50.0f;
            }
        }
        if (portConfigs[x].isMember("startChannel")) {
            m_ports[x].startChannel = portConfigs[x]["startChannel"].asInt();
        } else {
            m_ports[x].startChannel = sc;
            sc += m_ports[x].is16Bit ? 2 : 1;
        }
    }
}

/*
 *
 */
int PCA9685Output::Close(void) {
    LogDebug(VB_CHANNELOUT, "PCA9685Output::Close()\n");

    return ChannelOutput::Close();
}

void PCA9685Output::StartingOutput() {
    uint8_t b = i2c->readByteData(0x00);
    i2c->writeByteData(0x00, b & 0x6F);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
void PCA9685Output::StoppingOutput() {
    uint8_t allOff[4] = { 0x00, 0x00, 0x00, 0x10 };
    i2c->writeRawI2CBlockData(0xFA, allOff, 4);
    std::this_thread::sleep_for(std::chrono::milliseconds(3));
    uint8_t b = i2c->readByteData(0x00);
    i2c->writeByteData(0x00, b | 0x10);
}

void PCA9685Output::PrepData(unsigned char* channelData) {
    for (int x = 0; x < numPorts; x++) {
        m_ports[x].readValue(channelData, m_actualFrequency, x);
    }
}

/*
 *
 */
int PCA9685Output::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "PCA9685Output::SendData(%p)\n", channelData);
    lastFrameTime = GetTimeMicros();

    for (int x = 0; x < numPorts; x++) {
        data[x * 2] = 0;
        data[x * 2 + 1] = m_ports[x].m_nextValue;
        m_ports[x].m_lastValue = m_ports[x].m_nextValue;
    }
    i2c->writeRawI2CBlockData(0x06, (uint8_t*)&(data[0]), numPorts * 4);
    return m_channelCount;
}

void PCA9685Output::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    for (int x = 0; x < numPorts; x++) {
        addRange(m_ports[x].startChannel, m_ports[x].startChannel + (m_ports[x].is16Bit ? 1 : 0));
    }
}
static inline void writeChannelValue(unsigned char* channelData, uint32_t sc, bool is16bit, uint32_t value) {
    if (is16bit) {
        channelData[sc] = (value >> 8) & 0xFF;
        channelData[sc + 1] = value & 0xFF;
    } else {
        channelData[sc] = value;
    }
}

void PCA9685Output::OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) {
    if (!origConfig.isMember("deviceID")) {
        if (testType != lastTestType) {
            restoreOriginalConfig();
        }
        lastTestType = testType;
        if (testType == 1) {
            loadPortConfig(config["outputs"]);
            int testingPort = config.isMember("manipulation") ? config["manipulation"]["port"].asInt() : -1;
            uint32_t tvalue = config["manipulation"]["value"].asInt();
            bool isMin = config["manipulation"]["isMin"].asBool();
            for (int x = 0; x < numPorts; x++) {
                if (m_ports[x].type == PCA9685Output::PWMType::SERVO) {
                    uint32_t value = m_ports[x].is16Bit ? 32768 : 128;
                    if (x == testingPort) {
                        if (m_ports[x].m_reverse) {
                            isMin = !isMin;
                        }
                        value = isMin ? 1 : (m_ports[x].is16Bit ? 65535 : 255);
                    } else if (m_ports[x].m_zeroBehavior == PCA9685Output::ZeroType::MIN) {
                        value = 0;
                    } else if (m_ports[x].m_zeroBehavior == PCA9685Output::ZeroType::MAX) {
                        value = m_ports[x].is16Bit ? 65535 : 255;
                    }
                    m_ports[x].m_dataType = PCA9685Output::DataType::SCALED;
                    writeChannelValue(channelData, m_ports[x].startChannel, m_ports[x].is16Bit, value);
                }
            }
        }
    }
}
bool PCA9685Output::SupportsTesting() const {
    return !origConfig.isMember("deviceID");
}

/*
 *
 */
void PCA9685Output::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "PCA9685Outputs::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    deviceID: %X\n", m_deviceID);
    LogDebug(VB_CHANNELOUT, "    Frequency: %d\n", m_frequency);
    for (int x = 0; x < numPorts; x++) {
        LogDebug(VB_CHANNELOUT, "    Port %d:    %s\n", x, m_ports[x].description.c_str());
        LogDebug(VB_CHANNELOUT, "                Min:    %d\n", m_ports[x].m_min);
        LogDebug(VB_CHANNELOUT, "                Center: %d\n", m_ports[x].m_center);
        LogDebug(VB_CHANNELOUT, "                Max:    %d\n", m_ports[x].m_max);
        LogDebug(VB_CHANNELOUT, "                DataType: %d\n", m_ports[x].m_dataType);
        LogDebug(VB_CHANNELOUT, "                ZeroType: %d\n", m_ports[x].m_zeroBehavior);
    }

    ChannelOutput::DumpConfig();
}
