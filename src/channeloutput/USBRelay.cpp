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

#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "USBRelay.h"
#include "log.h"
#include "serialutil.h"

USBRelayOutput::USBRelayOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    SerialChannelOutput(),
    m_deviceName(""),
    m_fd(-1),
    m_subType(RELAY_DVC_UNKNOWN),
    m_relayCount(0) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::USBRelayOutput(%u, %u)\n",
             startChannel, channelCount);
}

USBRelayOutput::~USBRelayOutput() {
    // Empty destructor
}

int USBRelayOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::Init(JSON)\n");

    std::string subType = config["subType"].asString();

    if (subType == "Bit")
        m_subType = RELAY_DVC_BIT;
    else if (subType == "ICStation")
        m_subType = RELAY_DVC_ICSTATION;
    else if (subType == "CH340")
        m_subType = RELAY_DVC_CH340;

    m_deviceName = config["device"].asString();
    m_relayCount = config["channelCount"].asInt();

    if ((m_deviceName == "") || (m_subType == RELAY_DVC_UNKNOWN)) {
        LogErr(VB_CHANNELOUT, "Invalid Config, missing device or invalid type\n");
        return 0;
    }

    if (!setupSerialPort(config, 9600, "8N1")) {
        LogErr(VB_CHANNELOUT, "Failed to setup serial port for %s\n", m_deviceName.c_str());
        return 0;
    }

    if (m_subType == RELAY_DVC_BIT) {
        LogInfo(VB_CHANNELOUT, "Initializing Bit USB Relay with %d channels\n", m_relayCount);
    } else if (m_subType == RELAY_DVC_ICSTATION) {
        unsigned char c_init = 0x50;
        unsigned char c_reply = 0x00;
        unsigned char c_open = 0x51;

        sleep(2); // Increased initial sleep for reliability
        write(m_fd, &c_init, 1);

        int delay = 1000000; // Start with 1 second
        int max_attempts = 3;
        bool foundICS = false;
        for (int attempt = 0; attempt < max_attempts && !foundICS; attempt++) {
            usleep(delay);
            int res = read(m_fd, &c_reply, 1);
            if (res == 0) {
                LogDebug(VB_CHANNELOUT, "Did not receive a response byte from ICstation relay after %dms\n", delay / 1000);
                delay += 500000; // Increase by 500ms each attempt
                continue;
            }
            if (c_reply == 0xAB) {
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
                LogWarn(VB_CHANNELOUT, "Warning: ICStation USB Relay response of 0x%02x doesn't match known values.  Unable to detect number of relays.\n", c_reply);
            }
        }
        if (!foundICS) {
            LogWarn(VB_CHANNELOUT, "ICStation relay did not respond after %d attempts, using UI-configured channel count %d. Check hardware, USB connection, or increase timing delay.\n", max_attempts, m_relayCount);
        }

        write(m_fd, &c_open, 1);
    } else if (m_subType == RELAY_DVC_CH340) {
        LogInfo(VB_CHANNELOUT, "Initializing CH340 USB Relay with %d channels\n", m_relayCount);
    }

    return ChannelOutput::Init(config);
}

int USBRelayOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::Close()\n");

    closeSerialPort();

    return ChannelOutput::Close();
}

void USBRelayOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_relayCount - 1);
}

void USBRelayOutput::PrepData(unsigned char* channelData) {
    // No preparation needed for USB Relay
}

int USBRelayOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "USBRelayOutput::SendData(%p)\n", channelData);

    if (m_subType == RELAY_DVC_BIT) {
        unsigned char c = 0;

        for (int i = 0; i < m_relayCount; i++) {
            if (channelData[m_startChannel + i])
                c |= (1 << i);
        }

        write(m_fd, &c, 1);
    } else if (m_subType == RELAY_DVC_ICSTATION) {
        unsigned char c = 0;

        for (int i = 0; i < m_relayCount; i++) {
            if (channelData[m_startChannel + i])
                c |= (1 << i);
        }

        write(m_fd, &c, 1);
    } else if (m_subType == RELAY_DVC_CH340) {
        unsigned char cmd[4] = {0xA0, 0, 0, 0};
        for (int i = 0; i < m_relayCount; i++) {
            cmd[1] = i + 1;
            cmd[2] = channelData[m_startChannel + i] ? 1 : 0;
            cmd[3] = cmd[0] + cmd[1] + cmd[2];
            write(m_fd, cmd, 4);
        }
    }

    return m_relayCount;
}

void USBRelayOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::DumpConfig()\n");

    dumpSerialConfig();

    ChannelOutput::DumpConfig();
}