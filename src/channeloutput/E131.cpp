/*
 *   E131 output handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
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
#include <chrono>
#include <vector>
#include <list>

#include <fstream>
#include <sstream>
#include <string>

#include <jsoncpp/json/json.h>

#include "channeloutput.h"
#include "channeloutputthread.h"
#include "common.h"
#include "e131defs.h"
#include "E131.h"
#include "FPD.h"
#include "fpp.h"
#include "log.h"
#include "settings.h"
#include "Universe.h"
#include "ping.h"


const unsigned char E131header[] = {
	0x00,0x10,0x00,0x00,0x41,0x53,0x43,0x2d,0x45,0x31,0x2e,0x31,0x37,0x00,0x00,0x00,
	0x72,0x6e,0x00,0x00,0x00,0x04,'F','A','L','C','O','N',' ','F','P','P',
	0x00,0x00,0x00,0x00,0x00,0x00,0x72,0x58,0x00,0x00,0x00,0x02,'F','A','L','C',
	'O','N','C','H','R','I','S','T','M','A','S','.','C','O','M',' ',
	'B','Y',' ','D','P','I','T','T','S',' ','A','N','D',' ','M','Y',
	'K','R','O','F','T',' ','F','A','L','C','O','N',' ','P','I',' ',
	'P','L','A','Y','E','R',0x00,0x00,0x00,0x00,0x00,0x00,0x64,0x00,0x00,0x00,
	0x00,0x00,0x01,0x72,0x0b,0x02,0xa1,0x00,0x00,0x00,0x01,0x02,0x01,0x00
	};



E131OutputData::E131OutputData(const Json::Value &config)
: UDPOutputData(config), E131sequenceNumber(1), universeCount(1) {
    memset((char *) &e131Address, 0, sizeof(sockaddr_in));
    e131Address.sin_family = AF_INET;
    e131Address.sin_port = htons(E131_DEST_PORT);

    universe = config["id"].asInt();
    priority = config["priority"].asInt();
    if (config.isMember("universeCount")) {
        universeCount = config["universeCount"].asInt();
    }
    if (universeCount < 1) {
        universeCount = 1;
    }
    switch (type) {
        case 0: // Multicast
            ipAddress = "";
            break;
        case 1: //UnicastAddress
            ipAddress = config["address"].asString();
            break;
    }
    
    
    if (type == E131_TYPE_MULTICAST) {
        int UniverseOctet[2];
        char sAddress[32];

        UniverseOctet[0] = universe/256;
        UniverseOctet[1] = universe%256;
        sprintf(sAddress, "239.255.%d.%d", UniverseOctet[0],UniverseOctet[1]);
        e131Address.sin_addr.s_addr = inet_addr(sAddress);
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
            } else {
                e131Address.sin_addr.s_addr = *((unsigned long*)uhost->h_addr);
            }
        } else {
            e131Address.sin_addr.s_addr = inet_addr(ipAddress.c_str());
        }
    }
    
    e131Iovecs.resize(universeCount * 2);
    e131Headers.resize(universeCount);
    for (int x = 0; x < universeCount; x++) {
        unsigned char *e131Buffer = (unsigned char *)malloc(E131_HEADER_LENGTH);
        e131Headers[x] = e131Buffer;
        memcpy(e131Buffer, E131header, E131_HEADER_LENGTH);
        
        int uni = universe + x;
        e131Buffer[E131_PRIORITY_INDEX] = priority;
        e131Buffer[E131_UNIVERSE_INDEX] = (char)(uni/256);
        e131Buffer[E131_UNIVERSE_INDEX+1] = (char)(uni%256);
        
        // Property Value Count
        e131Buffer[E131_COUNT_INDEX] = ((channelCount+1)/256);
        e131Buffer[E131_COUNT_INDEX+1] = ((channelCount+1)%256);
        
        // RLP Protocol flags and length
        int count = 638 - 16 - (512 - (channelCount));
        e131Buffer[E131_RLP_COUNT_INDEX] = (count/256)+0x70;
        e131Buffer[E131_RLP_COUNT_INDEX+1] = count%256;
        
        // Framing Protocol flags and length
        count = 638 - 38 - (512 - (channelCount));
        e131Buffer[E131_FRAMING_COUNT_INDEX] = (count/256)+0x70;
        e131Buffer[E131_FRAMING_COUNT_INDEX+1] = count%256;
        
        // DMP Protocol flags and length
        count = 638 - 115 - (512 - (channelCount));
        e131Buffer[E131_DMP_COUNT_INDEX] = (count/256)+0x70;
        e131Buffer[E131_DMP_COUNT_INDEX+1] = count%256;

        // use scatter/gather for the packet.   One IOV will contain
        // the header, the second will point into the raw channel data
        // and will be set at output time.   This avoids any memcpy.
        e131Iovecs[x * 2].iov_base = e131Buffer;
        e131Iovecs[x * 2].iov_len = E131_HEADER_LENGTH;
        e131Iovecs[x * 2 + 1].iov_base = nullptr;
        e131Iovecs[x * 2 + 1].iov_len = channelCount;
    }
}

E131OutputData::~E131OutputData() {
    for (auto a : e131Headers) {
        free(a);
    }
}

bool E131OutputData::IsPingable() {
    return type == 1;
}

void E131OutputData::PrepareData(unsigned char *channelData) {
    if (valid && active) {
        unsigned char *cur = channelData + startChannel - 1;
        for (int x = 0; x < universeCount; x++) {
            e131Headers[x][E131_SEQUENCE_INDEX] = E131sequenceNumber;
            e131Iovecs[x * 2 + 1].iov_base = (void*)cur;
            cur += channelCount;
        }
    }
    E131sequenceNumber++;
}
void E131OutputData::CreateMessages(std::vector<struct mmsghdr> &ipMsgs) {
    if (valid && active) {
        for (int x = 0; x < universeCount; x++) {
            struct mmsghdr msg;
            memset(&msg, 0, sizeof(msg));
            
            msg.msg_hdr.msg_name = &e131Address;
            msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
            msg.msg_hdr.msg_iov = &e131Iovecs[x * 2];
            msg.msg_hdr.msg_iovlen = 2;
            msg.msg_len = channelCount + E131_HEADER_LENGTH;
            ipMsgs.push_back(msg);
        }
    }
}
void E131OutputData::GetRequiredChannelRange(int &min, int & max) {
    min = startChannel - 1;
    max = startChannel + (channelCount * universeCount) - 1;
}

void E131OutputData::DumpConfig() {
    LogDebug(VB_CHANNELOUT, "E1.31 Universe: %s   %d:%d:%d:%d:%d:%d  %s\n",
             description.c_str(),
             active,
             universe,
             startChannel,
             channelCount,
             type,
             universeCount,
             ipAddress.c_str());
}
