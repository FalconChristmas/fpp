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

#include "USBRelay.h"
#include "serialutil.h"

#include "Plugin.h"
class USBRelayPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    USBRelayPlugin() :
        FPPPlugins::Plugin("USBRelay") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new USBRelayOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new USBRelayPlugin();
}
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
USBRelayOutput::USBRelayOutput(unsigned int startChannel,
                               unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    m_deviceName(""),
    m_fd(-1),
    m_subType(RELAY_DVC_UNKNOWN),
    m_relayCount(0) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::USBRelayOutput(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
USBRelayOutput::~USBRelayOutput() {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::~USBRelayOutput()\n");
}

void USBRelayOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_relayCount - 1);
}

/*
 *
 */
int USBRelayOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::Init(JSON)\n");

    std::string subType = config["subType"].asString();

    if (subType == "Bit")
        m_subType = RELAY_DVC_BIT;
    else if (subType == "ICStation")
        m_subType = RELAY_DVC_ICSTATION;

    m_deviceName = config["device"].asString();
    m_relayCount = config["channelCount"].asInt();

    if ((m_deviceName == "") ||
        (m_subType == RELAY_DVC_UNKNOWN)) {
        LogErr(VB_CHANNELOUT, "Invalid Config, missing device or invalid type\n");
        return 0;
    }

    m_deviceName.insert(0, "/dev/");

    LogInfo(VB_CHANNELOUT, "Opening %s for USB Relay output\n",
            m_deviceName.c_str());

    m_fd = SerialOpen(m_deviceName.c_str(), 9600, "8N1");

    if (m_fd < 0) {
        LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
               errno, m_deviceName.c_str(), strerror(errno));
    }

    if (m_subType == RELAY_DVC_ICSTATION) {
        // Send Initialization Sequence
        unsigned char c_init = 0x50;
        unsigned char c_reply = 0x00;
        unsigned char c_open = 0x51;

        sleep(1);
        write(m_fd, &c_init, 1);
        usleep(500000);

        bool foundICS = false;
        int res = 0;
        res = read(m_fd, &c_reply, 1);
        if (res == 0) {
            LogWarn(VB_CHANNELOUT, "Did not receive a response byte from ICstation relay, unable to confirm number of relays.  Using configuration value from UI\n");
        } else if (c_reply == 0xAB) {
            LogInfo(VB_CHANNELOUT, "Found a 4-channel ICStation relay module\n");
            m_relayCount = 4;
            foundICS = true;
        } else if (c_reply == 0xAC) {
            LogInfo(VB_CHANNELOUT, "Found a 8-channel ICStation relay module\n");
            m_relayCount = 8;
            foundICS = true;
        } else if (c_reply == 0xAD) {
            LogInfo(VB_CHANNELOUT, "Found a 2-channel ICStation relay module\n");
            m_relayCount = 2;
            foundICS = true;
        } else {
            LogWarn(VB_CHANNELOUT, "Warning: ICStation USB Relay response of 0x%02x doesn't match "
                                   "known values.  Unable to detect number of relays.\n",
                    c_reply);
        }

        if (foundICS)
            write(m_fd, &c_open, 1);
    }

    return ChannelOutput::Init(config);
}

/*
 *
 */
int USBRelayOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::Close()\n");

    SerialClose(m_fd);
    m_fd = -1;

    return ChannelOutput::Close();
}

/*
 *
 */
int USBRelayOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "USBRelayOutput::RawSendData(%p)\n", channelData);

    char out = 0x00;
    int shiftBits = 0;

    for (int i = 0; i < m_relayCount; i++) {
        if ((i > 0) && ((i % 8) == 0)) {
            // Write out previous byte
            write(m_fd, &out, 1);
            out = 0x00;
            shiftBits = 0;
        }

        out |= (channelData[i] ? 1 : 0) << shiftBits;
        shiftBits++;
    }

    // Write out any unwritten bits
    if (shiftBits)
        write(m_fd, &out, 1);

    return m_relayCount;
}

/*
 *
 */
void USBRelayOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    Device Filename   : %s\n", m_deviceName.c_str());
    LogDebug(VB_CHANNELOUT, "    fd                : %d\n", m_fd);

    ChannelOutput::DumpConfig();
}
