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

/*
 * This driver requires a PCF8574 I2C chip with the following connections:
 *
 * MCP Pin      Connection
 * ----------   ------------------------
 *  16 (VCC)  - Pi Pin 4 (5V)
 *  8  (GND)  - Pi Pin 6 (Ground)
 *  14 (SCL)  - Pi Pin 5 (SCL)
 *  15 (SDA)  - Pi Pin 3 (SDA)
 *
 */

/*
 * Sample channeloutputs.json config
 *
 * {
 *       "channelOutputs": [
 *               {
 *                       "type": "PCF8574",
 *                       "enabled": 1,
 *                       "deviceID": 64,
 *                       "startChannel": 1,
 *                       "channelCount": 8,
 *                       "pinOrderingInvert": false,
 *                       "ports":[
 *                              {
 *                                  "invert": true,
 *                                  "pinMode": 0,
 *                                  "threshold": 128,
 *                                  "hysteresisUpper":192,
 *                                  "hysteresisLower": 64,
 *                                  "description": ""
 *                              },
 *                              {
 *                                  "invert": true,
 *                                   ...
 *                              }...
 *                       ]
 *                       }
 *               }
 *       ]
 * }
 *
 *
 *
 */
#include "fpp-pch.h"

#include "../log.h"

#include "PCF8574.h"

#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)     \
    (byte & 0x80 ? 1 : 0),     \
        (byte & 0x40 ? 1 : 0), \
        (byte & 0x20 ? 1 : 0), \
        (byte & 0x10 ? 1 : 0), \
        (byte & 0x08 ? 1 : 0), \
        (byte & 0x04 ? 1 : 0), \
        (byte & 0x02 ? 1 : 0), \
        (byte & 0x01 ? 1 : 0)

#include "Plugin.h"
class PCF8574Plugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    PCF8574Plugin() :
        FPPPlugins::Plugin("PCF8574") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new PCF8574Output(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new PCF8574Plugin();
}
}

/*
 *
 */
PCF8574Output::PCF8574Output(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    i2c(nullptr) {
    LogDebug(VB_CHANNELOUT, "PCF8574Output::PCF8574Output(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
PCF8574Output::~PCF8574Output() {
    LogDebug(VB_CHANNELOUT, "PCF8574Output::~PCF8574Output()\n");
    if (i2c) {
        delete i2c;
    }
}

/*
 *
 */
int PCF8574Output::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "PCF8574Output::Init(JSON)\n");

    if (config["deviceID"].isString()) {
        m_deviceID = std::atoi(config["deviceID"].asString().c_str());
    } else {
        m_deviceID = config["deviceID"].asInt();
    }

    if (m_deviceID < 0x20 || m_deviceID > 0x27) {
        LogErr(VB_CHANNELOUT, "Invalid PCF8574 Address: %X\n", m_deviceID);
        return 0;
    }

    i2c = new I2CUtils(1, m_deviceID);
    if (!i2c->isOk()) {
        LogErr(VB_CHANNELOUT, "Error opening I2C device for PCF8574 output\n");
        return 0;
    } else {
        LogDebug(VB_CHANNELOUT, "PCF8574Output opened I2C device ok\n");
    }

    for (int x = 0; x < 8; x++) {
        LogDebug(VB_CHANNELOUT, "PCF8574Output configure port %d\n", x);

        if (config["ports"][x].isMember("invert")) {
            m_ports[x].m_invert = config["ports"][x]["invert"].asInt();
        }
        if (config["ports"][x].isMember("pinMode")) {
            m_ports[x].m_pinmode = config["ports"][x]["pinMode"].asInt();
        }
        if (config["ports"][x].isMember("threshold")) {
            m_ports[x].m_threshold = config["ports"][x]["threshold"].asInt();
        }
        if (config["ports"][x].isMember("hysteresisUpper")) {
            m_ports[x].m_hysteresisUpper = config["ports"][x]["hysteresisUpper"].asInt();
        }
        if (config["ports"][x].isMember("hysteresisLower")) {
            m_ports[x].m_hysteresisLower = config["ports"][x]["hysteresisLower"].asInt();
        }
    }
    LogDebug(VB_CHANNELOUT, "PCF8574Output config ports ok\n");

    if (config.isMember("pinOrderingInvert")) {
        m_pinOrderingInvert = config["pinOrderingInvert"].asInt();
    }

    m_lastVal = 0xFF;

    return ChannelOutput::Init(config);
}

/*
 *
 */
int PCF8574Output::Close(void) {
    LogDebug(VB_CHANNELOUT, "PCF8574Output::Close()\n");

    return ChannelOutput::Close();
}

/*
 *
 */
int PCF8574Output::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "PCF8574Output::SendData(%p)\n", channelData);

    unsigned char* c = channelData;
    unsigned char byte1 = 0;

    for (int x = 0; x < m_channelCount; x++, c++) {
        unsigned char pinValue;

        switch (m_ports[x].m_pinmode) {
        case 0: // Simple trigger
            pinValue = ((*c) > 0);
            break;

        case 1: // Threshold trigger
            pinValue = ((*c) > m_ports[x].m_threshold);
            break;

        case 2: // Hysteresis trigger
            if (m_ports[x].m_lastState == 0) {
                pinValue = ((*c) > m_ports[x].m_hysteresisUpper);

            } else {
                pinValue = ((*c) > m_ports[x].m_hysteresisLower);
            }
            m_ports[x].m_lastState = pinValue;
            break;

        default:
            pinValue = ((*c) > 0);
            break;
        }

        pinValue ^= (m_ports[x].m_invert);

        if (m_pinOrderingInvert) {
            byte1 |= (pinValue << x);
        } else {
            byte1 |= (pinValue << (7 - x));
        }
    }

    LogExcess(VB_CHANNELOUT,
              "Byte: 0b" BYTETOBINARYPATTERN "\n",
              BYTETOBINARY(byte1));
    if (byte1 != m_lastVal) {
        m_lastVal = byte1;
        i2c->writeByte(byte1);
    }

    return m_channelCount;
}

/*
 *
 */
void PCF8574Output::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "PCF8574Output::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    deviceID: %X\n", m_deviceID);

    ChannelOutput::DumpConfig();
}
