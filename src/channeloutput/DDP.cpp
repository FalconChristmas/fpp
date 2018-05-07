/*
 *   DDP Channel Output driver for Falcon Player (FPP)
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
 *   ***********************************************************************
 *
 *   This Channel Output implements the data packet portion of the DDP
 *   protocol.  Discovery is not currently supported. Data packets are
 *   sent to port 4048 UDP or TCP. This implementation uses UDP.
 *
 *   The full DPP spec is currently located at http://www.3waylabs.com/ddp/
 *
 *   The data packet format is as follows: (mostly copied from above URL)
 *
 *   byte 0:     Flags: V V x T S R Q P
 *               V V:    2-bits for protocol version number (01 for this file)
 *               x:      reserved
 *               T:      timecode field added to end of header
 *                       if T & P are set, Push at specified time
 *               S:      Storage.  If set, data from Storage, not data-field.
 *               R:      Reply flag, marks reply to Query packet.
 *                       always set when any packet is sent by a Display.
 *                       if Reply, Q flag is ignored.
 *               Q:      Query flag, requests len data from ID at offset
 *                       (no data sent). if clear, is a Write buffer packet
 *               P:      Push flag, for display synchronization, or marks
 *                       last packet of Reply
 *
 *   byte  1:    Flags: x x x x n n n n
 *               x:      reserved for future use (set to zero)
 *               nnnn:   sequence number from 1-15, or zero if not used.
 *                       a sender can send duplicate packets with the same
 *                       sequence number and DDP header for redundancy.
 *                       a receiver can ignore duplicates received back-to-back.
 *                       the sequence number is ignored if zero.
 *
 *   byte  2:    data type: S x t t t b b b
 *               S:      0 = standard types, 1 = custom
 *               x:      reserved for future use (set to zero)
 *               ttt:    type, 0 = greyscale, 1 = rgb, 2 = hsl?
 *               bbb:    bits per pixel:
 *                       0=1, 1=4, 2=8, 3=16, 4=24, 5=32, 6=48, 7=64
 *
 *   byte  3:    Source or Destination ID
 *               0 = reserved
 *               1 = default output device
 *               2=249 custom IDs, (possibly defined via JSON config) ?????
 *               246 = JSON control (read/write)
 *               250 = JSON config  (read/write)
 *               251 = JSON status  (read only)
 *               254 = DMX transit
 *               255 = all devices
 *
 *   byte  4-7:  data offset in bytes (in units based on data-type.
 *               ie: RGB=3 bytes=1 unit) or bytes??  32-bit number, MSB first
 *
 *   byte  8-9:  data length in bytes (size of data field when writing)
 *               16-bit number, MSB first
 *               for Queries, this specifies size of data to read,
 *                   no data field follows header.
 *
 *  if T flag, header extended 4 bytes for timecode field (not counted in
 *  data length)
 *
 *  byte 10-13: timecode
 *
 *  if no T flag, data starts at byte 10, otherwise byte 14
 *
 *  byte 10 or 14: start of data
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>

#include "common.h"
#include "DDP.h"
#include "log.h"
#include "Sequence.h"
#include "settings.h"

#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

#include <jsoncpp/json/json.h>
#include <strings.h>


#define DDP_HEADER_LEN 10
#define DDP_PACKET_LEN (DDP_HEADER_LEN + 1440)
#define DDP_SYNCPACKET_LEN 10

#define DDP_FLAGS1_VER     0xc0   // version mask
#define DDP_FLAGS1_VER1    0x40   // version=1
#define DDP_FLAGS1_PUSH    0x01
#define DDP_FLAGS1_QUERY   0x02
#define DDP_FLAGS1_REPLY   0x04
#define DDP_FLAGS1_STORAGE 0x08
#define DDP_FLAGS1_TIME    0x10

#define DDP_ID_DISPLAY       1
#define DDP_ID_CONFIG      250
#define DDP_ID_STATUS      251

class DDPOutputData {
public:
    DDPOutputData(int t, int st, int sz, const std::string &add)
        : type(t), start(st), size(sz), address(add), seqNum(1) {
            
        memset((char *) &socketAddress, 0, sizeof(socketAddress));
        socketAddress.sin_family = AF_INET;
        socketAddress.sin_port = htons(DDP_PORT);
            
        const char *c = address.c_str();
        int isAlpha = 0;
        for (; *c && !isAlpha; c++)
            isAlpha = isalpha(*c);
        
        if (isAlpha) {
            struct hostent* uhost = gethostbyname(address.c_str());
            if (!uhost) {
                LogErr(VB_CHANNELOUT,
                       "Error looking up DDP hostname: %s\n",
                       address.c_str());
                type = 0;
            } else {
                socketAddress.sin_addr.s_addr = *((unsigned long*)uhost->h_addr);
            }
        } else {
            socketAddress.sin_addr.s_addr = inet_addr(address.c_str());
        }
        
        //1440 channels per packet
        pktCount = size / 1440;
        if (size % 1440) {
            pktCount++;
        }
            
        ddpMsgs = (struct mmsghdr *)calloc(pktCount, sizeof(struct mmsghdr));
        ddpIovecs = (struct iovec *)calloc(pktCount * 2, sizeof(struct iovec));
        ddpBuffers = (unsigned char **)calloc(pktCount, sizeof(unsigned char*));

            
        int chan = start;
        if (type == 5) {
            chan = 0;
        }
        for (int x = 0; x < pktCount; x++) {
            ddpBuffers[x] = (unsigned char *)calloc(1, DDP_HEADER_LEN);

            // use scatter/gather for the packet.   One IOV will contain
            // the header, the second will point into the raw channel data
            // and will be set at output time.   This avoids any memcpy.
            ddpIovecs[x * 2].iov_base = ddpBuffers[x];
            ddpIovecs[x * 2].iov_len = DDP_HEADER_LEN;
            ddpIovecs[x * 2 + 1].iov_base = nullptr;
            
            ddpBuffers[x][0] = DDP_FLAGS1_VER1;
            ddpBuffers[x][2] = 1;
            ddpBuffers[x][3] = DDP_ID_DISPLAY;
            int pktSize = 1440;
            if (x == (pktCount - 1)) {
                ddpBuffers[x][0] = DDP_FLAGS1_VER1 | DDP_FLAGS1_PUSH;
                //last packet
                if (size % 1440) {
                    pktSize = size % 1440;
                }
            }
            ddpIovecs[x * 2 + 1].iov_len = pktSize;
            
            //offset
            ddpBuffers[x][4] = (chan & 0xFF000000) >> 24;
            ddpBuffers[x][5] = (chan & 0xFF0000) >> 16;
            ddpBuffers[x][6] = (chan & 0xFF00) >> 8;
            ddpBuffers[x][7] = (chan & 0xFF);

            //size
            ddpBuffers[x][8] = (pktSize & 0xFF00) >> 8;
            ddpBuffers[x][9] = pktSize & 0xFF;
            
            chan += pktSize;

            ddpMsgs[x].msg_hdr.msg_name = &socketAddress;
            ddpMsgs[x].msg_hdr.msg_namelen = sizeof(sockaddr_in);
            ddpMsgs[x].msg_hdr.msg_iov = &ddpIovecs[x * 2];
            ddpMsgs[x].msg_hdr.msg_iovlen = 2;
            ddpMsgs[x].msg_len = pktSize + DDP_HEADER_LEN;
        }
    }
    ~DDPOutputData() {
        for (int x = 0; x < pktCount; x++) {
            free(ddpBuffers[x]);
        }
        free(ddpMsgs);
        free(ddpIovecs);
        free(ddpMsgs);
    }
    
    int type;
    int start;
    int size;
    std::string address;
    
    sockaddr_in socketAddress;
    int pktCount;
    struct mmsghdr *ddpMsgs = nullptr;
    struct iovec *ddpIovecs = nullptr;
    unsigned char **ddpBuffers = nullptr;
    
    unsigned char seqNum;
};


/*
 *
 */
DDPOutput::DDPOutput(unsigned int startChannel, unsigned int channelCount)
    : ChannelOutputBase(startChannel, channelCount), sendSocket(-1)
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::DDPOutput(%u, %u)\n",
		startChannel, channelCount);

	// Set any max channels limit if necessary
	m_maxChannels = FPPD_MAX_CHANNELS;
    m_channelCount = FPPD_MAX_CHANNELS;
    m_useOutputThread = 0;
    m_startChannel = 0;
}

/*
 *
 */
DDPOutput::~DDPOutput()
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::~DDPOutput()\n");
}

bool DDPOutput::InitFromUniverses() {
    FILE *fp;
    char buf[512];
    char *s;
    char filename[1024];
    
    strcpy(filename, getMediaDirectory());
    strcat(filename, "/config/co-universes.json");
    
    LogDebug(VB_CHANNELOUT, "Opening File Now %s\n", filename);
    if (!FileExists(filename)) {
        LogDebug(VB_CHANNELOUT, "Universe file %s does not exist\n",
               filename);
        return false;
    }
    
    Json::Value root;
    Json::Reader reader;
    std::ifstream t(filename);
    std::stringstream buffer;
    
    buffer << t.rdbuf();
    
    std::string config = buffer.str();
    bool success = reader.parse(buffer.str(), root);
    if (!success) {
        LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename);
        return false;
    }
    
    Json::Value outputs = root["channelOutputs"];
    for (int c = 0; c < outputs.size(); c++) {
        if (outputs[c]["type"].asString() != "universes")
            continue;
        if (outputs[c]["enabled"].asInt() == 0)
            continue;
        
        Json::Value univs = outputs[c]["universes"];

        for (int i = 0; i < univs.size(); i++) {
            Json::Value u = univs[i];
            int type = u["type"].asInt();
            if (u["active"].asInt() && (type == 4 || type == 5)) {
                outputData.push_back(new DDPOutputData(type,
                                                   u["startChannel"].asInt() - 1,
                                                   u["channelCount"].asInt(),
                                                   u["address"].asString()));
            }
        }
    }
    DumpConfig();
    return InitNetwork();
}

bool DDPOutput::InitNetwork() {
    if (outputData.size() == 0) {
        return false;
    }
    sendSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendSocket < 0) {
        LogErr(VB_CHANNELOUT, "Error opening datagram socket\n");
        exit(1);
    }
    struct sockaddr_in localAddress;
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = 0;
    
    char localIPAddress[16];
    GetInterfaceAddress(getE131interface(), localIPAddress, NULL, NULL);
    localAddress.sin_addr.s_addr = inet_addr(localIPAddress);
    if (bind(sendSocket, (struct sockaddr *) &localAddress, sizeof(struct sockaddr_in)) == -1) {
        LogErr(VB_CHANNELOUT, "Error in bind:errno=%d, %s\n", errno, strerror(errno));
        return false;
    }
    /* Disable loopback so I do not receive my own datagrams. */
    char loopch = 0;
    if(setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
        close(sendSocket);
        return false;
    }
    for (auto u : outputData) {
        if (u->type == 0) {
            close(sendSocket);
            return false;
        }
    }
    return true;
}

/*
 *
 */
int DDPOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::Init(JSON)\n");

	// Call the base class' Init() method, do not remove this line.
	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int DDPOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::Close()\n");
    if (sendSocket != -1) {
        close(sendSocket);
    }

	return ChannelOutputBase::Close();
}

/*
 *
 */
int DDPOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "DDPOutput::RawSendData(%p)\n", channelData);
    for (auto u : outputData) {
        int start = u->start;
        if (u->type == 5) {
            start = 0;
        }
        for (int p = 0; p < u->pktCount; p++) {
            unsigned char *header = u->ddpBuffers[p];
            header[1] = u->seqNum & 0xF;
            if (u->seqNum == 15) {
                u->seqNum = 1;
            } else {
                ++u->seqNum;
            }
            
            // set the pointer to the channelData for the universe
            u->ddpMsgs[p].msg_hdr.msg_iov[1].iov_base = (void*)(channelData + start);
            start += u->ddpIovecs[p * 2 + 1].iov_len;
        }
        int outputCount = sendmmsg(sendSocket, u->ddpMsgs, u->pktCount, 0);
        if (outputCount != u->pktCount) {
            LogErr(VB_CHANNELOUT, "sendto() failed for DDP (output count: %d) with error: %s\n",
                   outputCount,
                   strerror(errno));
            return 0;
        }
    }
	return 1;
}

/*
 *
 */
void DDPOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::DumpConfig()\n");
    for (auto u : outputData) {
        LogDebug(VB_CHANNELOUT, "DDP: %d:%d:%d  %s\n",
                 u->type,
                 u->start,
                 u->size,
                 u->address.c_str());
    }
	ChannelOutputBase::DumpConfig();
}

