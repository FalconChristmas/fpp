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

#include "../log.h"

#include "GPIO.h"

#include "Plugin.h"
class GPIOPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    GPIOPlugin() :
        FPPPlugins::Plugin("GPIO") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new GPIOOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new GPIOPlugin();
}
}

/*
 *
 */
GPIOOutput::GPIOOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    m_GPIOPin(nullptr),
    m_invertOutput(0),
    m_pwm(0) {
    LogDebug(VB_CHANNELOUT, "GPIOOutput::GPIOOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
GPIOOutput::~GPIOOutput() {
    LogDebug(VB_CHANNELOUT, "GPIOOutput::~GPIOOutput()\n");
}
int GPIOOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "GPIOOutput::Init()\n");
    int gpio = config["gpio"].asInt();
    m_invertOutput = config["invert"].asInt();
    if (config.isMember("softPWM")) {
        m_pwm = config["softPWM"].asInt();
    }
    if (config.isMember("pwm")) {
        m_pwm = config["pwm"].asInt();
    }

    m_GPIOPin = PinCapabilities::getPinByGPIO(gpio).ptr();
    if (m_GPIOPin == nullptr) {
        LogErr(VB_CHANNELOUT, "GPIO Pin not configured\n");
        return 0;
    }

    if (m_pwm) {
        if (!m_GPIOPin->supportPWM()) {
            LogErr(VB_CHANNELOUT, "GPIO Pin does now support PWM\n");
            return 0;
        }
        m_GPIOPin->setupPWM(25500);

        if (m_invertOutput)
            m_GPIOPin->setPWMValue(25500);
        else
            m_GPIOPin->setPWMValue(0);
    } else {
        m_GPIOPin->configPin("gpio", true);

        if (m_invertOutput)
            m_GPIOPin->setValue(1);
        else
            m_GPIOPin->setValue(0);
    }

    return ChannelOutput::Init(config);
}

/*
 *
 */
int GPIOOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "GPIOOutput::Close()\n");

    return ChannelOutput::Close();
}

/*
 *
 */
int GPIOOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "GPIOOutput::SendData(%p)\n", channelData);

    std::string warning;
    try {
        if (m_pwm) {
            if (m_invertOutput)
                m_GPIOPin->setPWMValue(100 * (int)(255 - channelData[0]));
            else
                m_GPIOPin->setPWMValue(100 * (int)(channelData[0]));
        } else {
            if (m_invertOutput)
                m_GPIOPin->setValue(!channelData[0]);
            else
                m_GPIOPin->setValue(channelData[0]);
        }
    } catch (const std::exception& e) {
        warning = std::string("Exception in GPIO output: ") + std::string(e.what());
    } catch (...) {
        warning = std::string("Exception in GPIO output");
    }
    if (!warning.empty() && warning != lastWarning) {
        WarningHolder::AddWarning(warning);
    }

    return m_channelCount;
}

/*
 *
 */
void GPIOOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "GPIOOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    GPIO Pin       : %d\n", m_GPIOPin ? m_GPIOPin->kernelGpio : -1);
    LogDebug(VB_CHANNELOUT, "    Inverted Output: %s\n", m_invertOutput ? "Yes" : "No");
    LogDebug(VB_CHANNELOUT, "    PWM            : %s\n", m_pwm ? "Yes" : "No");

    ChannelOutput::DumpConfig();
}
