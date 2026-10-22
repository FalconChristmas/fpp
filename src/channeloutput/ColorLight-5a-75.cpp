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

#include "fpp-json.h"

#ifndef PLATFORM_OSX
#include <linux/if_packet.h>
#include <linux/sockios.h>
#include <netinet/ether.h>
#else
#include <net/bpf.h>
#endif

#include "../SysSocket.h"
#include <arpa/inet.h>
#include <unistd.h>
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
#include "GammaLUT.h"
#include "overlays/PixelOverlay.h"

#include "Plugin.h"

static constexpr const char* CL_DROPPED_FRAMES_WARNING = "ColorLight: The network is not keeping up, frames are being dropped";

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
        LogErr(VB_CHANNELOUT, "Error opening bpf file: %s\n", FPPstrerror(errno));
        return -1;
    }

    struct ifreq bound_if;
    memset(&bound_if, 0, sizeof(bound_if));
    // ifr_name is IFNAMSIZ bytes; callers pass the validated m_ifName, but
    // guard the copy itself so a future caller cannot reintroduce the overflow.
    if (iface.size() >= sizeof(bound_if.ifr_name)) {
        LogErr(VB_CHANNELOUT, "Interface name too long for bind (max %d chars): %s\n",
               IFNAMSIZ - 1, iface.c_str());
        close(m_fd);
        return -1;
    }
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
    m_frameCharge(0),
    m_sendBufferSize(0),
    m_frameSent(false),
    m_droppedFrames(0),
    m_droppedLogTime(0),
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

        // a refused panel (out of range) must not count toward the outputs or chains below
        if (!m_panelMatrix->AddPanel(p["outputNumber"].asInt(),
                                     p["panelNumber"].asInt(), orientation,
                                     p["xOffset"].asInt(), p["yOffset"].asInt(),
                                     ColorOrderFromString(p["colorOrder"].asString()))) {
            continue;
        }

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
    BuildRemap();

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

    GammaLUT::Build(m_gammaCurve, GammaLUT::ParseConfig(config, 1.0f));

    if (config.isMember("interface"))
        m_ifName = config["interface"].asString();
    else
        m_ifName = "eth1";

    // ifr_name is IFNAMSIZ (16) bytes at every use below; an overlong name
    // would overflow the stack copies. It never worked (ioctl/sysfs lookups
    // fail on it), so fail init with an error like other bad config here.
    if (m_ifName.size() >= IFNAMSIZ) {
        LogErr(VB_CHANNELOUT, "Error ColorLight: Configured interface name too long (max %d chars): %s\n",
               IFNAMSIZ - 1, m_ifName.c_str());
        return 0;
    }

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
        LogErr(VB_CHANNELOUT, "Error ColorLight: Configured interface %s does not have link %s\n", m_ifName.c_str(), FPPstrerror(errno));
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
        LogErr(VB_CHANNELOUT, "Error ColorLight: Configured interface %s is not 1000Mbps Capable: %s\n", m_ifName.c_str(), FPPstrerror(errno));
        WarningHolder::AddWarning(8, "ColorLight: Configured interface " + m_ifName + " is not 1000Mbps Capable");
        if (m_colorlightDisable) {
            return 0;
        }
    } else {
        WarningHolder::RemoveWarning(8, "ColorLight: Configured interface " + m_ifName + " is not 1000Mbps Capable");
    }

    // Open our raw socket
    if ((m_fd = OpenSocket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
        LogErr(VB_CHANNELOUT, "Error creating raw socket: %s\n", FPPstrerror(errno));
        return 0;
    }

    // Get the output interface ID
    memset(&m_if_idx, 0, sizeof(struct ifreq));
    strcpy(m_if_idx.ifr_name, m_ifName.c_str());
    if (ioctl(m_fd, SIOCGIFINDEX, &m_if_idx) < 0) {
        LogErr(VB_CHANNELOUT, "Error getting index of %s interface: %s\n",
               m_ifName.c_str(), FPPstrerror(errno));
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

#ifndef PLATFORM_OSX
    // Size the send buffer for two whole frames, so a frame normally queues
    // in one go and FrameFits() can let one frame wait behind another before
    // dropping.  The kernel charges each packet its skb overhead on top of
    // the payload, measured at ~1.3KB on a 32 bit Beagle kernel; 2048 bytes
    // leaves room for 64 bit kernels.  An estimate that comes up short only
    // means the odd frame is found not to fit while it is being queued,
    // and is dropped then instead.  The kernel doubles what is asked for,
    // so asking for one frame gets two.
    m_frameCharge = 0;
    for (auto* msgs : { &m_msgs, &m_syncMsgs }) {
        for (auto& msg : *msgs) {
            for (size_t i = 0; i < msg.msg_hdr.msg_iovlen; i++)
                m_frameCharge += msg.msg_hdr.msg_iov[i].iov_len;
            m_frameCharge += 2048;
        }
    }
    int sendBuffer = m_frameCharge;
    if (setsockopt(m_fd, SOL_SOCKET, SO_SNDBUFFORCE, &sendBuffer, sizeof(sendBuffer)) == -1)
        setsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, &sendBuffer, sizeof(sendBuffer));
    socklen_t len = sizeof(m_sendBufferSize);
    if (getsockopt(m_fd, SOL_SOCKET, SO_SNDBUF, &m_sendBufferSize, &len) == -1)
        m_sendBufferSize = 0;
    if (m_sendBufferSize < m_frameCharge) {
        // Without room for a whole frame, FrameFits() would drop them all
        LogWarn(VB_CHANNELOUT, "ColorLight: Send buffer is %d bytes, a frame needs %d\n", m_sendBufferSize, m_frameCharge);
        m_sendBufferSize = 0;
    }
#endif

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
        WarningHolder::RemoveWarning(9, m);
    }
    ClearSendErrorWarning();
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

            m_panelMatrix->m_panels[panel].drawTestPattern(channelData + m_startChannel, cycleNum, percentOfCycle, testType);
        }
    }
}

/*
 * Queue packets without ever waiting for the NIC: this runs on the channel
 * output thread, and every other output on the box waits behind it.
 * PrepData() has already checked that the whole frame fits in the socket's
 * send buffer, so a short send means the network fell behind after all, or
 * the interface is gone.  Returns whether every packet was queued.
 */
bool ColorLight5a75Output::SendMessages(std::vector<struct mmsghdr>& msgsToSend) {
    struct mmsghdr* msgs = msgsToSend.data();
    int msgCount = msgsToSend.size();
    int sent = 0;
    int oc = 0;

    while (sent < msgCount) {
        errno = 0;
        oc = SendMessagesHelper(&msgs[sent], msgCount - sent);
        if (oc <= 0)
            break;
        sent += oc;
    }

    if (sent == msgCount) {
        ClearSendErrorWarning();
        return true;
    }

    int err = errno;
    if (oc < 0 && err != EAGAIN && err != EWOULDBLOCK && err != ENOBUFS) {
        RaiseSendErrorWarning(err);
    } else {
        NoteDroppedFrame();
    }
    return false;
}

// Raised when sends fail outright (interface down, USB NIC unplugged), and
// cleared by the first frame that is sent whole.
void ColorLight5a75Output::RaiseSendErrorWarning(int err) {
    std::string warning = "ColorLight: Unable to send on interface " + m_ifName + ": " + FPPstrerror(err);
    if (warning == m_sendErrorWarning)
        return;

    ClearSendErrorWarning();
    LogErr(VB_CHANNELOUT, "%s\n", warning.c_str());
    WarningHolder::AddWarning(7, warning);
    m_sendErrorWarning = warning;
}

void ColorLight5a75Output::ClearSendErrorWarning() {
    if (m_sendErrorWarning.empty())
        return;

    LogInfo(VB_CHANNELOUT, "ColorLight: Sending on interface %s again\n", m_ifName.c_str());
    WarningHolder::RemoveWarning(7, m_sendErrorWarning);
    m_sendErrorWarning.clear();
}

// A frame the network could not take is dropped whole.  The warning times
// out on its own once frames stop being dropped; the log gets one summary
// per interval rather than a line per frame.
void ColorLight5a75Output::NoteDroppedFrame() {
    WarningHolder::AddWarningTimeout(30, 21, CL_DROPPED_FRAMES_WARNING);

    m_droppedFrames++;

    long long now = GetMonotonicTimeMS();
    if (now - m_droppedLogTime >= 10000) {
        LogWarn(VB_CHANNELOUT, "ColorLight: Interface %s is not keeping up, %d frame(s) dropped since the last report\n",
                m_ifName.c_str(), m_droppedFrames);
        m_droppedFrames = 0;
        m_droppedLogTime = now;
    }
}

// Whether a whole frame fits in what is left of the socket's send buffer.
// If the previous frames have not drained by now, the network (or the USB
// NIC behind it) cannot keep up, and queueing more only adds latency.
bool ColorLight5a75Output::FrameFits() {
#ifndef PLATFORM_OSX
    int queued = 0;
    if (m_sendBufferSize && ioctl(m_fd, SIOCOUTQ, &queued) == 0)
        return queued + m_frameCharge <= m_sendBufferSize;
#endif
    return true;
}

/*
 *
 */
void ColorLight5a75Output::PrepData(unsigned char* channelData) {
    m_matrix->OverlaySubMatrices(channelData);

    m_frameSent = false;
    if (!FrameFits()) {
        NoteDroppedFrame();
        return;
    }

    // Every pointer is a local __restrict copy: the stores are through a
    // char pointer, which may alias anything, so reading the maps through
    // members would reload them for every byte.
    const unsigned char* __restrict src = channelData + m_startChannel; // FIXME, this function gets offset 0
    unsigned char* __restrict frame = (unsigned char*)m_outputFrame.get();
    const uint8_t* __restrict gamma = m_gammaCurve;
    const uint32_t* __restrict pixel = m_remapSrc.data();

    for (const RemapRun& run : m_remapRuns) {
        unsigned char* __restrict dst = frame + run.dst;
        const uint32_t o0 = run.order[0];
        const uint32_t o1 = run.order[1];
        const uint32_t o2 = run.order[2];

        for (uint32_t x = 0; x < run.count; x++, dst += 3) {
            const unsigned char* s = src + *(pixel++);
            dst[0] = gamma[s[o0]];
            dst[1] = gamma[s[o1]];
            dst[2] = gamma[s[o2]];
        }
    }

    m_frameSent = SendMessages(m_msgs);
}

/*
 * Flatten the panel maps into the order PrepData() writes the output frame.
 * A map with one int per byte is three times the reads of one per pixel, and
 * on a single-core Beagle it does not fit in cache, so the color order is
 * pulled out into per-run offsets.  PanelMatrix gives every pixel a single
 * color order, so a run is normally a whole panel row; a pixel whose offsets
 * differ from its neighbor's just starts a new run.
 */
void ColorLight5a75Output::BuildRemap(void) {
    m_remapRuns.clear();
    m_remapSrc.clear();

    for (int output = 0; output < m_outputs; output++) {
        for (int panel : m_panelMatrix->m_outputPanels[output]) {
            const LEDPanel& p = m_panelMatrix->m_panels[panel];
            int chain = (m_longestChain - 1) - p.chain;

            if (m_flippedLayout)
                chain = p.chain;

            for (int y = 0; y < m_panelHeight; y++) {
                uint32_t dst = (((((output * m_panelHeight) + y) * m_panelWidth * m_longestChain) + (chain * m_panelWidth)) * 3);
                const int* map = &p.pixelMap[y * m_panelWidth * 3];
                RemapRun* run = nullptr;

                for (int x = 0; x < m_panelWidth; x++, map += 3, dst += 3) {
                    uint32_t base = std::min(map[0], std::min(map[1], map[2]));
                    uint32_t order[3] = { map[0] - base, map[1] - base, map[2] - base };

                    if (!run || memcmp(run->order, order, sizeof(order))) {
                        m_remapRuns.push_back({ dst, 0, { order[0], order[1], order[2] } });
                        run = &m_remapRuns.back();
                    }
                    run->count++;
                    m_remapSrc.push_back(base);
                }
            }
        }
    }

    LogDebug(VB_CHANNELOUT, "ColorLight remap: %d pixels in %d runs\n",
             (int)m_remapSrc.size(), (int)m_remapRuns.size());
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

    // Latching a frame whose rows did not all go out would show part of it
    // over the last one, so the receiver keeps the last whole frame instead.
    if (m_frameSent)
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
    if ((sockfd = OpenSocket(PF_PACKET, SOCK_RAW, htons(0x0805))) == -1) {
        LogErr(VB_CHANNELOUT, "Unable to open receive socket\n");
        return;
    }

    memset(&ifopts, 0, sizeof(ifopts));
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
