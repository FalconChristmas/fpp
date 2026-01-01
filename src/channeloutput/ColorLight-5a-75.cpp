/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

/*
 * Packet Format:
 *
 * All sent packets have the same source and destination MAC addresses:
 * 	- Destination MAC: 11:22:33:44:55:66
 * 	- Source MAC:      22:22:33:44:55:66 (doesn't seem necessary to harcoded this)
 *
 * Received packets are broadcast with a source MAC of 22:22:33:44:55:66, dest of FF:FF:FF:FF:FF:FF
 *
 * Packet Type is a single byte.  First byte of data is stored with the packet
 * type in the 'ether_type' field in the ethernet header.  Data Byte count
 * includes first byte of data stored in the ether_type field.
 *
 * Packet types:
 * - 0x01 - Display/Sync Frame (sent twice for v13+ FW) (CL_SYNC_PACKET_TYPE)
 *          Older FW accepts this twice from LEDVision but is timing sensitive.
 *          FPP only sends once for older firmware.
 *      - Data Bytes:      99 (CL_SYNC_PACKET_SIZE - 13)
 *      - Data[0]:         0x00 - Sent from S2 Sender
 *                         0x07 - Sent from PC/Netcard
 *   	- Data[22]:        0xff - Brightness
 *      - Data[23]:        0x00 - Value in 0x01 packet from S2 Sender
 *   	                   0x05 - Value in 0x01 packet from PC/Netcard
 *   	- Data[25]:        0xff - Also brightness related
 *   	- Data[26]:        0xff - Also brightness related
 *   	- Data[27]:        0xff - Also brightness related
 *
 *   0x02 - Write receiver layout (dual 32x16 receivers, #0 @ 0,0, #1 @ 32,0
 *      - Data[0]:         0x00 - Receiver #
 *      - Data[1]:         0x00 - Receiver #
 *      - Data[2]:         0x00 - Receiver #
 *
 *      Receiver #0 (of 64, #0-63)
 *      - Data[3]:         0x00 - Receiver #
 *      - Data[4]:         0x00 - Receiver #
 *      - Data[5]:         0x00 - Receiver #
 *      - Data[6]:         0x00 - Receiver #
 *      - Data[7]:         0x00 - Receiver #  receiver width MSB
 *      - Data[8]:         0x20 - Receiver #  receiver width LSB
 *      - Data[9]:         0x00 - Receiver #  receiver height MSB
 *      - Data[10]:        0x10 - Receiver #  receiver height LSB
 *      - Data[11]:        0x00 - Receiver #
 *      - Data[12]:        0x00 - Receiver #
 *      - Data[13]:        0x00 - Receiver #  next receiver xOffset MSB
 *      - Data[14]:        0x20 - Receiver #  next receiver xOffset LSB
 *      - Data[15]:        0x00 - Receiver #  next receiver yOffset MSB
 *      - Data[16]:        0x00 - Receiver #  next receiver yOffset LSB
 *      - Data[17]:        0x00 - Receiver #  total display width MSB
 *      - Data[18]:        0x40 - Receiver #  total display width LSB
 *      - Data[19]:        0x00 - Receiver #  total display height MSB
 *      - Data[20]:        0x10 - Receiver #  total display height LSB
 *      - Data[21]:        0x00 - Receiver #
 *      - Data[22]:        0x00 - Receiver #
 *
 *      Receiver #1-63)
 *      - repeating 20 bytes of data in same format as receiver #0
 *
 *   0x07 - Discover (CL_DISC_PACKET_TYPE)
 *      - Data Bytes:      271 (CL_DISC_PACKET_SIZE - 13)
 *      - Data[3]:         0x00 - first receiver
 *                         0x01 - second receiver, etc.
 *
 *   0x08 - Discover Reply (from receiver to sender) (CL_RESP_PACKET_TYPE)
 *      - Data Bytes:      1057 (CL_RESP_PACKET_SIZE - 13)
 *      - Data[0]:         0x05 - 5A card
 *      - Data[2]:         Firmware Version MSB
 *      - Data[3]:         Firmware Version LSB
 *      - Data[38]:        Received Packet Count? MSB
 *      - Data[39]:        Received Packet Count?
 *      - Data[40]:        Received Packet Count?
 *      - Data[41]:        Received Packet Count? LSB
 *      - Data[46]:        Uptime in ms MSB
 *      - Data[47]:        Uptime in ms
 *      - Data[48]:        Uptime in ms
 *      - Data[49]:        Uptime in ms LSB
 *      - Data[85]:        Receiver Card # (0x00, 0x01, etc.)
 *
 *      These don't seem totally correct, offsetting a receiver card doesn't result in values that
 *      match the cabinet width/height fields, so there seems to be some other math going on.
 *      - Data[21]:        Cabinet Width? MSB
 *      - Data[22]:        Cabinet Width? LSB
 *      - Data[23]:        Cabinet Height? MSB
 *      - Data[24]:        Cabinet Height? LSB
 *      - Data[??]:        Cabinet X-Offset MSB
 *      - Data[??]:        Cabinet X-Offset LSB
 *      - Data[??]:        Cabinet Y-Offset MSB
 *      - Data[??]:        Cabinet Y-Offset LSB
 *
 *    Data    *  *  *  *  *  *  <-- Fields of interest
 *    Offset 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 - Offsets in packet data
 *           00 00 00 20 00 10 00 00 00 00 00 00 01 02 03 ff - 32x16 size, r0: 32x16 cabinet, offset 0,0
 *           00 10 00 20 00 20 00 00 00 00 00 00 01 02 03 ff - 32x32 size, r1: 32x16 cabinet, offset 0,16
 *
 *           00 00 00 20 00 10 00 00 00 00 00 00 01 02 03 ff - 32x16 size, r0: 32x16 cabinet, offset 0,0
 *           00 10 00 40 00 20 00 00 00 00 00 00 01 02 03 ff - 64x32 size, r1: 32x16 cabinet, offset 32,16
 *
 *           00 00 00 20 00 10 00 00 00 00 00 00 01 02 03 ff - 32x16 size, r0: 32x16 cabinet, offset 0,0
 *           00 10 00 20 00 20 00 00 00 00 00 00 01 02 03 ff - 64x16 size, r1: 32x16 cabinet, offset 32,0
 *
 *   0x10 - ???? - sent when sending config to receiver
 *   	- Data Length:     1026
 *
 *   0x11 - Save Config??
 *      - Data Bytes:      1283
 *      - Data[3]:         0x00 - first receiver, 0x01 2nd, etc.
 *
 *   0x18 - ???? - sent when sending config to receiver
 *
 *   0x1F - ???? - sent when sending config to receiver
 *   	- Data Length:     1034
 *
 *   0x26 - ???? - sent when sending config to receiver
 *   	- Data Length:     264
 *
 *   0x31 - ???? - sent when sending config to receiver.  Some kind of table, 2 bytes per entry?
 *
 *   0x32 - ???? - sent when sending config to receiver
 *
 *   0x76 - ???? - sent when sending config to receiver.  Some kind of table, 3 bytes per entry?
 *
 *   0x0A - Panel Brightness (sent twice for v13+ FW, but possbily uncessary) (CL_BRIG_PACKET_TYPE)
 *   	- Data Length:     64 (CL_BRIG_PACKET_SIZE - 13)
 *   	- Data[0]:         0xFF - Brightness
 *   	- Data[1]:         0xFF - Brightness
 *   	- Data[2]:         0xFF - Brightness
 *   	- Data[3]:         0xFF
 *
 *   0x55 - Pixel Data (sent twice per row for v13+ FW, but seems uncessary to do so) (CL_PIXL_PACKET_TYPE)
 *      - Data Length:     CL_PIXL_HEADER_SIZE + (Row_Width * 3)
 *   	- Data[0]:         Row Number MSB
 *   	- Data[1]:         Row Number LSB
 *   	- Data[2]:         MSB of pixel offset for this packet
 *   	- Data[3]:         LSB of pixel offset for this packet
 *   	- Data[4]:         MSB of pixel count in packet
 *   	- Data[5]:         LSB of pixel count in packet
 *   	- Data[6]:         0x08 - ?? unsure what this is
 *   	- Data[7]:         0x88 - ?? unsure what this is
 *   	- Data[8-end]:     RGB order pixel data
 *
 * Notes:
 * - Brightness may only work on newer firmware. Doesn't seem to work on v2.x and v3.x ColorLight FW.
 *
 * Other sources of info:
 * https://www.mylifesucks.de/oss/mplayer-colorlight/
 * https://hkubota.wordpress.com/2022/01/31/winter-project-colorlight-5a-75b-protocol/
 */
#include "fpp-pch.h"

#ifndef PLATFORM_OSX
#include <linux/if_packet.h>
#include <netinet/ether.h>
#else
#include <net/bpf.h>
#endif

#include "../SysSocket.h"
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <cmath>
#include <errno.h>
#include <fstream>
#include <iostream>

#include "../Warnings.h"
#include "../common.h"
#include "../log.h"
#include "../settings.h"

#include "ColorLight-5a-75.h"
#include "overlays/PixelOverlay.h"

#include "Plugin.h"

#ifdef PLATFORM_OSX
int bindBPFSocket(const std::string iface) {
    char buf[11] = { 0 };
    int i = 0;
    int m_fd = -1;
    for (int i = 0; i < 255; i++) {
        snprintf(buf, sizeof(buf), "/dev/bpf%i", i);
        m_fd = open(buf, O_RDWR);
        if (m_fd != -1) {
            break;
        }
    }
    if (m_fd == -1) {
        LogErr(VB_CHANNELOUT, "Error opening bpf file: %s\n", strerror(errno));
        return -1;
    }

    struct ifreq bound_if;
    memset(&bound_if, 0, sizeof(bound_if));
    strcpy(bound_if.ifr_name, iface.c_str());
    if (ioctl(m_fd, BIOCSETIF, &bound_if) > 0) {
        LogErr(VB_CHANNELOUT, "Cannot bind bpf device to physical device %s, exiting\n", iface.c_str());
    }
    int yes = 1;
    ioctl(m_fd, BIOCSHDRCMPLT, &yes);
    return m_fd;
}
#endif

class ColorLight5a75Plugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    ColorLight5a75Plugin() :
        FPPPlugins::Plugin("ColorLight5a75") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new ColorLight5a75Output(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new ColorLight5a75Plugin();
}
}

/*
 *
 */
ColorLight5a75Output::ColorLight5a75Output(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    m_width(0),
    m_height(0),
    m_colorOrder(FPPColorOrder::kColorOrderRGB),
    m_fd(-1),
    m_rowSize(0),
    m_panelWidth(0),
    m_panelHeight(0),
    m_panels(0),
    m_rows(0),
    m_outputs(0),
    m_longestChain(0),
    m_invertedData(0),
    m_slowCount(0),
    m_flippedLayout(0),
    m_highestFirmwareVersion(0) {
    LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::ColorLight5a75Output(%u, %u)\n",
             startChannel, channelCount);
}

/*
 *
 */
ColorLight5a75Output::~ColorLight5a75Output() {
    LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::~ColorLight5a75Output()\n");

    for (int i = 0; i < m_synciovecs.size(); i++) {
        free(m_synciovecs[i].iov_base);

        // Higher firmware has duplicate sync packet which re-uses same buffer
        if (m_highestFirmwareVersion >= 13)
            i++;
    }

    for (int i = 0; i < m_iovecs.size(); i++) {
        free(m_iovecs[i].iov_base);

        // Higher firmware has duplicate brightness packet which re-uses same buffer
        if ((i == 0) && (m_highestFirmwareVersion >= 13))
            i += 3;
        else
            i++; // Every second iovec is either part of the header malloc() or external data
    }

    if (m_fd >= 0)
        close(m_fd);
}

/*
 *
 */
int ColorLight5a75Output::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::Init(JSON)\n");

    m_panelWidth = config["panelWidth"].asInt();
    m_panelHeight = config["panelHeight"].asInt();

    if (!m_panelWidth)
        m_panelWidth = 32;

    if (!m_panelHeight)
        m_panelHeight = 16;

    m_invertedData = config["invertedData"].asInt();

    m_colorOrder = ColorOrderFromString(config["colorOrder"].asString());

    m_panelMatrix =
        std::make_unique<PanelMatrix>(m_panelWidth, m_panelHeight, m_invertedData);

    if (!m_panelMatrix) {
        LogErr(VB_CHANNELOUT, "Unable to create PanelMatrix\n");
        return 0;
    }

    if (config.isMember("cfgVersion")) {
        m_flippedLayout = config["cfgVersion"].asInt() >= 2 ? 0 : 1;
    } else {
        m_flippedLayout = 1;
    }

    for (int i = 0; i < config["panels"].size(); i++) {
        Json::Value p = config["panels"][i];
        char orientation = 'N';
        std::string o = p["orientation"].asString();
        if (o != "") {
            orientation = o[0];
        }

        if (m_flippedLayout) {
            switch (orientation) {
            case 'N':
                orientation = 'U';
                break;
            case 'U':
                orientation = 'N';
                break;
            case 'R':
                orientation = 'L';
                break;
            case 'L':
                orientation = 'R';
                break;
            }
        }

        if (p["colorOrder"].asString() == "")
            p["colorOrder"] = ColorOrderToString(m_colorOrder);

        m_panelMatrix->AddPanel(p["outputNumber"].asInt(),
                                p["panelNumber"].asInt(), orientation,
                                p["xOffset"].asInt(), p["yOffset"].asInt(),
                                ColorOrderFromString(p["colorOrder"].asString()));

        if (p["outputNumber"].asInt() > m_outputs)
            m_outputs = p["outputNumber"].asInt();

        if (p["panelNumber"].asInt() > m_longestChain)
            m_longestChain = p["panelNumber"].asInt();
    }

    // Both of these are 0-based, so bump them up by 1 for comparisons
    m_outputs++;
    m_longestChain++;

    m_panels = m_panelMatrix->PanelCount();

    m_rows = m_outputs * m_panelHeight;

    m_width = m_panelMatrix->Width();
    m_height = m_panelMatrix->Height();

    m_channelCount = m_width * m_height * 3;

    m_outputFrame = std::make_unique<char[]>(m_outputs * m_longestChain * m_panelHeight * m_panelWidth * 3);

    m_matrix = std::make_unique<Matrix>(m_startChannel, m_width, m_height);

    if (config.isMember("subMatrices")) {
        for (int i = 0; i < config["subMatrices"].size(); i++) {
            Json::Value sm = config["subMatrices"][i];

            m_matrix->AddSubMatrix(
                sm["enabled"].asInt(),
                sm["startChannel"].asInt() - 1,
                sm["width"].asInt(),
                sm["height"].asInt(),
                sm["xOffset"].asInt(),
                sm["yOffset"].asInt());
        }
    }

    float gamma = 1.0;
    if (config.isMember("gamma")) {
        gamma = atof(config["gamma"].asString().c_str());
    }
    if (gamma < 0.01 || gamma > 50.0) {
        gamma = 1.0;
    }
    for (int x = 0; x < 256; x++) {
        float f = x;
        f = 255.0 * pow(f / 255.0f, gamma);
        if (f > 255.0) {
            f = 255.0;
        }
        if (f < 0.0) {
            f = 0.0;
        }
        m_gammaCurve[x] = round(f);
    }

    if (config.isMember("interface"))
        m_ifName = config["interface"].asString();
    else
        m_ifName = "eth1";

    m_rowSize = m_longestChain * m_panelWidth * 3;

#ifndef PLATFORM_OSX

    // Check if interface is up
    std::ifstream ifstate_src("/sys/class/net/" + m_ifName + "/operstate");
    std::string ifstate;

    if (ifstate_src.is_open()) {
        ifstate_src >> ifstate; // pipe file's content into stream
        ifstate_src.close();
    }

    m_highestFirmwareVersion = getSettingInt("ColorlightFirmwareVersion");
    m_colorlightDisable = true;
    m_colorlightDisable = getSettingInt("ColorlightLinkDownDisable") == 1;

    if (config.isMember("linkCheck")) {
        m_colorlightDisable = config["linkCheck"].asBool();
    }
    if (config.isMember("firmwareVersion")) {
        m_highestFirmwareVersion = config["firmwareVersion"].asInt();
    }

    if (ifstate != "up") {
        LogErr(VB_CHANNELOUT, "Error ColorLight: Configured interface %s does not have link %s\n", m_ifName.c_str(), strerror(errno));
        WarningHolder::AddWarning(7, "ColorLight: Configured interface " + m_ifName + " does not have link");
        if (m_colorlightDisable) {
            return 0;
        }
    } else {
        WarningHolder::RemoveWarning(7, "ColorLight: Configured interface " + m_ifName + " does not have link");
    }

    // Check interface is 1000Mbps capable and display error if not
    ifspeed = 0;
    std::ifstream ifspeed_src("/sys/class/net/" + m_ifName + "/speed");

    if (ifspeed_src.is_open()) { // always check whether the file is open
        ifspeed_src >> ifspeed;  // pipe file's content into stream
        ifspeed_src.close();
    }

    if (ifspeed > 0 && ifspeed < 1000) {
        LogErr(VB_CHANNELOUT, "Error ColorLight: Configured interface %s is not 1000Mbps Capable: %s\n", m_ifName.c_str(), strerror(errno));
        WarningHolder::AddWarning(8, "ColorLight: Configured interface " + m_ifName + " is not 1000Mbps Capable");
        if (m_colorlightDisable) {
            return 0;
        }
    } else {
        WarningHolder::RemoveWarning(8, "ColorLight: Configured interface " + m_ifName + " is not 1000Mbps Capable");
    }

    // Open our raw socket
    if ((m_fd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
        LogErr(VB_CHANNELOUT, "Error creating raw socket: %s\n", strerror(errno));
        return 0;
    }

    // Get the output interface ID
    memset(&m_if_idx, 0, sizeof(struct ifreq));
    strcpy(m_if_idx.ifr_name, m_ifName.c_str());
    if (ioctl(m_fd, SIOCGIFINDEX, &m_if_idx) < 0) {
        LogErr(VB_CHANNELOUT, "Error getting index of %s interface: %s\n",
               m_ifName.c_str(), strerror(errno));
        return 0;
    }

    m_sock_addr.sll_family = AF_PACKET;
    m_sock_addr.sll_ifindex = m_if_idx.ifr_ifindex;
    m_sock_addr.sll_halen = ETH_ALEN;

    unsigned char dhost[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    memcpy(m_sock_addr.sll_addr, dhost, 6);

    // Force packets out the desired interface
    if ((bind(m_fd, (struct sockaddr*)&m_sock_addr, sizeof(m_sock_addr))) == -1) {
        LogErr(VB_CHANNELOUT, "bind() failed\n");
        return 0;
    }

    // 0 is auto-detect, else use the value loaded from the ColorlightFirmwareVersion setting
    if (!m_highestFirmwareVersion) {
        // Query the receivers to find out what firmware vesion is installed.
        GetReceiverInfo();

        if (!m_receivers.size() && m_colorlightDisable) {
            return 0;
        }
    }

#else
    m_fd = bindBPFSocket(m_ifName);
#endif

    unsigned int byteCount = 0;
    unsigned int p = 0;
    unsigned char* header = nullptr;
    unsigned char* data = nullptr;
    unsigned char brightness = 0xff;

    if (config.isMember("brightness")) {
        brightness = config["brightness"].asInt(); // 0-100% scale
        if (!brightness)
            brightness = 0xFF;
        else
            brightness = 2.55 * brightness; // Scale up to 0-255
    }

    /////////////////////////////////////////////////
    // Prep the brightness and pixel data packets
    int packetCount = 1 + (m_rows * (((int)(m_rowSize - 1) / CL_MAX_CHAN_PER_PACKET) + 1));

    if (m_highestFirmwareVersion >= 13) {
        packetCount += 1; // 0x0A brightness sent twice
    }

    m_msgs.resize(packetCount);
    m_iovecs.resize(packetCount * 2);

    // Brightness packet
    byteCount = CL_BRIG_PACKET_SIZE;
    header = (unsigned char*)malloc(byteCount);
    memset(header, 0, byteCount);

    SetHostMACs(header);

    // Packet Type follows Dest MAC, data follows type
    header[CL_PACKET_TYPE_OFFSET] = CL_BRIG_PACKET_TYPE;
    data = header + CL_PACKET_DATA_OFFSET;

    data[0] = brightness;
    data[1] = brightness;
    data[2] = brightness;
    data[3] = 0xff;

    // Split this apart since it is grouped with pixel data packets which are split into two iovec entries
    m_iovecs[p * 2].iov_base = header;
    m_iovecs[p * 2].iov_len = CL_PACKET_DATA_OFFSET;
    m_iovecs[p * 2 + 1].iov_base = header + CL_PACKET_DATA_OFFSET;
    m_iovecs[p * 2 + 1].iov_len = byteCount - CL_PACKET_DATA_OFFSET;
    p++;

    if (m_highestFirmwareVersion >= 13) {
        // Send duplicate brightness packet
        m_iovecs[p * 2].iov_base = m_iovecs[(p - 1) * 2].iov_base;
        m_iovecs[p * 2].iov_len = m_iovecs[(p - 1) * 2].iov_len;
        m_iovecs[p * 2 + 1].iov_base = m_iovecs[(p - 1) * 2 + 1].iov_base;
        m_iovecs[p * 2 + 1].iov_len = m_iovecs[(p - 1) * 2 + 1].iov_len;
        p++;
    }

    // Pixel Data row packets - Type 0x55
    char* rowPtr = m_outputFrame.get();
    unsigned int dSize = 0;
    unsigned int part = 0;
    unsigned int hSize = CL_PACKET_DATA_OFFSET + CL_PIXL_HEADER_SIZE;
    unsigned int offset = 0;
    unsigned int bytesInPacket = 0;
    unsigned int pixelOffset = 0;
    unsigned int pixelsInPacket = 0;

    for (int row = 0; row < m_rows; row++) {
        part = 0;
        offset = 0;

        while (offset < m_rowSize) {
            header = (unsigned char*)malloc(hSize);
            memset(header, 0, hSize);

            SetHostMACs(header);

            // Packet Type follows Dest MAC, data follows type
            header[CL_PACKET_TYPE_OFFSET] = CL_PIXL_PACKET_TYPE;
            data = header + CL_PACKET_DATA_OFFSET;

            if ((offset + CL_MAX_CHAN_PER_PACKET) > m_rowSize)
                bytesInPacket = m_rowSize - offset;
            else
                bytesInPacket = CL_MAX_CHAN_PER_PACKET;

            pixelOffset = offset / 3;
            pixelsInPacket = bytesInPacket / 3;

            data[0] = row >> 8;              // Pixel Row MSB
            data[1] = row & 0xFF;            // Pixel Row LSB
            data[2] = pixelOffset >> 8;      // Pixel Offset MSB
            data[3] = pixelOffset & 0xFF;    // Pixel Offset LSB
            data[4] = pixelsInPacket >> 8;   // Pixels In Packet MSB
            data[5] = pixelsInPacket & 0xFF; // Pixels In Packet LSB
            data[6] = 0x08;                  // ?? not sure what this value is
            data[7] = 0x88;                  // ?? not sure what this value is

            m_iovecs[p * 2].iov_base = header;
            m_iovecs[p * 2].iov_len = hSize;
            m_iovecs[p * 2 + 1].iov_base = rowPtr + offset;
            m_iovecs[p * 2 + 1].iov_len = bytesInPacket;
            p++;

#if 0
            // LEDVision sends data rows duplicated as well, but doesn't seem necessary.
            // If this is enabled, the packetCount needs to be adjusted when set above.
            if (0 && m_highestFirmwareVersion >= 13) {
                // Send duplicate row data packet
                m_iovecs[p * 2].iov_base     = m_iovecs[(p-1) * 2].iov_base;
                m_iovecs[p * 2].iov_len      = m_iovecs[(p-1) * 2].iov_len;
                m_iovecs[p * 2 + 1].iov_base = m_iovecs[(p-1) * 2 + 1].iov_base;
                m_iovecs[p * 2 + 1].iov_len  = m_iovecs[(p-1) * 2 + 1].iov_len;
                p++;
            }
#endif

            offset += bytesInPacket;
            part++;
        }

        rowPtr += m_rowSize;
    }

    for (int m = 0; m < packetCount; m++) {
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_hdr.msg_iov = &m_iovecs[m * 2];
        msg.msg_hdr.msg_iovlen = 2;
        m_msgs[m] = msg;
    }

    ////////////////////////////////////////////////////////////////////
    // Prep the Display Frame packet(s) - sent in PrepData()
    if (m_highestFirmwareVersion >= 13) {
        m_syncMsgs.resize(2);
        m_synciovecs.resize(2);
    } else {
        m_syncMsgs.resize(1);
        m_synciovecs.resize(1);
    }

    p = 0;
    byteCount = CL_SYNC_PACKET_SIZE;
    header = (unsigned char*)malloc(byteCount);
    memset(header, 0, byteCount);

    SetHostMACs(header);

    // Packet Type follows Dest MAC, data follows type
    header[CL_PACKET_TYPE_OFFSET] = CL_SYNC_PACKET_TYPE;
    data = header + CL_PACKET_DATA_OFFSET;

    data[0] = 0x07;

    data[22] = brightness;
    data[23] = 0x05; // ?? not sure what this value is

    data[25] = brightness;
    data[26] = brightness;
    data[27] = brightness;

    m_synciovecs[p].iov_base = header;
    m_synciovecs[p].iov_len = byteCount;
    p++;

    if (m_highestFirmwareVersion >= 13) {
        // Send duplicate display frame packet
        m_synciovecs[p].iov_base = m_synciovecs[p - 1].iov_base;
        m_synciovecs[p].iov_len = m_synciovecs[p - 1].iov_len;
        p++;
    }

    for (int m = 0; m < p; m++) {
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_hdr.msg_iov = &m_synciovecs[m];
        msg.msg_hdr.msg_iovlen = 1;
        m_syncMsgs[m] = msg;
    }

    // Create Pixel Overlay Models if needed
    if (PixelOverlayManager::INSTANCE.isAutoCreatePixelOverlayModels()) {
        std::string dd = "LED Panels";
        if (config.isMember("LEDPanelMatrixName") && !config["LEDPanelMatrixName"].asString().empty()) {
            dd = config["LEDPanelMatrixName"].asString();
        }
        if (config.isMember("description")) {
            dd = config["description"].asString();
        }
        std::string desc = dd;
        int count = 0;
        while (PixelOverlayManager::INSTANCE.getModel(desc) != nullptr) {
            count++;
            desc = dd + "-" + std::to_string(count);
        }
        PixelOverlayManager::INSTANCE.addAutoOverlayModel(desc,
                                                          m_startChannel, m_channelCount, 3,
                                                          "H", m_invertedData ? "BL" : "TL",
                                                          m_height, 1);
        m_autoCreatedModelName = desc;
    }

    return ChannelOutput::Init(config);
}

/*
 *
 */
int ColorLight5a75Output::Close(void) {
    LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::Close()\n");
    if (!m_autoCreatedModelName.empty()) {
        PixelOverlayManager::INSTANCE.removeAutoOverlayModel(m_autoCreatedModelName);
        m_autoCreatedModelName.clear();
    }
    for (auto& m : m_warnings) {
        WarningHolder::RemoveWarning(m);
    }
    return ChannelOutput::Close();
}

void ColorLight5a75Output::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

void ColorLight5a75Output::OverlayTestData(unsigned char* channelData, int cycleNum, float percentOfCycle, int testType, const Json::Value& config) {
    for (int output = 0; output < m_outputs; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();
        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];
            int chain = m_panelMatrix->m_panels[panel].chain;

            if (m_flippedLayout)
                chain = (m_longestChain - 1) - m_panelMatrix->m_panels[panel].chain - 1;

            m_panelMatrix->m_panels[panel].drawTestPattern(channelData + m_startChannel, cycleNum, testType);
        }
    }
}

int ColorLight5a75Output::SendMessages(std::vector<struct mmsghdr>& msgsToSend) {
    long long startTime = GetTimeMS();
    struct mmsghdr* msgs = &msgsToSend[0];
    int msgCount = msgsToSend.size();
    if (msgCount == 0)
        return 0;

    errno = 0;

    int oc = SendMessagesHelper(msgs, msgCount);
    int outputCount = 0;
    if (oc > 0) {
        outputCount = oc;
    }

    int errCount = 0;
    bool done = false;
    while ((outputCount != msgCount) && !done) {
        errCount++;
        errno = 0;
        oc = SendMessagesHelper(&msgs[outputCount], msgCount - outputCount);
        if (oc > 0) {
            outputCount += oc;
        }
        if (outputCount != msgCount) {
            long long tm = GetTimeMS();
            long long totalTime = tm - startTime;
            if (totalTime < 22) {
                // we'll keep trying for up to 22ms, but give the network stack some time to flush some buffers
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            } else {
                done = true;
            }
        }
    }
    long long endTime = GetTimeMS();
    long long totalTime = endTime - startTime;
    if (outputCount != msgCount) {
        int tti = (int)totalTime;
        LogWarn(VB_CHANNELOUT, "sendmmsg() failed for ColorLight output (Socket: %d   output count: %d/%d   time: %dms) with error: %d   %s, errorcount: %d\n",
                m_fd, outputCount, msgCount, tti, errno, strerror(errno), errCount);
        m_slowCount++;
        if (m_slowCount > 3) {
            LogWarn(VB_CHANNELOUT, "Repeated frames taking more than 20ms to send to ColorLight");
            WarningHolder::AddWarningTimeout("Repeated frames taking more than 20ms to send to ColorLight", 30);
        }
    } else {
        m_slowCount = 0;
    }

    return oc;
}

/*
 *
 */
void ColorLight5a75Output::PrepData(unsigned char* channelData) {
    m_matrix->OverlaySubMatrices(channelData);

    unsigned char* r = NULL;
    unsigned char* g = NULL;
    unsigned char* b = NULL;
    unsigned char* s = NULL;
    unsigned char* dst = NULL;
    int pw3 = m_panelWidth * 3;

    channelData += m_startChannel; // FIXME, this function gets offset 0

    for (int output = 0; output < m_outputs; output++) {
        int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();

        for (int i = 0; i < panelsOnOutput; i++) {
            int panel = m_panelMatrix->m_outputPanels[output][i];
            int chain = (m_longestChain - 1) - m_panelMatrix->m_panels[panel].chain;

            if (m_flippedLayout)
                chain = m_panelMatrix->m_panels[panel].chain;

            for (int y = 0; y < m_panelHeight; y++) {
                int px = chain * m_panelWidth;
                int yw = y * m_panelWidth * 3;

                dst = (unsigned char*)(m_outputFrame.get() + (((((output * m_panelHeight) + y) * m_panelWidth * m_longestChain) + px) * 3));

                for (int x = 0; x < pw3; x += 3) {
                    *(dst++) = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw + x]]];
                    *(dst++) = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw + x + 1]]];
                    *(dst++) = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw + x + 2]]];

                    px++;
                }
            }
        }
    }

    SendMessages(m_msgs);
}

int ColorLight5a75Output::SendMessagesHelper(struct mmsghdr* msgs, int msgCount) {
#ifdef PLATFORM_OSX
    char buf[1500];
    for (int m = 0; m < msgCount; m++) {
        int cur = 0;
        for (int io = 0; io < msgs[m].msg_hdr.msg_iovlen; io++) {
            memcpy(&buf[cur], msgs[m].msg_hdr.msg_iov[io].iov_base, msgs[m].msg_hdr.msg_iov[io].iov_len);
            cur += msgs[m].msg_hdr.msg_iov[io].iov_len;
        }
        int bytes_sent = write(m_fd, buf, cur);
        if (bytes_sent != cur) {
            return m;
        }
    }
    return msgCount;
#else
    return sendmmsg(m_fd, msgs, msgCount, MSG_DONTWAIT);
#endif
}

/*
 *
 */
int ColorLight5a75Output::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "ColorLight5a75Output::SendData(%p)\n", channelData);

    SendMessages(m_syncMsgs);

    return m_channelCount;
}

/*
 *
 */
void ColorLight5a75Output::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    Width          : %d\n", m_width);
    LogDebug(VB_CHANNELOUT, "    Height         : %d\n", m_height);
    LogDebug(VB_CHANNELOUT, "    Rows           : %d\n", m_rows);
    LogDebug(VB_CHANNELOUT, "    Row Size       : %d\n", m_rowSize);
    LogDebug(VB_CHANNELOUT, "    m_fd           : %d\n", m_fd);
    LogDebug(VB_CHANNELOUT, "    Outputs        : %d\n", m_outputs);
    LogDebug(VB_CHANNELOUT, "    Longest Chain  : %d\n", m_longestChain);
    LogDebug(VB_CHANNELOUT, "    Inverted Data  : %d\n", m_invertedData);
    LogDebug(VB_CHANNELOUT, "    Interface      : %s\n", m_ifName.c_str());

    ChannelOutput::DumpConfig();
}

/*
 *
 */
void ColorLight5a75Output::SetHostMACs(void* ptr) {
    struct ether_header* eh = (struct ether_header*)ptr;

    // Set the source MAC address
    eh->ether_shost[0] = 0x22;
    eh->ether_shost[1] = 0x22;
    eh->ether_shost[2] = 0x33;
    eh->ether_shost[3] = 0x44;
    eh->ether_shost[4] = 0x55;
    eh->ether_shost[5] = 0x66;

    // Set the dest MAC address
    eh->ether_dhost[0] = 0x11;
    eh->ether_dhost[1] = 0x22;
    eh->ether_dhost[2] = 0x33;
    eh->ether_dhost[3] = 0x44;
    eh->ether_dhost[4] = 0x55;
    eh->ether_dhost[5] = 0x66;
}

/*
 *
 */
void ColorLight5a75Output::GetReceiverInfo() {
    int sockfd;
    int sockopt;
    struct ifreq ifopts;
    struct mmsghdr msgs[1];
    struct iovec iovecs[1];

    unsigned char discoverBuffer[CL_DISC_PACKET_SIZE];
    memset(discoverBuffer, 0, CL_DISC_PACKET_SIZE);

    // Setup Discover packet
    SetHostMACs(discoverBuffer);

    discoverBuffer[CL_PACKET_TYPE_OFFSET] = CL_DISC_PACKET_TYPE;

    iovecs[0].iov_base = discoverBuffer;
    iovecs[0].iov_len = CL_DISC_PACKET_SIZE;

    memset(msgs, 0, sizeof(struct mmsghdr));
    msgs[0].msg_hdr.msg_iov = &iovecs[0];
    msgs[0].msg_hdr.msg_iovlen = 1;

#ifndef PLATFORM_OSX
    // Setup socket to receive response(s)
    if ((sockfd = socket(PF_PACKET, SOCK_RAW, htons(0x0805))) == -1) {
        LogErr(VB_CHANNELOUT, "Unable to open receive socket\n");
        return;
    }

    strncpy(ifopts.ifr_name, m_ifName.c_str(), IFNAMSIZ - 1);
    ioctl(sockfd, SIOCGIFFLAGS, &ifopts);
    ifopts.ifr_flags |= IFF_PROMISC;
    ioctl(sockfd, SIOCSIFFLAGS, &ifopts);

    // Wait up to 300ms for responses, shouldn't take anywhere near this long
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 300000;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) == -1) {
        LogErr(VB_CHANNELOUT, "Unable to set socket receive timeout\n");
        close(sockfd);
        return;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
        LogErr(VB_CHANNELOUT, "Unable to set receive socket options\n");
        close(sockfd);
        return;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, m_ifName.c_str(), IFNAMSIZ - 1) == -1) {
        LogErr(VB_CHANNELOUT, "Unable to bind receive socket\n");
        close(sockfd);
        return;
    }
#else
    sockfd = bindBPFSocket(m_ifName);
#endif

    LogInfo(VB_CHANNELOUT, "Searching for ColorLight receivers\n");

    unsigned char receiveBuffer[CL_RESP_PACKET_SIZE];
    unsigned char* data = receiveBuffer + CL_PACKET_DATA_OFFSET;
    int recvBytes = 0;
    int r = 0;
    bool tryNextReceiver = true;
    bool foundReceiver = false;

    memset(receiveBuffer, 0, CL_RESP_PACKET_SIZE);

    int retryCount = 2;
    while (tryNextReceiver && r < 32) {
        foundReceiver = false;
        discoverBuffer[16] = r;
        sendmmsg(m_fd, msgs, 1, MSG_DONTWAIT);
        if (retryCount == 0) {
            sendmmsg(m_fd, msgs, 1, MSG_DONTWAIT);
            sendmmsg(m_fd, msgs, 1, MSG_DONTWAIT);
        }

        // Should only get one packet from a receiver, but consume all just in case
        while ((recvBytes = recvfrom(sockfd, receiveBuffer, CL_RESP_PACKET_SIZE, 0, NULL, NULL)) > 0) {
            if (recvBytes > 1000) {
                // Shouldn't be receiving anything, but ignore anything other than receiver replies
                if ((receiveBuffer[6] != 0x11) ||
                    (receiveBuffer[7] != 0x22) ||
                    (receiveBuffer[8] != 0x33) ||
                    (receiveBuffer[9] != 0x44) ||
                    (receiveBuffer[10] != 0x55) ||
                    (receiveBuffer[11] != 0x66) ||
                    (receiveBuffer[CL_PACKET_TYPE_OFFSET] != CL_RESP_PACKET_TYPE)) {
                    continue;
                }

                if (WillLog(LOG_DEBUG, VB_CHANNELOUT)) {
                    LogDebug(VB_CHANNELOUT, "Response received from receiver id %d\n", r);
                    HexDump("DiscoverResponse", receiveBuffer, 256, VB_CHANNELOUT);
                }

                foundReceiver = true;
                ColorLightReceiver receiver;

                receiver.id = data[85];
                receiver.majorFirmwareVersion = data[2];
                receiver.minorFirmwareVersion = data[3];

                receiver.width = (data[21] << 8) | data[22];
                receiver.height = (data[23] << 8) | data[24];
                receiver.packets = (data[38] << 24) | (data[39] << 16) | (data[40] << 8) | data[41];
                receiver.uptime = (data[46] << 24) | (data[47] << 16) | (data[48] << 8) | data[49];

                LogInfo(VB_CHANNELOUT, "Found Receiver #%d:\n", receiver.id);
                LogInfo(VB_CHANNELOUT, "- Firmware Version    : v%d.%d\n",
                        receiver.majorFirmwareVersion, receiver.minorFirmwareVersion);
                LogInfo(VB_CHANNELOUT, "- Cabinet Size (WxH)  : %dx%d\n", receiver.width, receiver.height);
                // LogInfo(VB_CHANNELOUT, "- Cabinet Offset (X,Y): %d,%d\n", receiver.xOffset, receiver.yOffset);
                LogInfo(VB_CHANNELOUT, "- Uptime              : %ums\n", receiver.uptime);
                LogInfo(VB_CHANNELOUT, "- Packets             : %u\n", receiver.packets);

                if (receiver.majorFirmwareVersion > m_highestFirmwareVersion) {
                    m_highestFirmwareVersion = receiver.majorFirmwareVersion;
                }

                m_receivers.resize(r + 1);
                m_receivers[r] = receiver;
            }
        }
        if (!foundReceiver) {
            if (r == 0 && retryCount > 0) {
                r--;
                retryCount--;
            } else {
                tryNextReceiver = false;
            }
        }
        r++;
    }

    if (m_receivers.size()) {
        LogInfo(VB_CHANNELOUT, "Found %d ColorLight receiver(s)\n", m_receivers.size());
    } else {
        std::string warning = "ColorLight: No receiver cards were detected on interface " + m_ifName;
        WarningHolder::AddWarning(9, warning);
        LogWarn(VB_CHANNELOUT, "WARNING: %s\n", warning.c_str());
        m_warnings.push_front(warning);
    }

    close(sockfd);
}
