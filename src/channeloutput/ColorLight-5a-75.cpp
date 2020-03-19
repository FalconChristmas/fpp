/*
 *   ColorLight 5a-75 Channel Output driver for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *   Packet Format: (info based on mplayer ColorLight 5a-75 video output patch)
 *
 *   0x0101 Packet: (send first)
 *   	- Data Length:     98
 *   	- Destination MAC: 11:22:33:44:55:66
 *   	- Source MAC:      22:22:33:44:55:66
 *   	- Ether Type:      0x0101 (have also seen 0x0100, 0x0104, 0x0107.
 *   	- Data[0-end]:     0x00
 *
 *   0x0AFF Packet: (send second, but not at all in some captures)
 *   	- Data Length:     63
 *   	- Destination MAC: 11:22:33:44:55:66
 *   	- Source MAC:      22:22:33:44:55:66
 *   	- Ether Type:      0x0AFF 
 *   	- Data[0]:         0xFF
 *   	- Data[1]:         0xFF
 *   	- Data[2]:         0xFF
 *   	- Data[3-end]:     0x00
 *
 *   Row data packets: (send one packet for each row of display)
 *      - Data Length:     (Row_Width * 3) + 7
 *   	- Destination MAC: 11:22:33:44:55:66
 *   	- Source MAC:      22:22:33:44:55:66
 *   	- Ether Type:      0x5500 + MSB of Row Number
 *   	                     0x5500 for rows 0-255
 *   	                     0x5501 for rows 256-511
 *   	- Data[0]:         Row Number LSB
 *   	- Data[1]:         MSB of pixel offset for this packet
 *   	- Data[2]:         LSB of pixel offset for this packet
 *   	- Data[3]:         MSB of pixel count in packet
 *   	- Data[4]:         LSB of pixel count in packet
 *   	- Data[5]:         0x08 - ?? unsure what this is
 *   	- Data[6]:         0x80 - ?? unsure what this is
 *   	- Data[7-end]:     RGB order pixel data
 *
 *   Sample data packets seen in captures:
 *           0  1  2  3  4  5  6
 *     55 00 00 00 00 01 F1 00 00 (first 497 pixels on a 512 wide display)
 *     55 00 00 01 F1 00 0F 00 00 (last 15 pixels on a 512 wide display)
 *     55 00 00 00 00 01 20 08 88 (288 pixel wide display)
 *     55 00 00 00 00 00 80 08 88 (128 pixel wide display)
 *
 */

#include <arpa/inet.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cmath>

#include "common.h"
#include "Warnings.h"
#include "ColorLight-5a-75.h"
#include "log.h"
#include "settings.h"


extern "C" {
    ColorLight5a75Output *createOutputColorLight5a75(unsigned int startChannel,
                                           unsigned int channelCount) {
        return new ColorLight5a75Output(startChannel, channelCount);
    }
}


/*
 *
 */
ColorLight5a75Output::ColorLight5a75Output(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_width(0),
	m_height(0),
	m_colorOrder(kColorOrderRGB),
	m_fd(-1),
	m_rowSize(0),
	m_panelWidth(0),
	m_panelHeight(0),
	m_panels(0),
	m_rows(0),
	m_outputs(0),
	m_longestChain(0),
	m_invertedData(0),
	m_matrix(NULL),
	m_panelMatrix(NULL),
	m_slowCount(0),
	m_flippedLayout(0)
{
	LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::ColorLight5a75Output(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
ColorLight5a75Output::~ColorLight5a75Output()
{
	LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::~ColorLight5a75Output()\n");

    for (int i = 0; i < m_iovecs.size(); i++) {
        free(m_iovecs[i].iov_base);

        if (i >= 4)
            i++; // first 4 are header+data, only headers for the rest
    }

	if (m_fd >= 0)
		close(m_fd);

	if (m_outputFrame)
		delete [] m_outputFrame;
}

/*
 *
 */
int ColorLight5a75Output::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::Init(JSON)\n");

	m_panelWidth  = config["panelWidth"].asInt();
	m_panelHeight = config["panelHeight"].asInt();

	if (!m_panelWidth)
		m_panelWidth = 32;

	if (!m_panelHeight)
		m_panelHeight = 16;

	m_invertedData = config["invertedData"].asInt();

	m_colorOrder = ColorOrderFromString(config["colorOrder"].asString());

	m_panelMatrix =
		new PanelMatrix(m_panelWidth, m_panelHeight, m_invertedData);

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
		const char *o = p["orientation"].asString().c_str();

		if (o && *o)
			orientation = o[0];

		if (m_flippedLayout) {
			switch (orientation)
			{
				case 'N': orientation = 'U';
						  break;
				case 'U': orientation = 'N';
						  break;
				case 'R': orientation = 'L';
						  break;
				case 'L': orientation = 'R';
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

	m_width  = m_panelMatrix->Width();
	m_height = m_panelMatrix->Height();

	m_channelCount = m_width * m_height * 3;

	m_outputFrame = new char[m_outputs * m_longestChain * m_panelHeight * m_panelWidth * 3];

	m_matrix = new Matrix(m_startChannel, m_width, m_height);

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
    if ((bind(m_fd, (struct sockaddr *)&m_sock_addr, sizeof(m_sock_addr))) == -1) {
        LogDebug(VB_CHANNELOUT, "bind() failed\n");
        return 0;
    }

    int packetCount = 2 + (m_rows * (((int)(m_rowSize-1) / CL5A75_MAX_CHANNELS_PER_PACKET) + 1));
    m_msgs.resize(packetCount);
    m_iovecs.resize(packetCount * 2);

    unsigned int   p = 0;
    unsigned char *header = nullptr;
    unsigned char *data = nullptr;

    // First Init packet
    header = (unsigned char *)malloc(sizeof(struct ether_header));
    memset(header, 0, sizeof(struct ether_header));
    m_eh = (struct ether_header *)header;
    m_eh->ether_type = htons(0x0101);
    SetHostMACs(header);
    m_iovecs[p * 2].iov_base = header;
    m_iovecs[p * 2].iov_len = sizeof(struct ether_header);

    data = (unsigned char *)malloc(CL5A75_0101_PACKET_DATA_LEN);
    memset(data, 0, CL5A75_0101_PACKET_DATA_LEN);
    m_iovecs[p * 2 + 1].iov_base = data;
    m_iovecs[p * 2 + 1].iov_len = CL5A75_0101_PACKET_DATA_LEN;
    p++;

    // Second Init packet
    header = (unsigned char *)malloc(sizeof(struct ether_header));
    memset(header, 0, sizeof(struct ether_header));
    m_eh = (struct ether_header *)header;
    m_eh->ether_type = htons(0x0AFF);
    SetHostMACs(header);
    m_iovecs[p * 2].iov_base = header;
    m_iovecs[p * 2].iov_len = sizeof(struct ether_header);

    data = (unsigned char *)malloc(CL5A75_0AFF_PACKET_DATA_LEN);
    memset(data, 0, CL5A75_0AFF_PACKET_DATA_LEN);
    data[0] = 0xff;
    data[1] = 0xff;
    data[2] = 0xff;
    m_iovecs[p * 2 + 1].iov_base = data;
    m_iovecs[p * 2 + 1].iov_len = CL5A75_0AFF_PACKET_DATA_LEN;
    p++;

    char *rowPtr = (char *)m_outputFrame;
    unsigned int dSize = 0;
    unsigned int part = 0;
    unsigned int hSize = sizeof(struct ether_header) + CL5A75_HEADER_LEN;
    unsigned int offset = 0;
    unsigned int bytesInPacket = 0;
    unsigned int pixelOffset = 0;
    unsigned int pixelsInPacket = 0;

    for (int r = 0; r < m_rows; r++) {
        part = 0;
        offset = 0;

        while (offset < m_rowSize) {
            header = (unsigned char *)malloc(hSize);
            memset(header, 0, hSize);
            m_eh = (struct ether_header *)header;

            m_eh->ether_type = htons(0x5500 + (int)(r / 256));
            SetHostMACs(header);

            data = header + sizeof(struct ether_header);
            data[0] = r % 256;

            if ((offset + CL5A75_MAX_CHANNELS_PER_PACKET) > m_rowSize)
                bytesInPacket = m_rowSize - offset;
            else
                bytesInPacket = CL5A75_MAX_CHANNELS_PER_PACKET;

            pixelOffset = offset / 3;
            pixelsInPacket = bytesInPacket / 3;

            data[1] = pixelOffset >> 8;      // Pixel Offset MSB
            data[2] = pixelOffset & 0xFF;    // Pixel Offset LSB
            data[3] = pixelsInPacket >> 8;   // Pixels In Packet MSB
            data[4] = pixelsInPacket & 0xFF; // Pixels In Packet LSB

            data[5] = 0x08; // ?? still not sure what this value is
            data[6] = 0x80; // ?? still not sure what this value is

            m_iovecs[p * 2].iov_base = header;
            m_iovecs[p * 2].iov_len = hSize;
            m_iovecs[p * 2 + 1].iov_base = rowPtr + offset;
            m_iovecs[p * 2 + 1].iov_len = bytesInPacket;

            offset += bytesInPacket;
            part++;
            p++;
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

    return ChannelOutputBase::Init(config);
}

/*
 *
 */
int ColorLight5a75Output::Close(void)
{
	LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::Close()\n");

	return ChannelOutputBase::Close();
}

void ColorLight5a75Output::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}
/*
 *
 */
void ColorLight5a75Output::PrepData(unsigned char *channelData)
{
	m_matrix->OverlaySubMatrices(channelData);

	unsigned char *r = NULL;
	unsigned char *g = NULL;
	unsigned char *b = NULL;
	unsigned char *s = NULL;
	unsigned char *dst = NULL;
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

				dst = (unsigned char*)(m_outputFrame + (((((output * m_panelHeight) + y) * m_panelWidth * m_longestChain) + px) * 3));

				for (int x = 0; x < pw3; x += 3)
				{
					*(dst++) = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw + x]]];
					*(dst++) = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw + x + 1]]];
					*(dst++) = m_gammaCurve[channelData[m_panelMatrix->m_panels[panel].pixelMap[yw + x + 2]]];

					px++;
				}
			}
		}
	}
}

/*
 *
 */
int ColorLight5a75Output::SendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "ColorLight5a75Output::SendData(%p)\n", channelData);

    long long startTime = GetTimeMS();
    struct mmsghdr *msgs = &m_msgs[0];
    int msgCount = m_msgs.size();
    if (msgCount == 0)
        return 0;

    errno = 0;
    int oc = sendmmsg(m_fd, msgs, msgCount, MSG_DONTWAIT);
    int outputCount = 0;
    if (oc > 0) {
        outputCount = oc;
    }

    int errCount = 0;
    while ((outputCount != msgCount) && (errCount < 5)) {
        errCount++;
        LogErr(VB_CHANNELOUT, "sendmmsg() failed for ColorLight output (Socket: %d   output count: %d/%d) with error: %d   %s\n",
            m_fd, outputCount, msgCount, errno, strerror(errno));

        errno = 0;
        oc = sendmmsg(m_fd, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
        if (oc > 0) {
            outputCount += oc;
        }
    }

    long long endTime = GetTimeMS();
    long long totalTime = endTime - startTime;
    if (totalTime > 25) {
        m_slowCount++;
        LogDebug(VB_CHANNELOUT, "Long time to send frame to colorlight: %d ms\n", ((int)totalTime));
        if (m_slowCount > 3) {
            LogWarn(VB_CHANNELOUT, "Repeated frames taking more than 25ms to send to ColorLight");
            WarningHolder::AddWarningTimeout("Repeated frames taking more than 25ms to send to ColorLight", 30);
        }
    } else {
        m_slowCount = 0;
    }

	return m_channelCount;
}

/*
 *
 */
void ColorLight5a75Output::DumpConfig(void)
{
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

	ChannelOutputBase::DumpConfig();
}

/*
 *
 */
void ColorLight5a75Output::SetHostMACs(void *ptr)
{
	struct ether_header *eh = (struct ether_header *)ptr;

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
