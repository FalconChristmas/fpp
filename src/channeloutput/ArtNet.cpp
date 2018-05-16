/*
 *   ArtNet output handler for Falcon Player (FPP)
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
#include <string.h>
#include <math.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netdb.h>

#include <fstream>
#include <sstream>
#include <string>

#include <jsoncpp/json/json.h>

#include "channeloutput.h"
#include "channeloutputthread.h"
#include "common.h"
#include "ArtNet.h"
#include "FPD.h"
#include "fpp.h"
#include "log.h"
#include "settings.h"


#define MAX_ARTNET_UNIVERSE_COUNT    512
#define ARTNET_HEADER_LENGTH         18
#define ARTNET_SYNC_PACKET_LENGTH    14

#define ARTNET_DEST_PORT        6454
#define ARTNET_SOURCE_PORT      58302
#define ARTNET_SYNC_SOURCE_PORT 58303

#define ARTNET_SEQUENCE_INDEX   12
#define ARTNET_UNIVERSE_INDEX   14
#define ARTNET_LENGTH_INDEX     16

#define ARTNET_TYPE_BROADCAST 2
#define ARTNET_TYPE_UNICAST   3


const char  ArtNetHeader[] = {
	'A', 'r', 't', '-', 'N', 'e', 't', 0x00, // 8-byte ID
	0x00, // Opcode Low
	0x50, // Opcode High
	0x00, // Protocol Version High
	0x0E, // Protocol Version Low
	0x00, // Sequence #
	0x00, // Physical #
	0x00, // Universe Number Low
	0x00, // Universe Number High (7 bits)
	0x00, // Length High (length always an even number)
	0x00  // Length Low
	};

const char ArtNetSyncPacket[] = {
	'A', 'r', 't', '-', 'N', 'e', 't', 0x00, // 8-byte ID
	0x00, // Opcode Low
	0x52, // Opcode High
	0x00, // Protocol Version High
	0x0E, // Protocol Version Low
	0x00, // Aux1
	0x00  // Aux2
	};

static struct iovec ArtNetSyncIovecs = { (void*)ArtNetSyncPacket, ARTNET_SYNC_PACKET_LENGTH };
static struct sockaddr_in   ArtNetSyncAddress;



ArtNetOutputData::ArtNetOutputData(const Json::Value &config)
: UDPOutputData(config), sequenceNumber(1) {
    memset((char *) &anAddress, 0, sizeof(sockaddr_in));
    anAddress.sin_family = AF_INET;
    anAddress.sin_port = htons(ARTNET_DEST_PORT);

    
    memset((char *) &ArtNetSyncAddress, 0, sizeof(sockaddr_in));
    ArtNetSyncAddress.sin_family = AF_INET;
    ArtNetSyncAddress.sin_port = htons(0);
    ArtNetSyncAddress.sin_addr.s_addr = inet_addr("255.255.255.255");

    universe = config["id"].asInt();
    priority = config["priority"].asInt();
    switch (type) {
        case ARTNET_TYPE_BROADCAST: // Multicast
            ipAddress = "";
            break;
        case 3: //UnicastAddress
            ipAddress = config["address"].asString();
            break;
    }
    
    if (type == ARTNET_TYPE_BROADCAST) {
        anAddress.sin_addr.s_addr = inet_addr("255.255.255.255");
    } else {
        bool isAlpha = false;
        for (int x = 0; x < ipAddress.length(); x++) {
            isAlpha |= isalpha(ipAddress[x]);
        }
        
        if (isAlpha) {
            struct hostent* uhost = gethostbyname(ipAddress.c_str());
            if (!uhost) {
                LogErr(VB_CHANNELOUT,
                       "Error looking up E1.31 hostname: %s\n",
                       ipAddress.c_str());
                valid = false;
            }
            
            anAddress.sin_addr.s_addr = *((unsigned long*)uhost->h_addr);
        } else {
            anAddress.sin_addr.s_addr = inet_addr(ipAddress.c_str());
        }
    }
    memcpy(anBuffer, ArtNetHeader, ARTNET_HEADER_LENGTH);
    
    anBuffer[ARTNET_UNIVERSE_INDEX]   = (char)(universe%256);
    anBuffer[ARTNET_UNIVERSE_INDEX+1] = (char)(universe/256);
    
    anBuffer[ARTNET_LENGTH_INDEX]     = (char)(channelCount/256);
    anBuffer[ARTNET_LENGTH_INDEX+1]   = (char)(channelCount%256);

    
    // use scatter/gather for the packet.   One IOV will contain
    // the header, the second will point into the raw channel data
    // and will be set at output time.   This avoids any memcpy.
    anIovecs[0].iov_base = anBuffer;
    anIovecs[0].iov_len = ARTNET_HEADER_LENGTH;
    anIovecs[1].iov_base = nullptr;
    anIovecs[1].iov_len = channelCount;
}

ArtNetOutputData::~ArtNetOutputData() {
}

bool ArtNetOutputData::IsPingable() {
    return type == ARTNET_TYPE_UNICAST;
}


void ArtNetOutputData::PrepareData(unsigned char *channelData) {
    if (valid && active) {
        anBuffer[ARTNET_SEQUENCE_INDEX] = sequenceNumber;
        anIovecs[1].iov_base = (void*)(channelData + startChannel - 1);
    }
    sequenceNumber++;
}
void ArtNetOutputData::CreateMessages(std::vector<struct mmsghdr> &udpMsgs) {
    if (valid && active && type == ARTNET_TYPE_UNICAST) {
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        
        msg.msg_hdr.msg_name = &anAddress;
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        msg.msg_hdr.msg_iov = anIovecs;
        msg.msg_hdr.msg_iovlen = 2;
        msg.msg_len = channelCount + ARTNET_HEADER_LENGTH;
        udpMsgs.push_back(msg);
    }
}
void ArtNetOutputData::CreateBroadcastMessages(std::vector<struct mmsghdr> &bMsgs) {
    if (valid && active && type == ARTNET_TYPE_BROADCAST) {
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        
        msg.msg_hdr.msg_name = &anAddress;
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        msg.msg_hdr.msg_iov = anIovecs;
        msg.msg_hdr.msg_iovlen = 2;
        msg.msg_len = channelCount + ARTNET_HEADER_LENGTH;
        bMsgs.push_back(msg);
    }
}
void ArtNetOutputData::AddPostDataMessages(std::vector<struct mmsghdr> &bMsgs) {
    if (valid && active) {
        for (auto msg : bMsgs) {
            if (msg.msg_hdr.msg_iov == &ArtNetSyncIovecs) {
                //already added, skip
                return;
            }
        }
        
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        
        msg.msg_hdr.msg_name = &ArtNetSyncAddress;
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        msg.msg_hdr.msg_iov = &ArtNetSyncIovecs;
        msg.msg_hdr.msg_iovlen = 1;
        msg.msg_len = ARTNET_SYNC_PACKET_LENGTH;
        bMsgs.push_back(msg);
    }
}


void ArtNetOutputData::DumpConfig() {
    LogDebug(VB_CHANNELOUT, "ArtNet Universe: %s   %d:%d:%d:%d:%d  %s\n",
             description.c_str(),
             active,
             universe,
             startChannel,
             channelCount,
             type,
             ipAddress.c_str());
}
