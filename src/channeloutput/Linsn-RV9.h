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
 */

#ifndef _LINSNRV9_H
#define _LINSNRV9_H

#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <string>
#include <vector>

#include "ChannelOutputBase.h"
#include "ColorOrder.h"
#include "Matrix.h"
#include "PanelMatrix.h"

#define LINSNRV9_BUFFER_SIZE  1486
#define LINSNRV9_HEADER_SIZE  32
#define LINSNRV9_DATA_SIZE    1440

class LinsnRV9Output : public ChannelOutputBase {
  public:
	LinsnRV9Output(unsigned int startChannel, unsigned int channelCount);
	~LinsnRV9Output();

	int  Init(Json::Value config);
	int  Close(void);

	void PrepData(unsigned char *channelData);
	int  RawSendData(unsigned char *channelData);

	void DumpConfig(void);

  private:
	void HandShake(void);

	void GetSrcMAC(void);
	void SetHostMACs(void *data);
	void SetDiscoveryMACs(void *data);

	int          m_width;
	int          m_height;
	std::string  m_ifName;

	FPPColorOrder m_colorOrder;

	int   m_fd;

	char  m_buffer[LINSNRV9_BUFFER_SIZE];
	char *m_header;
	char *m_data;
	char *m_rowData;
	int   m_pktSize;
	int   m_framePackets;
	int   m_frameNumber;

	struct ifreq          m_if_idx;
	struct ifreq          m_if_mac;
	struct ether_header  *m_eh;
	struct sockaddr_ll    m_sock_addr;

	int          m_panelWidth;
	int          m_panelHeight;
	int          m_panels;
	int          m_rows;
	int          m_outputs;
	int          m_longestChain;
	int          m_invertedData;
	char        *m_outputFrame;
	int          m_outputFrameSize;
	Matrix      *m_matrix;
	PanelMatrix *m_panelMatrix;
	int          m_formatIndex;

	struct FormatCode {
		unsigned char code;
		int           width;
		int           height;
		int           dataOffset;
		int           d27;
	};

	std::vector<struct FormatCode> m_formatCodes;

	unsigned char m_srcMAC[6];
	unsigned char m_dstMAC[6];
};

#endif
