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

#include <errno.h>
#include <fcntl.h>
#include <thread>

#include "../log.h"

#include "GPIO595.h"

#define GPIO595_MAX_CHANNELS 128

/////////////////////////////////////////////////////////////////////////////

#include "Plugin.h"
class GPIO595Plugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    GPIO595Plugin() :
        FPPPlugins::Plugin("GPIO595") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new GPIO595Output(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new GPIO595Plugin();
}
}

/*
 *
 */
GPIO595Output::GPIO595Output(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount),
    m_clockPin(nullptr),
    m_dataPin(nullptr),
    m_latchPin(nullptr) {
    LogDebug(VB_CHANNELOUT, "GPIO595Output::GPIO595Output(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
GPIO595Output::~GPIO595Output() {
    LogDebug(VB_CHANNELOUT, "GPIO595Output::~GPIO595Output()\n");
}
int GPIO595Output::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "GPIO595Output::Init()\n");

    if (config.isMember("clockPin")) {
        int p = config["clockPin"].asInt();
        m_clockPin = PinCapabilities::getPinByGPIO(p).ptr();
    }
    if (config.isMember("dataPin")) {
        int p = config["dataPin"].asInt();
        m_dataPin = PinCapabilities::getPinByGPIO(p).ptr();
    }
    if (config.isMember("latchPin")) {
        int p = config["latchPin"].asInt();
        m_latchPin = PinCapabilities::getPinByGPIO(p).ptr();
    }

    if ((m_clockPin == nullptr) ||
        (m_dataPin == nullptr) ||
        (m_latchPin == nullptr)) {
        LogErr(VB_CHANNELOUT, "GPIO595 Invalid Config\n");
        return 0;
    }

    m_clockPin->configPin("gpio", true);
    m_dataPin->configPin("gpio", true);
    m_latchPin->configPin("gpio", true);

    m_clockPin->setValue(0);
    m_dataPin->setValue(0);
    m_latchPin->setValue(1);

    return ThreadedChannelOutput::Init(config);
}

/*
 *
 */
int GPIO595Output::Close(void) {
    LogDebug(VB_CHANNELOUT, "GPIO595Output::Close()\n");

    return ThreadedChannelOutput::Close();
}

/*
 *
 */
int GPIO595Output::RawSendData(unsigned char* channelData) {
    LogDebug(VB_CHANNELOUT, "GPIO595Output::RawSendData(%p)\n", channelData);

    // Drop the latch low
    m_latchPin->setValue(0);
    std::this_thread::sleep_for(std::chrono::microseconds(1));

    // Output one bit per channel along with a clock tick
    int i = 0;
    for (i = m_channelCount - 1; i >= 0; i--) {
        // We only support basic On/Off.  non-zero channel value == On
        m_dataPin->setValue(channelData[i]);

        // Send a clock tick
        m_clockPin->setValue(1);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        m_clockPin->setValue(0);
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    // Bring the latch high to push the bits out of the chip
    m_latchPin->setValue(1);
    std::this_thread::sleep_for(std::chrono::microseconds(1));

    return m_channelCount;
}

/*
 *
 */
void GPIO595Output::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "GPIO595Output::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    Clock Pin: %d\n", m_clockPin ? m_clockPin->kernelGpio : -1);
    LogDebug(VB_CHANNELOUT, "    Data Pin : %d\n", m_dataPin ? m_dataPin->kernelGpio : -1);
    LogDebug(VB_CHANNELOUT, "    Latch Pin: %d\n", m_latchPin ? m_latchPin->kernelGpio : -1);

    ThreadedChannelOutput::DumpConfig();
}
