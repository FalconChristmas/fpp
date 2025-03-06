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

#include "Plugin.h"
#include "USBRelay.h"

class USBRelayPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    USBRelayPlugin() :
        FPPPlugins::Plugin("USBRelay") {}
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

USBRelayOutput::USBRelayOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount), SerialChannelOutput(), m_subType(RELAY_DVC_UNKNOWN), m_relayCount(0) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::USBRelayOutput(%u, %u)\n",
             startChannel, channelCount);
}

USBRelayOutput::~USBRelayOutput() {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::~USBRelayOutput()\n");
}

void USBRelayOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_relayCount - 1);
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

    m_relayCount = config["channelCount"].asInt();

    if (m_subType == RELAY_DVC_UNKNOWN) {
        LogErr(VB_CHANNELOUT, "Invalid Config, missing device or invalid type\n");
        return 0;
    }

    if (!setupSerialPort(config, 9600, "8N1")) {
        return 0;
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
        int res = read(m_fd, &c_reply, 1);
        if (res == 0) {
            LogWarn(VB_CHANNELOUT, "Did not receive a response byte from ICstation relay\n");
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

int USBRelayOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "USBRelayOutput::RawSendData(%p)\n", channelData);

    if (m_subType == RELAY_DVC_CH340) {
        // CH340-specific logic: Send a 4-byte command for each relay to control its state
        unsigned char cmd[4] = { 0xA0, 0, 0, 0 }; // Initialize the command array: byte 0 is the start flag (0xA0)
        for (int i = 0; i < m_relayCount; i++) {  // Loop over each relay channel (1 to m_relayCount)
            cmd[1] = i + 1;                       // Byte 1: Relay number (1-based: 1 to 8 for an 8-channel relay)
            cmd[2] = channelData[i] ? 1 : 0;      // Byte 2: Relay state (1 for ON, 0 for OFF), based on channelData[i]
            cmd[3] = cmd[0] + cmd[1] + cmd[2];    // Byte 3: Checksum, sum of bytes 0, 1, and 2
            write(m_fd, cmd, 4);                  // Write the 4-byte command to the serial device (m_fd) to control the relay
        }
    } else {
        // Non-CH340 (Bit or ICStation) logic: Send data as a bitstream, 8 bits per byte
        char out = 0x00;
        int shiftBits = 0;

        for (int i = 0; i < m_relayCount; i++) {
            if ((i > 0) && ((i % 8) == 0)) {
                write(m_fd, &out, 1);
                out = 0x00;
                shiftBits = 0;
            }

            out |= (channelData[i] ? 1 : 0) << shiftBits;
            shiftBits++;
        }

        if (shiftBits)
            write(m_fd, &out, 1);
    }

    return m_relayCount;
}

void USBRelayOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "USBRelayOutput::DumpConfig()\n");
    dumpSerialConfig();
    ChannelOutput::DumpConfig();
}