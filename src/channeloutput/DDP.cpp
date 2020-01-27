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
#include "Warnings.h"

#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

#include <jsoncpp/json/json.h>
#include <strings.h>


#define DDP_HEADER_LEN 10
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

//1440 channels per packet
#define DDP_CHANNELS_PER_PACKET 1440

#define DDP_PACKET_LEN (DDP_HEADER_LEN + DDP_CHANNELS_PER_PACKET)

static const std::string DDPTYPE = "DDP";

const std::string &DDPOutputData::GetOutputTypeString() const {
    return DDPTYPE;
}

DDPOutputData::DDPOutputData(const Json::Value &config) : UDPOutputData(config), sequenceNumber(1) {
    memset((char *) &ddpAddress, 0, sizeof(sockaddr_in));
    ddpAddress.sin_family = AF_INET;
    ddpAddress.sin_port = htons(DDP_PORT);
    ddpAddress.sin_addr.s_addr = toInetAddr(ipAddress, valid);
    
    if (!valid) {
        WarningHolder::AddWarning("Could not resolve host name " + ipAddress + " - disabling output");
        active = false;
    }
    
    pktCount = channelCount / DDP_CHANNELS_PER_PACKET;
    if (channelCount % DDP_CHANNELS_PER_PACKET) {
        pktCount++;
    }
    
    ddpIovecs = (struct iovec *)calloc(pktCount * 2, sizeof(struct iovec));
    ddpBuffers = (unsigned char **)calloc(pktCount, sizeof(unsigned char*));
    
    int chan = startChannel - 1;
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
        int pktSize = DDP_CHANNELS_PER_PACKET;
        if (x == (pktCount - 1)) {
            ddpBuffers[x][0] = DDP_FLAGS1_VER1 | DDP_FLAGS1_PUSH;
            //last packet
            if (channelCount % DDP_CHANNELS_PER_PACKET) {
                pktSize = channelCount % DDP_CHANNELS_PER_PACKET;
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
    }
}
DDPOutputData::~DDPOutputData() {
    for (int x = 0; x < pktCount; x++) {
        free(ddpBuffers[x]);
    }
    free(ddpBuffers);
    free(ddpIovecs);
}

void DDPOutputData::PrepareData(unsigned char *channelData, UDPOutputMessages &msgs) {
    if (valid && active) {
        int start = startChannel - 1;
        if (type == 5) {
            start = 0;
        }
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        
        msg.msg_hdr.msg_name = &ddpAddress;
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        bool skipped = false;
        bool allSkipped = true;
        for (int p = 0; p < pktCount; p++) {
            bool nto = NeedToOutputFrame(channelData, startChannel-1, start, ddpIovecs[p * 2 + 1].iov_len);
            if (!nto && (p == (pktCount - 1)) && !allSkipped) {
                // at least one packet is not a duplicate, we need to send the last
                // packet so that the sync flag is sent
                nto = true;
            }
            if (nto) {
                msg.msg_hdr.msg_iov = &ddpIovecs[p * 2];
                msg.msg_hdr.msg_iovlen = 2;
                msg.msg_len = ddpIovecs[p * 2 + 1].iov_len + DDP_HEADER_LEN;
                
                msgs[ANY_MESSAGES_KEY].push_back(msg);

                unsigned char *header = ddpBuffers[p];
                header[1] = sequenceNumber & 0xF;
                if (sequenceNumber == 15) {
                    sequenceNumber = 1;
                } else {
                    ++sequenceNumber;
                }
                
                // set the pointer to the channelData for the universe
                ddpIovecs[p * 2 + 1].iov_base = (void*)(channelData + start);
                allSkipped = false;
            } else {
                skipped = true;
            }
            start += ddpIovecs[p * 2 + 1].iov_len;
        }
        if (skipped) {
            skippedFrames++;
        }
        if (!allSkipped) {
            SaveFrame(channelData);
        }
    }
}
void DDPOutputData::DumpConfig() {
    LogDebug(VB_CHANNELOUT, "DDP: %s   %d:%d:%d:%d  %s\n",
             description.c_str(),
             active,
             startChannel,
             channelCount,
             type,
             ipAddress.c_str());
}
