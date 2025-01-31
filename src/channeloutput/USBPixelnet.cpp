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

#include <unistd.h>

#include "../Warnings.h"
#include "../log.h"

#include "USBPixelnet.h"
#include "serialutil.h"

#include "Plugin.h"
class USBPixelnetPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    USBPixelnetPlugin() :
        FPPPlugins::Plugin("USBPixelnet") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new USBPixelnetOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new USBPixelnetPlugin();
}
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
USBPixelnetOutput::USBPixelnetOutput(unsigned int startChannel,
                                     unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount),
    m_outputData(NULL),
    m_pixelnetData(NULL),
    m_dongleType(PIXELNET_DVC_UNKNOWN) {
    LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::USBPixelnetOutput(%u, %u)\n",
             startChannel, channelCount);

    m_useDoubleBuffer = 1;
}

/*
 *
 */
USBPixelnetOutput::~USBPixelnetOutput() {
    LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::~USBPixelnetOutput()\n");
}
int USBPixelnetOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::Init()\n");
    if (config.isMember("type")) {
        std::string type = config["type"].asString();
        if (type == "Pixelnet-Lynx") {
            LogDebug(VB_CHANNELOUT, "Treating device as Pixelnet-Lynx compatible\n");
            m_dongleType = PIXELNET_DVC_LYNX;
        } else if (type == "Pixelnet-Open") {
            LogDebug(VB_CHANNELOUT, "Treating device as Pixelnet-Open compatible\n");
            m_dongleType = PIXELNET_DVC_OPEN;
        }
    }

    if (m_dongleType == PIXELNET_DVC_LYNX) {
        if (!setupSerialPort(config, 115200, "8N1")) {
            return 0;
        }
    } else if (m_dongleType == PIXELNET_DVC_OPEN) {
        if (!setupSerialPort(config, 1000000, "8N2")) {
            return 0;
        }
    }

    if (m_dongleType == PIXELNET_DVC_LYNX) {
        m_outputData = m_rawData + 7;
        m_pixelnetData = m_outputData + 1;

        m_outputData[0] = '\xAA';

        m_outputPacketSize = 4097;
    } else if (m_dongleType == PIXELNET_DVC_OPEN) {
        m_outputData = m_rawData + 2;
        m_pixelnetData = m_outputData + 6;

        m_outputData[0] = '\xAA';
        m_outputData[1] = '\x55';
        m_outputData[2] = '\x55';
        m_outputData[3] = '\xAA';
        m_outputData[4] = '\x15';
        m_outputData[5] = '\x5D';

        m_outputPacketSize = 4102;
    }

    return ThreadedChannelOutput::Init(config);
}

void USBPixelnetOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
int USBPixelnetOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::Close()\n");
    closeSerialPort();
    return ThreadedChannelOutput::Close();
}

/*
 *
 */
int USBPixelnetOutput::RawSendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "USBPixelnetOutput::RawSendData(%p)\n", channelData);

    memcpy(m_pixelnetData, channelData, m_channelCount);

    // 0xAA is start of Pixelnet packet, so convert 0xAA (170) to 0xAB (171)
    unsigned char* dptr = m_pixelnetData;
    unsigned char* eptr = dptr + m_channelCount;
    for (; dptr < eptr; dptr++) {
        if (*dptr == 0xAA)
            *dptr = 0xAB;
    }

    // Send Header and Pixelnet Data
    write(m_fd, m_outputData, m_outputPacketSize);

    return m_channelCount;
}

/*
 *
 */
void USBPixelnetOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::DumpConfig()\n");
    dumpSerialConfig();
    LogDebug(VB_CHANNELOUT, "    Output Packet Size: %d\n", m_outputPacketSize);
    ThreadedChannelOutput::DumpConfig();
}
