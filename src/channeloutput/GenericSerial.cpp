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

#include "../common.h"
#include "../log.h"

#include "GenericSerial.h"
#include "serialutil.h"

/////////////////////////////////////////////////////////////////////////////
#include "Plugin.h"
class GenericSerialPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    GenericSerialPlugin() :
        FPPPlugins::Plugin("GenericSerial") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new GenericSerialOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new GenericSerialPlugin();
}
}
/*
 *
 */
GenericSerialOutput::GenericSerialOutput(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount),
    m_deviceName("UNKNOWN"),
    m_fd(-1),
    m_speed(9600),
    m_headerSize(0),
    m_footerSize(0),
    m_packetSize(0),
    m_data(NULL) {
    LogDebug(VB_CHANNELOUT, "GenericSerialOutput::GenericSerialOutput(%u, %u)\n",
             startChannel, channelCount);

    m_useDoubleBuffer = 1;
}

/*
 *
 */
GenericSerialOutput::~GenericSerialOutput() {
    LogDebug(VB_CHANNELOUT, "GenericSerialOutput::~GenericSerialOutput()\n");
}

int GenericSerialOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "GenericSerialOutput::Init()\n");

    if (config.isMember("device")) {
        m_deviceName = config["device"].asString();
        LogDebug(VB_CHANNELOUT, "Using %s for Generic Serial output\n",
                 m_deviceName.c_str());
    }
    if (config.isMember("speed")) {
        m_speed = config["speed"].asInt();
    }
    if (config.isMember("footer")) {
        m_footer = config["footer"].asString();
        m_footerSize = m_header.length();
    }
    if (config.isMember("header")) {
        m_header = config["header"].asString();
        m_headerSize = m_header.length();
    }
    m_packetSize = m_headerSize + m_channelCount + m_footerSize;

    m_data = new char[m_packetSize + 1];
    if (!m_data) {
        LogErr(VB_CHANNELOUT, "Could not allocate channel data buffer\n");
        return 0;
    }

    if (m_headerSize)
        memcpy(m_data, m_header.c_str(), m_headerSize);

    if (m_footerSize)
        memcpy(m_data + m_headerSize + m_channelCount,
               m_footer.c_str(), m_footerSize);

    m_deviceName.insert(0, "/dev/");

    m_fd = SerialOpen(m_deviceName.c_str(), m_speed, "8N1");

    if (m_fd < 0) {
        LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
               errno, m_deviceName.c_str(), strerror(errno));
        return 0;
    }

    return ThreadedChannelOutput::Init(config);
}

void GenericSerialOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
int GenericSerialOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "GenericSerialOutput::Close()\n");

    SerialClose(m_fd);

    delete[] m_data;

    return ThreadedChannelOutput::Close();
}

/*
 *
 */
int GenericSerialOutput::RawSendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "GenericSerialOutput::RawSendData(%p)\n", channelData);

    memcpy(m_data + m_headerSize, channelData, m_channelCount);

    if (WillLog(LOG_EXCESSIVE, VB_CHANNELDATA))
        HexDump("Generic Serial", m_data, m_headerSize + 16, VB_CHANNELDATA);

    write(m_fd, m_data, m_packetSize);

    return m_channelCount;
}

/*
 *
 */
void GenericSerialOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "GenericSerialOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    Device Name: %s\n", m_deviceName.c_str());
    LogDebug(VB_CHANNELOUT, "    fd         : %d\n", m_fd);
    LogDebug(VB_CHANNELOUT, "    Port Speed : %d\n", m_speed);
    LogDebug(VB_CHANNELOUT, "    Header Size: %d\n", m_headerSize);
    LogDebug(VB_CHANNELOUT, "    Header     : '%s'\n", m_header.c_str());
    LogDebug(VB_CHANNELOUT, "    Footer Size: %d\n", m_footerSize);
    LogDebug(VB_CHANNELOUT, "    Footer     : '%s'\n", m_footer.c_str());
    LogDebug(VB_CHANNELOUT, "    Packet Size: %d\n", m_packetSize);

    ThreadedChannelOutput::DumpConfig();
}
