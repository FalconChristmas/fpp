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
	m_fd(-1),
	m_buffer_0101(NULL),
	m_buffer_0101_len(0),
	m_buffer_0AFF(NULL),
	m_buffer_0AFF_len(0),
	m_data(NULL),
	m_rowSize(0),
	m_pktSize(0)
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
}

/*
 *
 */
int ColorLight5a75Output::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "ColorLight5a75Output::Init(JSON)\n");

	// FIXME, don't hardcode this
	if (config.isMember("interface"))
		m_ifName = config["interface"].asString();
	else
		m_ifName = "eth0";

	if (config.isMember("width"))
	{
		m_width = config["width"].asInt();
	}
	else
	{
		LogErr(VB_CHANNELOUT, "width attribute is not defined!\n");
		return 0;
	}

	if (config.isMember("height"))
	{
		m_height = config["height"].asInt();
	}
	else
	{
		LogErr(VB_CHANNELOUT, "height attribute is not defined!\n");
		return 0;
	}

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

	m_rowSize = m_width * 3;
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
	m_data[3] = 0x00; // ??, mplayer code had 0x01 here
	m_data[4] = 0x80; // ??, mplayer code had 0x00 here
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

	char *rowPtr = (char *)channelData;
	int row = 0;
	for (row = 0; row < m_height; row++)
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
	LogDebug(VB_CHANNELOUT, "    Row Size       : %d\n", m_rowSize);
	LogDebug(VB_CHANNELOUT, "    m_fd           : %d\n", m_fd);
	LogDebug(VB_CHANNELOUT, "    Data Pkt Size  : %d\n", m_pktSize);

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
