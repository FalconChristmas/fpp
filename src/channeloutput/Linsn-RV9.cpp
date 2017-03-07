/*
 *   Linsn RV9 Channel Output driver for Falcon Player (FPP)
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
 *   Packet Format: (based on captures from LED Studio 12.x to RV908T card)
 *
 *   FIXME - add packet format here once figured out
 *
 *   First packet of data frame
 *   - bytes  0 -  5 = Dst MAC (00:00:00:00:00:fe)
 *   - bytes  6 - 11 = Src MAC (PC's MAC)
 *   - bytes 12 - 13 = Protocol (0xAA55)
 *   - bytes 14 - 15 = 2-byte packet number for frame (LSB first)
 *   - byte  16      = 0x00
 *   - byte  17      = 0x00
 *   - byte  18      = 0x00
 *   - byte  19      = 0x00
 *   - byte  20      = 0x00
 *   - byte  21      = 0x00
 *   - byte  22      = 0x96
 *   - byte  23      = 0x00
 *   - byte  24      = 0x00
 *   - byte  25      = 0x00
 *   - byte  26      = 0x85 = (133)
 *   - byte  27      = 0x0f = ( 15)
 *   - byte  28      = 0xff = (255) // something to do with brightness
 *   - byte  29      = 0xff = (255) // something to do with brightness
 *   - byte  30      = 0xff = (255) // something to do with brightness
 *   - byte  31      = 0xff = (255) // something to do with brightness
 *   - byte  32      = 0x00
 *   - byte  33      = 0x00
 *   - byte  34      = 0x00
 *   - byte  35      = 0x00
 *   - byte  36      = 0x00
 *   - byte  37      = 0x00
 *   - byte  38      = 0x00
 *   - bytes 39 - 44 = Src MAC (PC's MAC)
 *   - byte  45      = 0xd2 = (210)
 *   - bytes 46 - 1485 = RGB Data
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
#include "Linsn-RV9.h"
#include "log.h"

/*
 *
 */
LinsnRV9Output::LinsnRV9Output(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_width(0),
	m_height(0),
	m_fd(-1),
	m_header(NULL),
	m_data(NULL),
	m_pktSize(LINSNRV9_BUFFER_SIZE),
	m_frameSize(0),
	m_framePackets(0),
	m_panelWidth(0),
	m_panelHeight(0),
	m_panels(0),
	m_rows(0),
	m_outputs(0),
	m_longestChain(0),
	m_invertedData(0),
	m_matrix(NULL),
	m_panelMatrix(NULL),
	m_formatCode(0xd2),
	m_formatWidth(512),
	m_formatHeight(256)
{
	LogDebug(VB_CHANNELOUT, "LinsnRV9Output::LinsnRV9Output(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = m_formatWidth * m_formatHeight * 3;

	struct FormatCode fc_e0 = { 0xe0, 32, 32 };
	m_formatCodes.push_back(fc_e0);

	struct FormatCode fc_e1 = { 0xe1, 64, 32 };
	m_formatCodes.push_back(fc_e1);

	struct FormatCode fc_d2 = { 0xd2, 512, 256 };
	m_formatCodes.push_back(fc_d2);

	struct FormatCode fc_c2 = { 0xc2, 1024, 512 };
	m_formatCodes.push_back(fc_c2);

	// FIXME, don't hardcode these, set based on m_width once we figureo ut
	// the codes other than 0xd2.
	m_formatCode = 0xd2;
	m_formatWidth = 512;
}

/*
 *
 */
LinsnRV9Output::~LinsnRV9Output()
{
	LogDebug(VB_CHANNELOUT, "LinsnRV9Output::~LinsnRV9Output()\n");

	if (m_fd >= 0)
		close(m_fd);

	if (m_outputFrame)
		delete [] m_outputFrame;
}

/*
 *
 */
int LinsnRV9Output::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "LinsnRV9Output::Init(JSON)\n");

	m_panelWidth  = config["panelWidth"].asInt();
	m_panelHeight = config["panelHeight"].asInt();

	if (!m_panelWidth)
		m_panelWidth = 32;

	if (!m_panelHeight)
		m_panelHeight = 16;

	m_invertedData = config["invertedData"].asInt();
	m_colorOrder = config["colorOrder"].asString();

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

	m_outputFrameSize = m_formatWidth * m_formatHeight * 3;
	m_outputFrame = new char[m_outputFrameSize];

	m_framePackets = (m_height * m_formatWidth / 480) + 1;

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
	// Set main data packet
	memset(m_buffer, 0, LINSNRV9_BUFFER_SIZE);
	m_eh = (struct ether_header *)m_buffer;
	m_header = m_buffer + sizeof(struct ether_header);
	m_data = m_header + LINSNRV9_HEADER_SIZE;
	m_eh->ether_type = htons(0xAA55);

	m_pktSize = sizeof(struct ether_header) + LINSNRV9_HEADER_SIZE + LINSNRV9_DATA_SIZE;

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

	// Send discovery/wakeup packets
	SetDiscoveryMACs(m_buffer);

	m_buffer[22] = 0x96;

	m_buffer[26] = 0x85;
	m_buffer[27] = 0x1f;
	m_buffer[28] = 0xff; // something to do with brightness
	m_buffer[29] = 0xff; // something to do with brightness
	m_buffer[30] = 0xff; // something to do with brightness
	m_buffer[31] = 0xff; // something to do with brightness

	m_buffer[45] = m_formatCode; // format Code

	m_buffer[47] = 0xfe;
	m_buffer[48] = 0xff;

	for (int i = 0; i < 2; i++)
	{
		if (sendto(m_fd, m_buffer, LINSNRV9_BUFFER_SIZE, 0, (struct sockaddr*)&m_sock_addr, sizeof(struct sockaddr_ll)) < 0)
		{
			LogErr(VB_CHANNELOUT, "Error sending row data packet: %s\n", strerror(errno));
			return 0;
		}

//		usleep(200000);
		sleep(1);
	}

	// Set MACs for data packets
	// FIXME, this should use the MAC received during discovery
	SetHostMACs(m_buffer);

	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int LinsnRV9Output::Close(void)
{
	LogDebug(VB_CHANNELOUT, "LinsnRV9Output::Close()\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
void LinsnRV9Output::PrepData(unsigned char *channelData)
{
	unsigned char *r = NULL;
	unsigned char *g = NULL;
	unsigned char *b = NULL;
	unsigned char *dst = NULL;
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
				int pw3 = m_panelWidth * 3;

				for (int x = 0; x < pw3; x += 3)
				{
					// FIXME, optimize this since it is called once per pixel
					if (m_colorOrder == "RGB")
					{
						r = channelData + m_panelMatrix->m_panels[panel].pixelMap[yw + x];
						g = r + 1;
						b = r + 2;
					}
					else if (m_colorOrder == "RBG")
					{
						r = channelData + m_panelMatrix->m_panels[panel].pixelMap[yw + x];
						b = r + 1;
						g = r + 2;
					}
					else if (m_colorOrder == "GRB")
					{
						g = channelData + m_panelMatrix->m_panels[panel].pixelMap[yw + x];
						r = g + 1;
						b = g + 2;
					}
					else if (m_colorOrder == "GBR")
					{
						g = channelData + m_panelMatrix->m_panels[panel].pixelMap[yw + x];
						b = g + 1;
						r = g + 2;
					}
					else if (m_colorOrder == "BRG")
					{
						b = channelData + m_panelMatrix->m_panels[panel].pixelMap[yw + x];
						r = b + 1;
						g = b + 2;
					}
					else if (m_colorOrder == "BGR")
					{
						b = channelData + m_panelMatrix->m_panels[panel].pixelMap[yw + x];
						g = b + 1;
						r = b + 2;
					}
					else
					{
						r = channelData + m_panelMatrix->m_panels[panel].pixelMap[yw + x];
						g = r + 1;
						b = r + 2;
					}

if (0 && (y < 2) && (x < 12))
{
                  int offset = (((((output * m_panelHeight) + y) * m_formatWidth) + px) * 3);
                  LogDebug(VB_CHANNELOUT, "op: %d, ph: %d, y: %d, x: %d, pw: %d, ch: %d, p: %d, px: %d, o: %d\n", output, m_panelHeight, y, x, m_panelWidth, chain, panel, px, offset);
}

					//dst = (unsigned char*)(m_outputFrame + (((((output * m_panelHeight) + y) * m_panelWidth * m_longestChain) + px) * 3));
					dst = (unsigned char*)(m_outputFrame + (((((output * m_panelHeight) + y) * m_formatWidth) + px) * 3));

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
int LinsnRV9Output::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "LinsnRV9Output::RawSendData(%p)\n", channelData);

	SetHostMACs(m_buffer);
	memset(m_data, 0, LINSNRV9_DATA_SIZE);

	m_buffer[14] = 0x00;
	m_buffer[15] = 0x00;

	m_buffer[22] = 0x96;

	m_buffer[26] = 0x85;
	m_buffer[27] = 0x0f;
	m_buffer[28] = 0xff; // something to do with brightness
	m_buffer[29] = 0xff; // something to do with brightness
	m_buffer[30] = 0xff; // something to do with brightness
	m_buffer[31] = 0xff; // something to do with brightness

	// FIXME, this seems to be related to the matrix size
	m_buffer[45] = m_formatCode;

	if (sendto(m_fd, m_buffer, LINSNRV9_BUFFER_SIZE, 0, (struct sockaddr*)&m_sock_addr, sizeof(struct sockaddr_ll)) < 0)
	{
		LogErr(VB_CHANNELOUT, "Error sending row data packet: %s\n", strerror(errno));
		return 0;
	}

	int row = 0;
	int frameNumber = 1;
	int bytesSent = 0;
	int framesSent = 0;

	memset(m_header, 0, LINSNRV9_HEADER_SIZE);

	//while (bytesSent < m_outputFrameSize)
	while (frameNumber < m_framePackets)
	{
		m_buffer[14] = (unsigned char)(frameNumber & 0x00FF);
		m_buffer[15] = (unsigned char)(frameNumber >> 8);

		memcpy(m_data, m_outputFrame + bytesSent, LINSNRV9_DATA_SIZE);

		if (sendto(m_fd, m_buffer, LINSNRV9_BUFFER_SIZE, 0, (struct sockaddr*)&m_sock_addr, sizeof(struct sockaddr_ll)) < 0)
		{
			LogErr(VB_CHANNELOUT, "Error sending row data packet: %s\n", strerror(errno));
			return 0;
		}

		bytesSent += LINSNRV9_DATA_SIZE;
		frameNumber++;
	}

	return m_channelCount;
}

/*
 *
 */
void LinsnRV9Output::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "LinsnRV9Output::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Interface      : %s\n", m_ifName.c_str());
	LogDebug(VB_CHANNELOUT, "    Width          : %d\n", m_width);
	LogDebug(VB_CHANNELOUT, "    Height         : %d\n", m_height);
	LogDebug(VB_CHANNELOUT, "    m_fd           : %d\n", m_fd);
	LogDebug(VB_CHANNELOUT, "    m_pktSize      : %d\n", m_pktSize);
	LogDebug(VB_CHANNELOUT, "    m_frameSize    : %d\n", m_frameSize);
	LogDebug(VB_CHANNELOUT, "    m_framePackets : %d (0x%02x)\n",
		m_framePackets, m_framePackets);

	ChannelOutputBase::DumpConfig();
}

/*
 *
 */
void LinsnRV9Output::SetHostMACs(void *ptr)
{
	struct ifreq ifr;
	int s;
	struct ether_header *eh = (struct ether_header *)ptr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifr.ifr_name, m_ifName.c_str());
	ioctl(s, SIOCGIFHWADDR, &ifr);

	// Set the source MAC address
	// FIXME, needs to be the host's MAC, get from E1.31 channel output code
	eh->ether_shost[0] = ifr.ifr_hwaddr.sa_data[0];
	eh->ether_shost[1] = ifr.ifr_hwaddr.sa_data[1];
	eh->ether_shost[2] = ifr.ifr_hwaddr.sa_data[2];
	eh->ether_shost[3] = ifr.ifr_hwaddr.sa_data[3];
	eh->ether_shost[4] = ifr.ifr_hwaddr.sa_data[4];
	eh->ether_shost[5] = ifr.ifr_hwaddr.sa_data[5];

	// Set the dest MAC address
	eh->ether_dhost[0] = 0x00;
	eh->ether_dhost[1] = 0x00;
	eh->ether_dhost[2] = 0x00;
	eh->ether_dhost[3] = 0x00;
	eh->ether_dhost[4] = 0x00;
	eh->ether_dhost[5] = 0xfe;

	// Linsn also embed's sender MAC in its header
	((unsigned char *)ptr)[39] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[0];
	((unsigned char *)ptr)[40] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[1];
	((unsigned char *)ptr)[41] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[2];
	((unsigned char *)ptr)[42] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[3];
	((unsigned char *)ptr)[43] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[4];
	((unsigned char *)ptr)[44] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[5];

	close(s);
}

/*
 *
 */
void LinsnRV9Output::SetDiscoveryMACs(void *ptr)
{
	struct ifreq ifr;
	int s;
	struct ether_header *eh = (struct ether_header *)ptr;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifr.ifr_name, m_ifName.c_str());
	ioctl(s, SIOCGIFHWADDR, &ifr);

	// Set the source MAC address
	// FIXME, needs to be the host's MAC, get from E1.31 channel output code
	eh->ether_shost[0] = ifr.ifr_hwaddr.sa_data[0];
	eh->ether_shost[1] = ifr.ifr_hwaddr.sa_data[1];
	eh->ether_shost[2] = ifr.ifr_hwaddr.sa_data[2];
	eh->ether_shost[3] = ifr.ifr_hwaddr.sa_data[3];
	eh->ether_shost[4] = ifr.ifr_hwaddr.sa_data[4];
	eh->ether_shost[5] = ifr.ifr_hwaddr.sa_data[5];

	// Set the dest MAC address
	eh->ether_dhost[0] = 0xff;
	eh->ether_dhost[1] = 0xff;
	eh->ether_dhost[2] = 0xff;
	eh->ether_dhost[3] = 0xff;
	eh->ether_dhost[4] = 0xff;
	eh->ether_dhost[5] = 0xff;

	// Linsn also embed's sender MAC in its header
	((unsigned char *)ptr)[39] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[0];
	((unsigned char *)ptr)[40] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[1];
	((unsigned char *)ptr)[41] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[2];
	((unsigned char *)ptr)[42] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[3];
	((unsigned char *)ptr)[43] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[4];
	((unsigned char *)ptr)[44] = ((unsigned char *)ifr.ifr_hwaddr.sa_data)[5];

	close(s);
}

