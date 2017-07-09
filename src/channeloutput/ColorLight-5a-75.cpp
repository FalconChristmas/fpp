/*
 *   ColorLight 5a-75 Channel Output driver for Falcon Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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
 *   	- Source MAC:      22:22:33:44:55:66
 *   	- Destination MAC: 11:22:33:44:55:66
 *   	- Ether Type:      0x0101
 *
 *   0x0AFF Packet: (send second)
 *   	- Data Length:     63
 *   	- Source MAC:      22:22:33:44:55:66
 *   	- Destination MAC: 11:22:33:44:55:66
 *   	- Ether Type:      0x0AFF
 *   	- Data[0]:         0xFF
 *   	- Data[1]:         0xFF
 *   	- Data[2]:         0xFF
 *
 *   Row data packets: (send one packet for each row of display)
 *      - Data Length:     (Row_Width * 3) + 7
 *   	- Source MAC:      22:22:33:44:55:66
 *   	- Destination MAC: 11:22:33:44:55:66
 *   	- Ether Type:      0x5500
 *   	- Data[0]:         Row Number (0-255)
 *   	- Data[1]:         0x00
 *   	- Data[2]:         0x00
 *   	- Data[3]:         0x00
 *   	- Data[4]:         0x80
 *   	- Data[5]:         0x08
 *   	- Data[6]:         0x80
 *   	- Data[7-end]:     RGB order pixel data
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

#include "common.h"
#include "ColorLight-5a-75.h"
#include "log.h"


/*
 *
 */
ColorLight5a75Output::ColorLight5a75Output(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_width(0),
	m_height(0),
	m_colorOrder(kColorOrderRGB),
	m_fd(-1),
	m_buffer_0101(NULL),
	m_buffer_0101_len(0),
	m_buffer_0AFF(NULL),
	m_buffer_0AFF_len(0),
	m_data(NULL),
	m_rowSize(0),
	m_pktSize(0),
	m_panelWidth(0),
	m_panelHeight(0),
	m_panels(0),
	m_rows(0),
	m_outputs(0),
	m_longestChain(0),
	m_invertedData(0),
	m_matrix(NULL),
	m_panelMatrix(NULL)
{
	LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::ColorLight5a75Output(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = 256 * 256 * 3;
}

/*
 *
 */
ColorLight5a75Output::~ColorLight5a75Output()
{
	LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::~ColorLight5a75Output()\n");

	if (m_fd >= 0)
		close(m_fd);

	if (m_outputFrame)
		delete [] m_outputFrame;

	if (m_buffer_0101)
		free(m_buffer_0101);

	if (m_buffer_0AFF)
		free(m_buffer_0AFF);
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
		new PanelMatrix(m_panelWidth, m_panelHeight, 3, m_invertedData);

	if (!m_panelMatrix)
	{
		LogErr(VB_CHANNELOUT, "Unable to create PanelMatrix\n");
		return 0;
	}

	for (int i = 0; i < config["panels"].size(); i++)
	{
		Json::Value p = config["panels"][i];
		char orientation = 'N';
		const char *o = p["orientation"].asString().c_str();

		if (o && *o)
			orientation = o[0];

		// FIXME, is the ColorLight receiver flipping the panels 180 degrees?
		switch (orientation)
		{
			case 'N':	orientation = 'U';
						break;
			case 'U':	orientation = 'N';
						break;
			case 'R':	orientation = 'L';
						break;
			case 'L':	orientation = 'R';
						break;
		}

		m_panelMatrix->AddPanel(p["outputNumber"].asInt(),
			p["panelNumber"].asInt(), orientation,
			p["xOffset"].asInt(), p["yOffset"].asInt());

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

	if (config.isMember("subMatrices"))
	{
		for (int i = 0; i < config["subMatrices"].size(); i++)
		{
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

	if (config.isMember("interface"))
		m_ifName = config["interface"].asString();
	else
		m_ifName = "eth1";


	////////////////////////////
	// Setup 0x0101 packet data
	m_buffer_0101_len = sizeof(struct ether_header) + 98;
	m_buffer_0101 = (char *)calloc(m_buffer_0101_len, 1);
	if (!m_buffer_0101)
	{
		LogErr(VB_CHANNELOUT, "Error allocating m_buffer_0101\n");
		return 0;
	}

	memset(m_buffer_0101, 0, m_buffer_0101_len);
	SetHostMACs(m_buffer_0101);
	m_eh = (struct ether_header *)m_buffer_0101;
	m_eh->ether_type = htons(0x0101);

	////////////////////////////
	// Setup 0x0AFF packet data
	m_buffer_0AFF_len = sizeof(struct ether_header) + 63;
	m_buffer_0AFF = (char *)calloc(m_buffer_0AFF_len, 1);
	if (!m_buffer_0AFF)
	{
		LogErr(VB_CHANNELOUT, "Error allocating m_buffer_0AFF\n");
		return 0;
	}

	memset(m_buffer_0AFF, 0, m_buffer_0AFF_len);
	SetHostMACs(m_buffer_0AFF);
	m_eh = (struct ether_header *)m_buffer_0AFF;
	m_data = m_buffer_0AFF + sizeof(struct ether_header);
	m_eh->ether_type = htons(0x0AFF);
	m_data[0] = 0xff;
	m_data[1] = 0xff;
	m_data[2] = 0xff;

	////////////////////////////
	// Set main data packet
	memset(m_buffer, 0, CL5A75_BUFFER_SIZE);
	m_eh = (struct ether_header *)m_buffer;
	m_data = m_buffer + sizeof(struct ether_header);
	m_eh->ether_type = htons(0x5500);
	SetHostMACs(m_buffer);

	m_rowSize = m_longestChain * m_panelWidth * 3;
	m_pktSize = sizeof(struct ether_header) + 7 + m_rowSize;


	// Open our raw socket
	if ((m_fd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1)
	{
		LogErr(VB_CHANNELOUT, "Error creating raw socket: %s\n", strerror(errno));
		return 0;
	}

	memset(&m_if_idx, 0, sizeof(struct ifreq));
	strcpy(m_if_idx.ifr_name, m_ifName.c_str());
	if (ioctl(m_fd, SIOCGIFINDEX, &m_if_idx) < 0)
	{
		LogErr(VB_CHANNELOUT, "Error getting index of %s inteface: %s\n",
			m_ifName.c_str(), strerror(errno));
		return 0;
	}

	m_sock_addr.sll_ifindex = m_if_idx.ifr_ifindex;
	m_sock_addr.sll_halen = ETH_ALEN;
	memcpy(m_sock_addr.sll_addr, m_eh->ether_dhost, 6);

	m_data[0] = 0x00; // row #
	m_data[1] = 0x00; // ??
	m_data[2] = 0x00; // ??
	m_data[3] = 0x01; // ??
	m_data[4] = 0x00; // ??, mplayer code had 0x00 here
	m_data[5] = 0x08; // ??
	m_data[6] = 0x88; // ??

	m_rowData = m_data + 7;

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

/*
 *
 */
void ColorLight5a75Output::PrepData(unsigned char *channelData)
{
	unsigned char *r = NULL;
	unsigned char *g = NULL;
	unsigned char *b = NULL;
	unsigned char *s = NULL;
	unsigned char *dst = NULL;
	int pw3 = m_panelWidth * 3;

	channelData += m_startChannel; // FIXME, this function gets offset 0

	for (int output = 0; output < m_outputs; output++)
	{
		int panelsOnOutput = m_panelMatrix->m_outputPanels[output].size();

		for (int i = 0; i < panelsOnOutput; i++)
		{
			int panel = m_panelMatrix->m_outputPanels[output][i];
			int chain = (panelsOnOutput - 1) - m_panelMatrix->m_panels[panel].chain;
			chain = m_panelMatrix->m_panels[panel].chain;

			for (int y = 0; y < m_panelHeight; y++)
			{
				int px = chain * m_panelWidth;
				int yw = y * m_panelWidth * 3;

				dst = (unsigned char*)(m_outputFrame + (((((output * m_panelHeight) + y) * m_panelWidth * m_longestChain) + px) * 3));

				for (int x = 0; x < pw3; x += 3)
				{
					s = channelData + m_panelMatrix->m_panels[panel].pixelMap[yw + x];

					switch (m_colorOrder)
					{
						default:
						case kColorOrderRGB:	r = s;
												g = s + 1;
												b = s + 2;
												break;

						case kColorOrderRBG:	r = s;
												b = s + 1;
												g = s + 2;
												break;

						case kColorOrderGRB:	g = s;
												r = s + 1;
												b = s + 2;
												break;

						case kColorOrderGBR:	g = s;
												b = s + 1;
												r = s + 2;
												break;

						case kColorOrderBRG:	b = s;
												r = s + 1;
												g = s + 2;
												break;

						case kColorOrderBGR:	b = s;
												g = s + 1;
												r = s + 2;
												break;
					}

					*(dst++) = *r;
					*(dst++) = *g;
					*(dst++) = *b;

					px++;
				}
			}
		}
	}
}

/*
 *
 */
int ColorLight5a75Output::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "ColorLight5a75Output::RawSendData(%p)\n", channelData);

	if (sendto(m_fd, m_buffer_0101, m_buffer_0101_len, 0, (struct sockaddr*)&m_sock_addr, sizeof(struct sockaddr_ll)) < 0)
	{
		LogErr(VB_CHANNELOUT, "Error sending 0x0101 packet: %s\n", strerror(errno));
		return 0;
	}

	if (sendto(m_fd, m_buffer_0AFF, m_buffer_0AFF_len, 0, (struct sockaddr*)&m_sock_addr, sizeof(struct sockaddr_ll)) < 0)
	{
		LogErr(VB_CHANNELOUT, "Error sending 0x0AFF packet: %s\n", strerror(errno));
		return 0;
	}

	char *rowPtr = (char *)m_outputFrame;
	int row = 0;
	for (row = 0; row < m_rows; row++)
	{
		m_data[0] = row;
		memcpy(m_rowData, rowPtr, m_rowSize);

		if (sendto(m_fd, m_buffer, m_pktSize, 0, (struct sockaddr*)&m_sock_addr, sizeof(struct sockaddr_ll)) < 0)
		{
			LogErr(VB_CHANNELOUT, "Error sending row data packet: %s\n", strerror(errno));
			return 0;
		}

		rowPtr += m_rowSize;
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
	LogDebug(VB_CHANNELOUT, "    Data Pkt Size  : %d\n", m_pktSize);
	LogDebug(VB_CHANNELOUT, "    Outputs        : %d\n", m_outputs);
	LogDebug(VB_CHANNELOUT, "    Longest Chain  : %d\n", m_longestChain);
	LogDebug(VB_CHANNELOUT, "    Inverted Data  : %d\n", m_invertedData);

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
