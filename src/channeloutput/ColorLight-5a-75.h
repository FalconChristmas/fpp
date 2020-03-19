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
 */

#ifndef _COLORLIGHT5A75_H
#define _COLORLIGHT5A75_H

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <string>
#include <sys/uio.h>
#include <vector>

#include "ChannelOutputBase.h"
#include "ColorOrder.h"
#include "Matrix.h"
#include "PanelMatrix.h"

#define CL5A75_BUFFER_SIZE               1536
#define CL5A75_HEADER_LEN                7
#define CL5A75_MAX_PIXELS_PER_PACKET     497
#define CL5A75_MAX_CHANNELS_PER_PACKET   (CL5A75_MAX_PIXELS_PER_PACKET * 3)
#define CL5A75_0101_PACKET_DATA_LEN      98
#define CL5A75_0AFF_PACKET_DATA_LEN      63


class ColorLight5a75Output : public ChannelOutputBase {
  public:
	ColorLight5a75Output(unsigned int startChannel, unsigned int channelCount);
	virtual ~ColorLight5a75Output();

	virtual int  Init(Json::Value config) override;
	virtual int  Close(void) override;

	virtual void PrepData(unsigned char *channelData) override;
	virtual int  SendData(unsigned char *channelData) override;

	virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;

  private:
	void SetHostMACs(void *data);

	int          m_width;
	int          m_height;
	std::string  m_layout;
	std::string  m_ifName;

	FPPColorOrder m_colorOrder;

	int   m_fd;
	int   m_rowSize;
    
    int m_slowCount;

	struct ifreq          m_if_idx;
	struct ifreq          m_if_mac;
	struct ether_header  *m_eh;
	struct sockaddr_ll    m_sock_addr;

	std::vector<struct mmsghdr>  m_msgs;
	std::vector<struct iovec>    m_iovecs;

	int          m_panelWidth;
	int          m_panelHeight;
	int          m_panels;
	int          m_rows;
	int          m_outputs;
	int          m_longestChain;
	int          m_invertedData;
	char        *m_outputFrame;
	Matrix      *m_matrix;
	PanelMatrix *m_panelMatrix;
    uint8_t      m_gammaCurve[256];
    int          m_flippedLayout;

};

#endif
