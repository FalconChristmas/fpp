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
 #include <fcntl.h>
 #include <termios.h>
 
 #include "../Warnings.h"
 #include "../log.h"
 
 #include "USBRelay.h"
 #include "serialutil.h"
 
 #include "Plugin.h"
 class USBRelayPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
 public:
     USBRelayPlugin() : FPPPlugins::Plugin("USBRelay") {
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
     ChannelOutput(startChannel, channelCount), m_subType(RELAY_DVC_UNKNOWN), m_relayCount(0), m_fd(-1) {
     LogDebug(VB_CHANNELOUT, "USBRelayOutput::USBRelayOutput(%u, %u)\n",
              startChannel, channelCount);
 }
 
 /*
  *
  */
 USBRelayOutput::~USBRelayOutput() {
     LogDebug(VB_CHANNELOUT, "USBRelayOutput::~USBRelayOutput()\n");
     if (m_fd >= 0) {
         close(m_fd);
         m_fd = -1;
     }
 }
 
 /*
  *
  */
 void USBRelayOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
     addRange(m_startChannel, m_startChannel + m_relayCount - 1);
 }
 
 /*
  *
  */
 int USBRelayOutput::Init(Json::Value config) {
     LogDebug(VB_CHANNELOUT, "USBRelayOutput::Init(JSON)\n");
 
     if (!config.isMember("device") || config["device"].asString().empty()) {
         LogErr(VB_CHANNELOUT, "Invalid Config: 'device' field is missing or empty\n");
         return 0;
     }
     std::string device = config["device"].asString();
     // Prepend "/dev/" if it’s not already there
     if (device.find("/dev/") != 0) {
         device = "/dev/" + device;
     }
     LogDebug(VB_CHANNELOUT, "Device from config (adjusted): '%s'\n", device.c_str());
 
     std::string subType = config["subType"].asString();
     LogDebug(VB_CHANNELOUT, "SubType from config: '%s'\n", subType.c_str());
 
     if (subType == "Bit")
         m_subType = RELAY_DVC_BIT;
     else if (subType == "ICStation")
         m_subType = RELAY_DVC_ICSTATION;
     else if (subType == "CH340")
         m_subType = RELAY_DVC_CH340;
     else {
         LogErr(VB_CHANNELOUT, "Invalid Config: unknown subtype '%s'\n", subType.c_str());
         return 0;
     }
 
     m_relayCount = config["channelCount"].asInt();
     LogDebug(VB_CHANNELOUT, "Channel count from config: %d\n", m_relayCount);
 
     if (m_subType == RELAY_DVC_CH340 && m_relayCount != 1) {
         LogWarn(VB_CHANNELOUT, "CH340 supports only 1 relay; forcing channelCount to 1\n");
         m_relayCount = 1;
     }
 
     // Open and configure the serial port directly
     LogDebug(VB_CHANNELOUT, "Attempting to open serial port: '%s'\n", device.c_str());
     m_fd = SerialOpen(device.c_str(), 9600, "8N1");
     if (m_fd < 0) {
         LogErr(VB_CHANNELOUT, "Failed to open serial port '%s': %s\n", device.c_str(), strerror(errno));
         return 0;
     }
     LogDebug(VB_CHANNELOUT, "Serial port opened successfully, fd=%d\n", m_fd);
 
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
             //LogWarn(VB_CHANNELOUT, "Did not receive a response byte from ICstation relay, unable to confirm number of relays. Using configuration value from UI\n");
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
             LogWarn(VB_CHANNELOUT, "Warning: ICStation USB Relay response of 0x%02x doesn't match known values. Unable to detect number of relays.\n", c_reply);
         }
 
         if (foundICS)
             write(m_fd, &c_open, 1);
     } else if (m_subType == RELAY_DVC_CH340) {
         LogInfo(VB_CHANNELOUT, "Initialized CH340 USB Relay\n");
     }
 
     int baseInitResult = ChannelOutput::Init(config);
     LogDebug(VB_CHANNELOUT, "Base ChannelOutput::Init returned: %d\n", baseInitResult);
     return baseInitResult;
 }
 
 /*
  *
  */
 int USBRelayOutput::Close(void) {
     LogDebug(VB_CHANNELOUT, "USBRelayOutput::Close()\n");
     if (m_fd >= 0) {
         close(m_fd);
         m_fd = -1;
     }
     return ChannelOutput::Close();
 }
 
 /*
  *
  */
 int USBRelayOutput::SendData(unsigned char* channelData) {
     LogExcess(VB_CHANNELOUT, "USBRelayOutput::RawSendData(%p)\n", channelData);
 
     if (m_subType == RELAY_DVC_BIT || m_subType == RELAY_DVC_ICSTATION) {
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
     } else if (m_subType == RELAY_DVC_CH340) {
        unsigned char cmd[4];
        for (int relay = 1; relay <= m_relayCount; relay++) {
            cmd[0] = 0xA0;               // Start byte
            cmd[1] = relay;              // Relay number (supports multiple relays)
            cmd[2] = channelData[relay - 1] ? 0x01 : 0x00;  // On (0x01) or Off (0x00)
            cmd[3] = cmd[0] + cmd[1] + cmd[2];              // Checksum
            write(m_fd, cmd, 4);
    } else if (m_subType == RELAY_DVC_ICSTATION) {
         // ICStation doesn’t seem to use SendData for relay control in this code; handled in Init
     }
 
     return m_relayCount;
 }
 
 /*
  *
  */
 void USBRelayOutput::DumpConfig(void) {
     LogDebug(VB_CHANNELOUT, "USBRelayOutput::DumpConfig()\n");
     ChannelOutput::DumpConfig();
 }