/*
 *   ZCPP Channel Output driver for Falcon Player (FPP)
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
 *   This Channel Output implements the data packet portion of the ZCPP
 *   protocol.  Discovery is not currently supported. Data packets are
 *   sent to port 30005 UDP or TCP. This implementation uses UDP.
 *
 *   The full ZCPP spec is currently located at https://raw.githubusercontent.com/smeighan/xLights/master/Zero%20Configuration%20Pixel%20Protocol.docx
 *
 */

#include "fpp-pch.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>

#include "ZCPP.h"

#define ZCPP_HEADER_LEN 15

//1458 channels per packet
#define ZCPP_CHANNELS_PER_PACKET 1449

#define ZCPP_PACKET_LEN (ZCPP_HEADER_LEN + ZCPP_CHANNELS_PER_PACKET)

static const std::string ZCPPTYPE = "ZCPP";

const std::string &ZCPPOutputData::GetOutputTypeString() const {
    return ZCPPTYPE;
}

ZCPPOutputData::ZCPPOutputData(const Json::Value &config) : UDPOutputData(config), sequenceNumber(1) {
    memset((char *) &zcppAddress, 0, sizeof(sockaddr_in));
    zcppAddress.sin_family = AF_INET;
    zcppAddress.sin_port = htons(ZCPP_PORT);
    zcppAddress.sin_addr.s_addr = toInetAddr(ipAddress, valid);
    
    if (!valid) {
        WarningHolder::AddWarning("Could not resolve host name " + ipAddress + " - disabling output");
        active = false;
    }

    priority = config["priority"].asInt();

    pktCount = channelCount / ZCPP_CHANNELS_PER_PACKET;
    if (channelCount % ZCPP_CHANNELS_PER_PACKET) {
        pktCount++;
    }

    zcppIovecs = (struct iovec *)calloc(pktCount * 2, sizeof(struct iovec));
    zcppBuffers = (unsigned char **)calloc(pktCount, sizeof(unsigned char*));
    
    //int chan = startChannel - 1;
    int chan = 0;

    for (int x = 0; x < pktCount; x++) {
        zcppBuffers[x] = (unsigned char *)calloc(1, ZCPP_HEADER_LEN);
        
        // use scatter/gather for the packet.   One IOV will contain
        // the header, the second will point into the raw channel data
        // and will be set at output time.   This avoids any memcpy.
        zcppIovecs[x * 2].iov_base = zcppBuffers[x];
        zcppIovecs[x * 2].iov_len = ZCPP_HEADER_LEN;
        zcppIovecs[x * 2 + 1].iov_base = nullptr;
        
        zcppBuffers[x][0] = 'Z';
        zcppBuffers[x][1] = 'C';
        zcppBuffers[x][2] = 'P';
        zcppBuffers[x][3] = 'P';
        zcppBuffers[x][4] = 0x14;
        zcppBuffers[x][5] = 0x00;
        zcppBuffers[x][7] = 0x00;
        zcppBuffers[x][14] = priority;

        if (x == 0) 
            zcppBuffers[x][7] = ZCPP_DATA_FLAG_FIRST;

        int pktSize = ZCPP_CHANNELS_PER_PACKET;
        if (x == (pktCount - 1)) {
            zcppBuffers[x][7] = ZCPP_DATA_FLAG_LAST;
            //last packet
            if (channelCount % ZCPP_CHANNELS_PER_PACKET) {
                pktSize = channelCount % ZCPP_CHANNELS_PER_PACKET;
            }
        }
        zcppIovecs[x * 2 + 1].iov_len = pktSize;
        
        //offset
        zcppBuffers[x][8] = (chan & 0xFF000000) >> 24;
        zcppBuffers[x][9] = (chan & 0xFF0000) >> 16;
        zcppBuffers[x][10] = (chan & 0xFF00) >> 8;
        zcppBuffers[x][11] = (chan & 0xFF);
        
        //size
        zcppBuffers[x][12] = (pktSize & 0xFF00) >> 8;
        zcppBuffers[x][13] = pktSize & 0xFF;
        
        chan += pktSize;
    }
}
ZCPPOutputData::~ZCPPOutputData() {
    for (int x = 0; x < pktCount; x++) {
        free(zcppBuffers[x]);
    }
    free(zcppBuffers);
    free(zcppIovecs);
}

void ZCPPOutputData::PrepareData(unsigned char *channelData, UDPOutputMessages &msgs) {
    if (valid && active) {
        int start = 0;

        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        
        msg.msg_hdr.msg_name = &zcppAddress;
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        bool skipped = false;
        bool allSkipped = true;
        for (int p = 0; p < pktCount; p++) {
            bool nto = NeedToOutputFrame(channelData, startChannel-1, start, zcppIovecs[p * 2 + 1].iov_len);
            if (!nto && (p == (pktCount - 1)) && !allSkipped) {
                // at least one packet is not a duplicate, we need to send the last
                // packet so that the sync flag is sent
                nto = true;
            }
            if (nto) {
                msg.msg_hdr.msg_iov = &zcppIovecs[p * 2];
                msg.msg_hdr.msg_iovlen = 2;
                msg.msg_len = zcppIovecs[p * 2 + 1].iov_len + ZCPP_HEADER_LEN;
                
                msgs[ANY_MESSAGES_KEY].push_back(msg);

                unsigned char *header = zcppBuffers[p];
                header[6] = sequenceNumber & 0xF;
                if (sequenceNumber == 255) {
                    sequenceNumber = 1;
                } else {
                    ++sequenceNumber;
                }
                
                // set the pointer to the channelData for the universe
                zcppIovecs[p * 2 + 1].iov_base = (void*)(channelData + start);
                allSkipped = false;
            } else {
                skipped = true;
            }
            start += zcppIovecs[p * 2 + 1].iov_len;
        }
        if (skipped) {
            skippedFrames++;
        }
        if (!allSkipped) {
            SaveFrame(channelData);
        }
    }
}
void ZCPPOutputData::DumpConfig() {
    LogDebug(VB_CHANNELOUT, "ZCPP: %s   %d:%d:%d:%d:%d  %s\n",
             description.c_str(),
             active,
             startChannel,
             channelCount,
             priority,
             type,
             ipAddress.c_str());
}
