#pragma once
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

#include "ChannelOutput.h"
#include "util/I2CUtils.h"

class PCA9685Output : public ChannelOutput {
public:
    PCA9685Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~PCA9685Output();

    virtual std::string GetOutputType() const {
        return "PCA9685 PWM";
    }

    virtual void OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) override;
    virtual bool SupportsTesting() const override;

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int SendData(unsigned char* channelData) override;

    virtual void StartingOutput() override;
    virtual void StoppingOutput() override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

    enum class ZeroType {
        HOLD = 0,
        NORMAL = 1,
        CENTER = 2,
        OFF = 3,
        MIN = 4,
        MAX = 5
    };

    enum class DataType {
        SCALED,
        ABSOLUTE,
        ABSOLUTE_HALF,
        ABSOLUTE_DOUBLE
    };

    enum class PWMType {
        SERVO,
        LED
    };

    void restoreOriginalConfig();
    void loadPortConfig(const Json::Value& portConfig);

private:
    I2CUtils* i2c;
    int m_deviceID;
    std::string m_i2cDevice;
    int m_frequency;
    float m_actualFrequency;

    class PCA9685Port {
    public:
        std::string description;
        PWMType type = PWMType::SERVO;
        bool is16Bit = true;
        uint32_t startChannel = 0;

        // servo properties
        int m_min = 1000;
        int m_max = 2000;
        int m_center = 1500;
        DataType m_dataType = DataType::SCALED;
        ZeroType m_zeroBehavior = ZeroType::HOLD;
        bool m_reverse = false;

        // LED properties
        float gamma = 1.0f;
        uint32_t brightness = 100;

        unsigned short m_lastValue = 0;
        unsigned short m_nextValue = 0;

        unsigned short readValue(unsigned char* channelData, float frequency, int port);
    };

    PCA9685Port m_ports[16];
    uint32_t numPorts = 16;

    std::atomic<uint64_t> lastFrameTime;
    std::array<uint16_t, 32> data;
    Json::Value origConfig;
    int lastTestType = 0;
};
