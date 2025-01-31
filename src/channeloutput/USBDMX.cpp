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

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "../Warnings.h"
#include "../log.h"

#include "USBDMX.h"
#include "serialutil.h"

#define DMX_MAX_CHANNELS 512

#include "Plugin.h"
class USBDMXPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    USBDMXPlugin() :
        FPPPlugins::Plugin("USBDMX") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new USBDMXOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new USBDMXPlugin();
}
}

/////////////////////////////////////////////////////////////////////////////

USBDMXOutput::USBDMXOutput(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount), SerialChannelOutput(), m_dongleType(DMX_DVC_UNKNOWN) {
    LogDebug(VB_CHANNELOUT, "USBDMXOutput::USBDMXOutput(%u, %u)\n",
             startChannel, channelCount);

    m_useDoubleBuffer = 1;
    // DMX protocol requires data to be sent at least every 250ms
    m_maxWait = 250;
    m_dataOffset = 1;
    memset(m_outputData, 0, sizeof(m_outputData));
    if (m_channelCount > DMX_MAX_CHANNELS) {
        WarningHolder::AddWarning("USBDMX: Invalid Config.  Channel count of " + std::to_string(m_channelCount) + " exceeds the DMX universe size of 512");
        m_channelCount = DMX_MAX_CHANNELS;
    }
}

USBDMXOutput::~USBDMXOutput() {
    LogDebug(VB_CHANNELOUT, "USBDMXOutput::~USBDMXOutput()\n");
}
void USBDMXOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

int USBDMXOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "USBDMXOutput::Init()\n");
    if (config.isMember("type")) {
        std::string type = config["type"].asString();
        if (type == "DMX-Open") {
            LogDebug(VB_CHANNELOUT, "Treating device as DMX-Open compatible\n");
            m_dongleType = DMX_DVC_OPEN;
        } else if (type == "DMX-Pro") {
            LogDebug(VB_CHANNELOUT, "Treating device as DMX-Pro compatible\n");
            m_dongleType = DMX_DVC_PRO;
        }
    }
    if (m_dongleType == DMX_DVC_OPEN) {
        if (!setupSerialPort(config, 250000, "8N2")) {
            return 0;
        }
    } else if (m_dongleType == DMX_DVC_PRO) {
        if (!setupSerialPort(config, 115200, "8N1")) {
            return 0;
        }
    } else {
        LogErr(VB_CHANNELOUT, "Invalid Config.  Unknown dongle type.\n");
        WarningHolder::AddWarning("USBDMX: Invalid Config.  Unknown dongle type.");
        return 0;
    }

    if (m_dongleType == DMX_DVC_OPEN) {
        if (SerialResetRTS(m_fd) != 0) {
            LogErr(VB_CHANNELOUT, "Error %d resetting RTS on %s: %s\n",
                   errno, m_deviceName.c_str(), strerror(errno));
            return 0;
        }
        m_dataLen = m_channelCount + 1;
    } else if (m_dongleType == DMX_DVC_PRO) {
        m_outputData[0] = 0x7E;
        m_outputData[1] = 0x06;
        m_outputData[2] = (m_channelCount + 1) & 0xFF;
        m_outputData[3] = ((m_channelCount + 1) >> 8) & 0xFF;
        m_outputData[4] = 0x00;
        m_dataOffset = 5;

        m_outputData[m_channelCount + 5] = 0xE7;
        m_dataLen = m_channelCount + 6;
    }

    return ThreadedChannelOutput::Init(config);
}

int USBDMXOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "USBDMXOutput::Close()\n");
    closeSerialPort();
    return ThreadedChannelOutput::Close();
}

int USBDMXOutput::RawSendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "USBDMXOutput::RawSendData(%p)\n", channelData);
    memcpy(m_outputData + m_dataOffset, channelData, m_channelCount);
    WaitTimedOut();
    return m_channelCount;
}
void USBDMXOutput::WaitTimedOut() {
    if (m_dongleType == DMX_DVC_OPEN) {
        SerialSendBreak(m_fd, 200);
        usleep(20);
    }
    write(m_fd, m_outputData, m_dataLen);
}

void USBDMXOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "USBDMXOutput::DumpConfig()\n");
    LogDebug(VB_CHANNELOUT, "    Dongle Type: %s\n",
             m_dongleType == DMX_DVC_PRO ? "Pro" : m_dongleType == DMX_DVC_OPEN ? "Open"
                                                                                : "UNKNOWN");
    dumpSerialConfig();
    ThreadedChannelOutput::DumpConfig();
}
