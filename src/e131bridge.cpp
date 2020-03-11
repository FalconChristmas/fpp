/*
 *   E131 bridge for Falcon Player (FPP)
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

#include <errno.h> 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ifaddrs.h>

#include <fstream>
#include <sstream>
#include <string>

#include <jsoncpp/json/json.h>

#include "common.h"
#include "e131bridge.h"
#include "log.h"
#include "Sequence.h"
#include "settings.h"
#include "command.h"
#include "Warnings.h"
#include "fppversion.h"

#include "e131defs.h"
#include "channeloutput/channeloutput.h"
#include "channeloutput/channeloutputthread.h"
#include "channeloutput/DDP.h"
#include "channeloutput/Universe.h"

#define BRIDGE_INVALID_UNIVERSE_INDEX 0xFFFFFF

struct sockaddr_in addr;
socklen_t addrlen;

int bridgeSock = -1;
int ddpSock = -1;
int artnetSock = -1;


#define MAX_MSG 48
#define BUFSIZE 1500
struct mmsghdr msgs[MAX_MSG];
struct iovec iovecs[MAX_MSG];
unsigned char buffers[MAX_MSG][BUFSIZE+1];
struct sockaddr_in inAddress[MAX_MSG];

unsigned int UniverseCache[65536];


std::vector<UniverseEntry> InputUniverses;
int InputUniverseCount;

static unsigned long ddpBytesReceived = 0;
static unsigned long ddpPacketsReceived = 0;
static unsigned long ddpErrors = 0;

static long ddpLastSequence = 0;
static long ddpLastChannel = 0;
static unsigned long ddpMinChannel = 0xFFFFFFF;
static unsigned long ddpMaxChannel = 0;

static unsigned long e131Errors = 0;
static unsigned long e131SyncPackets = 0;
static UniverseEntry unknownUniverse;




// prototypes for functions below
bool Bridge_StoreData(char *bridgeBuffer);
bool Bridge_StoreDDPData(char *bridgeBuffer);
bool Bridge_StoreArtNetData(char *bridgeBuffer);
int Bridge_GetIndexFromUniverseNumber(int universe);
void InputUniversesPrint();


/*
 *
 */
void LoadInputUniversesFromFile(void)
{
	FILE *fp;
	char buf[512];
	char *s;
	InputUniverseCount=0;
	char active =0;
	char filename[1024];

	strcpy(filename, getMediaDirectory());
	strcat(filename, "/config/ci-universes.json");

	LogDebug(VB_E131BRIDGE, "Opening File Now %s\n", filename);

	if (!FileExists(filename)) {
		LogErr(VB_E131BRIDGE, "Universe file %s does not exist\n",
			filename);
		return;
	}

	Json::Value root;
    if (!LoadJsonFromFile(filename, root)) {
		LogErr(VB_E131BRIDGE, "Error parsing %s\n", filename);
		return;
	}

	Json::Value outputs = root["channelInputs"];

	std::string type;
	int start = 0;
	int count = 0;

	for (int c = 0; c < outputs.size(); c++) {
		if (outputs[c]["type"].asString() != "universes")
			continue;

		if (outputs[c]["enabled"].asInt() == 0)
			continue;

		Json::Value univs = outputs[c]["universes"];

		for (int i = 0; i < univs.size(); i++) {
			Json::Value u = univs[i];

			if(u["active"].asInt()) {
                int universe = u["id"].asInt();
                int count = u.get("universeCount", Json::Value(1)).asInt();
                int startChannel = u["startChannel"].asInt();
                int channelCount = u["channelCount"].asInt();
                for (int x = 0; x < count; ++x) {
                    InputUniverses.resize(InputUniverseCount + 1);
                    InputUniverses[InputUniverseCount].active = u["active"].asInt();
                    InputUniverses[InputUniverseCount].universe = universe + x;
                    InputUniverses[InputUniverseCount].startAddress = startChannel;
                    InputUniverses[InputUniverseCount].size = channelCount;
                    InputUniverses[InputUniverseCount].type = u["type"].asInt();
                    
                    switch (InputUniverses[InputUniverseCount].type) {
                        case 0: // Multicast
                            strcpy(InputUniverses[InputUniverseCount].unicastAddress,"\0");
                            break;
                        case 1: //UnicastAddress
                            strcpy(InputUniverses[InputUniverseCount].unicastAddress,
                                   u["address"].asString().c_str());
                            break;
                        case 2: // ArtNet
                        case 3: // ArtNet
                            strcpy(InputUniverses[InputUniverseCount].unicastAddress,"\0");
                            break;
                        default:
                            continue;
                    }
                    
                    InputUniverses[InputUniverseCount].priority = u["priority"].asInt();
                    startChannel += channelCount;
                    InputUniverseCount++;
                }
			}
		}
	}
}

/*
 * Read data waiting for us
 */
bool Bridge_ReceiveE131Data(void)
{
//	LogExcess(VB_E131BRIDGE, "Bridge_ReceiveData()\n");

    int msgcnt = recvmmsg(bridgeSock, msgs, MAX_MSG, 0, nullptr);
    bool sync = false;
    while (msgcnt > 0) {
        for (int x = 0; x < msgcnt; x++) {
            sync |= Bridge_StoreData((char*)buffers[x]);
        }
        msgcnt = recvmmsg(bridgeSock, msgs, MAX_MSG, 0, nullptr);
    }
    return sync;
}
bool Bridge_ReceiveDDPData(void)
{
    //    LogExcess(VB_E131BRIDGE, "Bridge_ReceiveData()\n");
    int msgcnt = recvmmsg(ddpSock, msgs, MAX_MSG, 0, nullptr);
    bool sync = false;
    while (msgcnt > 0) {
        for (int x = 0; x < msgcnt; x++) {
            sync |= Bridge_StoreDDPData((char*)buffers[x]);
        }
        msgcnt = recvmmsg(ddpSock, msgs, MAX_MSG, 0, nullptr);
    }
    return sync;
}
bool Bridge_ReceiveArtNetData(void) {
    int msgcnt = recvmmsg(artnetSock, msgs, MAX_MSG, 0, nullptr);
    bool sync = false;
    while (msgcnt > 0) {
        for (int x = 0; x < msgcnt; x++) {
            sync |= Bridge_StoreArtNetData((char*)buffers[x]);
        }
        msgcnt = recvmmsg(artnetSock, msgs, MAX_MSG, 0, nullptr);
    }
    return sync;
}

void Bridge_Initialize_Internal()
{
	LogExcess(VB_E131BRIDGE, "Bridge_Initialize()\n");

    // prepare the msg receive buffers
    memset(msgs, 0, sizeof(msgs));
    for (int i = 0; i < MAX_MSG; i++) {
        iovecs[i].iov_base         = buffers[i];
        iovecs[i].iov_len          = BUFSIZE;
        msgs[i].msg_hdr.msg_iov    = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
    }
    
	/* Initialize our Universe Index lookup cache */
    for (int i = 0; i < 65536; i++) {
		UniverseCache[i] = BRIDGE_INVALID_UNIVERSE_INDEX;
    }

	LoadInputUniversesFromFile();
	LogInfo(VB_E131BRIDGE, "Universe Count = %d\n",InputUniverseCount);
	InputUniversesPrint();
    
    
    // FIXME FIXME FIXME FIXME
    // This is a total hack to get a file descriptor greater than 2
    // because otherwise, the bind() call later will fail for some reason.
    // FIXME FIXME FIXME FIXME
    int i1 = socket(AF_INET, SOCK_DGRAM, 0);
    int i2 = socket(AF_INET, SOCK_DGRAM, 0);
    int i3 = socket(AF_INET, SOCK_DGRAM, 0);
    
    ddpSock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (ddpSock < 0) {
        LogDebug(VB_E131BRIDGE, "e131bridge DDP socket failed: %s", strerror(errno));
        exit(1);
    }
    memset((char *)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DDP_PORT);
    addrlen = sizeof(addr);
    // Bind the socket to address/port
    if (bind(ddpSock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        LogDebug(VB_E131BRIDGE, "e131bridge DDP bind failed: %s", strerror(errno));
        exit(1);
    }

    bool hasArtNet = false;
    bool hase131 = false;
    for(int i = 0; i < InputUniverseCount; i++)  {
        if (InputUniverses[i].type == E131_TYPE_MULTICAST
            || InputUniverses[i].type == E131_TYPE_UNICAST) {
            hase131 = true;
        } else if (InputUniverses[i].type == ARTNET_TYPE_BROADCAST
                   || InputUniverses[i].type == ARTNET_TYPE_UNICAST) {
            hasArtNet = true;
        }
    }
    
    if (hase131) {
        int            UniverseOctet[2];
        struct ip_mreq mreq;
        char           strMulticastGroup[16];
        /* set up socket */
        bridgeSock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        if (bridgeSock < 0) {
            LogDebug(VB_E131BRIDGE, "e131bridge socket failed: %s", strerror(errno));
            exit(1);
        }

        // FIXME, move this to /etc/sysctl.conf or our startup script
        system("sudo sysctl net/ipv4/igmp_max_memberships=512");

        memset((char *)&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(E131_DEST_PORT);
        addrlen = sizeof(addr);
        // Bind the socket to address/port
        if (bind(bridgeSock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            LogDebug(VB_E131BRIDGE, "e131bridge bind failed: %s", strerror(errno));
            exit(1);
        }
    
        //get all the addresses
        struct ifaddrs *interfaces,*tmp;
        getifaddrs(&interfaces);
        
        char address[16];
        address[0] = 0;
        // Join the multicast groups
        for(int i = 0; i < InputUniverseCount; i++)  {
            if (InputUniverses[i].type == E131_TYPE_MULTICAST) {
                UniverseOctet[0] = InputUniverses[i].universe/256;
                UniverseOctet[1] = InputUniverses[i].universe%256;
                sprintf(strMulticastGroup, "239.255.%d.%d", UniverseOctet[0],UniverseOctet[1]);
                mreq.imr_multiaddr.s_addr = inet_addr(strMulticastGroup);

                LogInfo(VB_E131BRIDGE, "Adding group %s\n", strMulticastGroup);

                // add group to groups to listen for on eth0 and wlan0 if it exists
                int multicastJoined = 0;
                tmp = interfaces;
                //loop through all the interfaces and subscribe to the group
                while (tmp) {
                    //struct sockaddr_in *sin = (struct sockaddr_in *)tmp->ifa_addr;
                    //strcpy(address, inet_ntoa(sin->sin_addr));
                    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
                        GetInterfaceAddress(tmp->ifa_name, address, NULL, NULL);
                        if (strcmp(address, "127.0.0.1")) {
                            LogDebug(VB_E131BRIDGE, "   Adding interface %s - %s\n", tmp->ifa_name, address);
                            mreq.imr_interface.s_addr = inet_addr(address);
                            if (setsockopt(bridgeSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
                                LogWarn(VB_E131BRIDGE, "   Could not setup Multicast Group for interface %s\n", tmp->ifa_name);
                            }
                            multicastJoined = 1;
                        }
                    } else if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
                        //FIXME for ipv6 multicast
                        //LogDebug(VB_E131BRIDGE, "   Inet6 interface %s\n", tmp->ifa_name);
                    }
                    tmp = tmp->ifa_next;
                }

                if (!multicastJoined) {
                    LogDebug(VB_E131BRIDGE, "  Binding to default interface\n");
                    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
                    if (setsockopt(bridgeSock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq, sizeof(mreq)) < 0) {
                        LogWarn(VB_E131BRIDGE, "   Could not setup Multicast Group\n");
                    }
                }
            }
        }
        freeifaddrs(interfaces);
    }
    
    if (hasArtNet) {
        artnetSock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        if (artnetSock < 0) {
            LogDebug(VB_E131BRIDGE, "ArtNet socket failed: %s", strerror(errno));
            exit(1);
        }
        int enable = 1;
        //need to be able to send broadcase for ArtPollReply
        setsockopt(artnetSock, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable));
        enable = 1;
        setsockopt(artnetSock, SOL_SOCKET, SO_NO_CHECK, (void*)&enable, sizeof enable);

        memset((char *)&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(0x1936); //artnet port
        addrlen = sizeof(addr);
        // Bind the socket to address/port
        if (bind(artnetSock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
            LogDebug(VB_E131BRIDGE, "ArtNet bind failed: %s", strerror(errno));
            exit(1);
        }
    }
    
	StartChannelOutputThread();
    
    if (i1 >= 0) close(i1);
    if (i2 >= 0) close(i2);
    if (i3 >= 0) close(i3);
}

bool Bridge_StoreData(char *bridgeBuffer)
{
    if ((bridgeBuffer[E131_VECTOR_INDEX] == VECTOR_ROOT_E131_DATA) &&
        (bridgeBuffer[E131_START_CODE] == 0x00)) {
        int universe = ((int)bridgeBuffer[E131_UNIVERSE_INDEX] << 8) + bridgeBuffer[E131_UNIVERSE_INDEX + 1];
        int universeIndex = Bridge_GetIndexFromUniverseNumber(universe);
        if(universeIndex != BRIDGE_INVALID_UNIVERSE_INDEX) {
            int sn = bridgeBuffer[E131_SEQUENCE_INDEX];
            if (InputUniverses[universeIndex].packetsReceived != 0) {
                if (InputUniverses[universeIndex].lastSequenceNumber == 255) {
                    // some wrap from 255 -> 1 and some from 255 -> 0, spec doesn't say which
                    if (sn != 0 && sn != 1) {
                        ++InputUniverses[universeIndex].errorPackets;
                    }
                } else if ((InputUniverses[universeIndex].lastSequenceNumber + 1) != sn) {
                    ++InputUniverses[universeIndex].errorPackets;
                }
            }
            InputUniverses[universeIndex].lastSequenceNumber = sn;
            
            memcpy((void*)(sequence->m_seqData+InputUniverses[universeIndex].startAddress-1),
                   (void*)(bridgeBuffer+E131_HEADER_LENGTH),
                   InputUniverses[universeIndex].size);
            InputUniverses[universeIndex].bytesReceived += InputUniverses[universeIndex].size;
            InputUniverses[universeIndex].packetsReceived++;
            sequence->setDataNotProcessed();
        } else {
            unknownUniverse.packetsReceived++;
            int len = bridgeBuffer[16] & 0xF;
            len <<= 8;
            len += bridgeBuffer[17];
            unknownUniverse.bytesReceived += len;
            LogDebug(VB_E131BRIDGE, "Received e1.31 data packet for unconfigured universe %d\n", universe);
        }
    } else if (bridgeBuffer[E131_VECTOR_INDEX] == VECTOR_ROOT_E131_EXTENDED) {
        if (bridgeBuffer[E131_EXTENDED_PACKET_TYPE_INDEX] == VECTOR_E131_EXTENDED_SYNCHRONIZATION) {
            e131SyncPackets++;
            return true;
        }
        e131Errors++;
        LogDebug(VB_E131BRIDGE, "Unknown e1.31 extended packet type %d\n", (int)bridgeBuffer[E131_EXTENDED_PACKET_TYPE_INDEX]);
    } else {
        e131Errors++;
        LogDebug(VB_E131BRIDGE, "Unknown e1.31 packet type %d, start code %d\n", (int)bridgeBuffer[E131_VECTOR_INDEX], (int)bridgeBuffer[E131_START_CODE]);
    }
    return false;
}
bool Bridge_StoreArtNetData(char *bridgeBuffer)  {
    
    if (bridgeBuffer[0] != 'A' || bridgeBuffer[1] != 'r' || bridgeBuffer[2] != 't' || bridgeBuffer[3] != '-'
        || bridgeBuffer[4] != 'N' || bridgeBuffer[5] != 'e' || bridgeBuffer[6] != 't' || bridgeBuffer[7] != 0
        || bridgeBuffer[11] != 0xE   //version must be 14
        ) {
        return false;
    }
    if (bridgeBuffer[9] == 0x50 && bridgeBuffer[8] == 0x00) {
        //data packet
        int sn = bridgeBuffer[12];
       
        int univ = bridgeBuffer[15];
        univ <<= 8;
        univ &= 0x7F00;
        univ |= bridgeBuffer[14];
        
        int len = bridgeBuffer[16];
        len <<= 8;
        len &= 0x7F00;
        len += bridgeBuffer[17];
        int universeIndex = Bridge_GetIndexFromUniverseNumber(univ);
        if(universeIndex != BRIDGE_INVALID_UNIVERSE_INDEX) {
            if (InputUniverses[universeIndex].packetsReceived != 0) {
                if (InputUniverses[universeIndex].lastSequenceNumber == 255) {
                    // some wrap from 255 -> 1 and some from 255 -> 0
                    if (sn != 0 && sn != 1) {
                        ++InputUniverses[universeIndex].errorPackets;
                    }
                } else if ((InputUniverses[universeIndex].lastSequenceNumber + 1) != sn) {
                    ++InputUniverses[universeIndex].errorPackets;
                }
            }
            InputUniverses[universeIndex].lastSequenceNumber = sn;
            InputUniverses[universeIndex].bytesReceived += std::min(InputUniverses[universeIndex].size, len);
            InputUniverses[universeIndex].packetsReceived++;

            memcpy((void*)(sequence->m_seqData+InputUniverses[universeIndex].startAddress-1),
                   (void*)(&bridgeBuffer[18]),
                   std::min(InputUniverses[universeIndex].size, len));
            sequence->setDataNotProcessed();
        } else {
            unknownUniverse.packetsReceived++;
            int len = bridgeBuffer[16] & 0xF;
            len <<= 8;
            len += bridgeBuffer[17];
            unknownUniverse.bytesReceived += len;
            LogDebug(VB_E131BRIDGE, "Received ArtNet data packet for unconfigured universe %d\n", univ);
        }

    } else if (bridgeBuffer[9] == 0x52 && bridgeBuffer[8] == 0x00) {
        //sync packet
        return true;
    } else if (bridgeBuffer[9] == 0x20 && bridgeBuffer[8] == 0x00) {
        //ArtNet Poll, need to send a reply
        char buf[512];
        memset(buf, 0, sizeof(buf));
        strcpy(buf, "Art-Net");
        buf[9] = 0x21; //PollReply
        buf[10] = 0; //IP
        buf[11] = 0;
        buf[12] = 0;
        buf[13] = 0;
        buf[14] = 0x36; //port
        buf[15] = 0x19;

        buf[16] = std::atoi(getFPPMajorVersion());
        buf[17] = std::atoi(getFPPMinorVersion());
        
        buf[18] = 0; //universes?
        buf[19] = 0;
        
        buf[20] = 0; //OEM value?
        buf[21] = 0;

        buf[23] = 0; //status1
        
        buf[24] = 0; //ESTA
        buf[25] = 0;
        
        std::string hostname = getSetting("HostName");
        
        if (hostname == "") {
            hostname = "FPP";
        }
        strcpy(&buf[26], hostname.c_str()); //HOSTNAME?
        strcpy(&buf[44], hostname.c_str()); //Description?
        strcpy(&buf[108], ""); //Status?
        
        buf[172] = 0;
        buf[173] = 4;
        buf[174] = 0xc0;
        buf[175] = 0xc0;
        buf[176] = 0xc0;
        buf[177] = 0xc0;
        
        buf[178] = 0x0; //input
        buf[179] = 0x0;
        buf[180] = 0x0;
        buf[181] = 0x0;
        buf[182] = 0x0; //output
        buf[183] = 0x0;
        buf[184] = 0x0;
        buf[185] = 0x0;
        
        
        char addressBuf[128];
        //get all the addresses
        struct ifaddrs *interfaces,*tmp;
        getifaddrs(&interfaces);
        tmp = interfaces;
        while (tmp) {
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
                if (strncmp("usb", tmp->ifa_name, 3) != 0
                    && strncmp("lo", tmp->ifa_name, 2) != 0
                    && tmp->ifa_addr) {
                    
                    struct sockaddr_in *sain = (struct sockaddr_in*)tmp->ifa_addr;
                    unsigned long s_addr = sain->sin_addr.s_addr;
                    buf[13] = s_addr >> 24; //IP
                    buf[12] = s_addr >> 16;
                    buf[11] = s_addr >> 8;
                    buf[10] = s_addr & 0xFF;
                    
                    struct sockaddr_in s;
                    memset(&s, '\0', sizeof(struct sockaddr_in));
                    s.sin_family = AF_INET;
                    s.sin_port = (in_port_t)htons(0x1936);
                    s.sin_addr.s_addr = htonl(INADDR_BROADCAST);
                    sendto(artnetSock, buf, 240, 0, (struct sockaddr *)&s, sizeof(struct sockaddr_in));
                }
            }
            tmp = tmp->ifa_next;
        }
        freeifaddrs(interfaces);

        
    }
    return false;
}
bool Bridge_StoreDDPData(char *bridgeBuffer)  {
    bool push = false;
    if (bridgeBuffer[3] == 1) {
        ddpPacketsReceived++;
        bool tc = bridgeBuffer[0] & DDP_TIMECODE_FLAG;
        push = bridgeBuffer[0] & DDP_PUSH_FLAG;
        
        unsigned long chan = bridgeBuffer[4];
        chan <<= 8;
        chan += bridgeBuffer[5];
        chan <<= 8;
        chan += bridgeBuffer[6];
        chan <<= 8;
        chan += bridgeBuffer[7];
        
        unsigned long len = bridgeBuffer[8] << 8;
        len += bridgeBuffer[9];
        
        int sn = bridgeBuffer[1] & 0xF;
        if (sn) {
            bool isErr = false;
            if (ddpLastSequence) {
                if (sn == 1) {
                    if (ddpLastSequence != 15) {
                        isErr = true;
                    }
                } else if ((sn - 1) != ddpLastSequence) {
                    isErr = true;
                }
            }
            if (isErr) {
                ddpErrors++;
                //printf("%d   %d    %d  %d\n", sn, ddpLastSequence, chan, ddpLastChannel);
            }
            ddpLastSequence = sn;
            ddpLastChannel = chan + len;
        }

        ddpMinChannel = std::min(ddpMinChannel, chan + 1);
        ddpMaxChannel = std::max(ddpMaxChannel, chan + len);

        int offset = tc ? 14 : 10;
        memcpy(sequence->m_seqData + chan, &bridgeBuffer[offset], len);
        sequence->setDataNotProcessed();
        ddpBytesReceived += len;
    }
    return push;
}


inline int Bridge_GetIndexFromUniverseNumber(int universe)
{
	int val = BRIDGE_INVALID_UNIVERSE_INDEX;

	if (UniverseCache[universe] != BRIDGE_INVALID_UNIVERSE_INDEX)
		return UniverseCache[universe];

	for (int index = 0; index < InputUniverseCount; index++) {
		if (universe == InputUniverses[index].universe) {
			val = index;
			UniverseCache[universe] = index;
			break;
		}
	}
	return val;
}


void Bridge_Initialize(std::map<int, std::function<bool(int)>> &callbacks) {
    Bridge_Initialize_Internal();
    if (bridgeSock > 0) {
        std::function<bool(int)> f = [] (int i) {
            return Bridge_ReceiveE131Data();
        };
        callbacks[bridgeSock] = f;
    }
    if (ddpSock > 0) {
        std::function<bool(int)> f = [] (int i) {
            return Bridge_ReceiveDDPData();
        };
        callbacks[ddpSock] = f;
    }
    if (artnetSock > 0) {
        std::function<bool(int)> f = [] (int i) {
            return Bridge_ReceiveArtNetData();
        };
        callbacks[artnetSock] = f;
    }
}

void Bridge_Shutdown(void)
{
    close(bridgeSock);
    close(ddpSock);
    close(artnetSock);
    bridgeSock = -1;
    ddpSock = -1;
}


void ResetBytesReceived()
{
	for (int i = 0; i < InputUniverseCount; i++) {
		InputUniverses[i].bytesReceived = 0;
		InputUniverses[i].packetsReceived = 0;
        InputUniverses[i].errorPackets = 0;
        InputUniverses[i].lastSequenceNumber = 0;
	}
    ddpBytesReceived = 0;
    ddpPacketsReceived = 0;
    ddpErrors = 0;
    e131Errors = 0;
}

Json::Value GetE131UniverseBytesReceived()
{
    Json::Value result;
    Json::Value universes(Json::arrayValue);

    int i;

    if (ddpBytesReceived) {
        Json::Value ddpUniverse;
        ddpUniverse["id"] = "DDP";

        if (ddpMaxChannel > ddpMinChannel) {
            std::stringstream ss;
            ss << ddpMinChannel << "-" << ddpMaxChannel;
            std::string chanRange = ss.str();
            ddpUniverse["startChannel"] = chanRange;
        } else {
            ddpUniverse["startChannel"] = "-";
        }
        
        std::stringstream ss;
        ss << ddpBytesReceived;
        std::string bytesReceived = ss.str();
        ddpUniverse["bytesReceived"] = bytesReceived;
        
        std::stringstream pr;
        pr << ddpPacketsReceived;
        std::string packetsReceived = pr.str();
        ddpUniverse["packetsReceived"] = packetsReceived;
        
        std::stringstream er;
        er << ddpErrors;
        std::string errors = er.str();
        ddpUniverse["errors"] = errors;
        universes.append(ddpUniverse);
    }

	for(i = 0; i < InputUniverseCount; i++) {
		Json::Value universe;

		universe["id"] = InputUniverses[i].universe;
		universe["startChannel"] = InputUniverses[i].startAddress;

		universe["bytesReceived"] = std::to_string(InputUniverses[i].bytesReceived);
		universe["packetsReceived"] = std::to_string(InputUniverses[i].packetsReceived);
		universe["errors"] = std::to_string(InputUniverses[i].errorPackets);

		universes.append(universe);
	}
    if (e131Errors) {
        Json::Value universe;
        
        universe["id"] = "E1.31 Errors";
        universe["startChannel"] = "-";
        universe["bytesReceived"] = "-";
        universe["packetsReceived"] = "-";
        
        std::stringstream er;
        er << e131Errors;
        std::string errors = er.str();
        universe["errors"] = errors;
        
        universes.append(universe);
    }
    if (unknownUniverse.packetsReceived) {
        Json::Value universe;
        
        universe["id"] = "Ignored";
        universe["startChannel"] = "-";
        
        std::stringstream er;
        er << e131Errors;
        std::string errors = er.str();

        std::stringstream ss;
        ss << unknownUniverse.bytesReceived;
        std::string bytesReceived = ss.str();
        universe["bytesReceived"] = bytesReceived;

        std::stringstream pr;
        pr << unknownUniverse.packetsReceived;
        std::string packetsReceived = pr.str();
        universe["packetsReceived"] = packetsReceived;
        
        universe["errors"] = "-";
        
        universes.append(universe);
    }
    if (e131SyncPackets) {
        Json::Value universe;
        
        universe["id"] = "E1.31 Sync";
        universe["startChannel"] = "-";
        universe["bytesReceived"] = "-";

        std::stringstream er;
        er << e131SyncPackets;
        std::string sync = er.str();
        universe["packetsReceived"] = sync;
        
        universe["errors"] = "-";
        
        universes.append(universe);
    }
    
	result["universes"] = universes;

	return result;
}

void InputUniversesPrint()
{
    for(int i = 0; i < InputUniverseCount; i++) {
        if (InputUniverses[i].type < 2) {
            LogDebug(VB_E131BRIDGE, "E1.31 Universe: %d:%d:%d:%d:%d  %s\n",
                      InputUniverses[i].active,
                      InputUniverses[i].universe,
                      InputUniverses[i].startAddress,
                      InputUniverses[i].size,
                      InputUniverses[i].type,
                      InputUniverses[i].unicastAddress);
        } else {
            LogDebug(VB_E131BRIDGE, "ArtNet Universe: %d:%d:%d:%d:%d  %s\n",
                     InputUniverses[i].active,
                     InputUniverses[i].universe,
                     InputUniverses[i].startAddress,
                     InputUniverses[i].size,
                     InputUniverses[i].type,
                     InputUniverses[i].unicastAddress);
        }
	}
}



void AddFakeListener(int port, const std::string &protocol,
                     std::map<int, std::function<bool(int)>> &callbacks) {
    int sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (sock < 0) {
        LogDebug(VB_E131BRIDGE, "e131bridge %s socket failed: %s", protocol.c_str(), strerror(errno));
        exit(1);
    }
    memset((char *)&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    addrlen = sizeof(addr);
    // Bind the socket to address/port
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        LogDebug(VB_E131BRIDGE, "e131bridge %s bind failed: %s", protocol.c_str(), strerror(errno));
        exit(1);
    }
    
    std::function<bool(int)> f = [sock, protocol](int i) {
        std::map<in_addr_t, std::string> errrors;
        int msgcnt = recvmmsg(sock, msgs, MAX_MSG, 0, nullptr);
        while (msgcnt > 0) {
            for (int x = 0; x < msgcnt; x++) {
                struct in_addr i = inAddress[x].sin_addr;
                in_addr_t at = i.s_addr;
                if (errrors[at] == "") {
                    std::string ne = "Received " + protocol + " data from " + inet_ntoa(inAddress[x].sin_addr);
                    LogDebug(VB_E131BRIDGE, "%s\n", ne.c_str());
                    WarningHolder::AddWarningTimeout(ne, 30);
                    errrors[at] = ne;
                }
            }
            msgcnt = recvmmsg(sock, msgs, MAX_MSG, 0, nullptr);
        }
        return false;
    };
    callbacks[sock] = f;
}
void Fake_Bridge_Initialize(std::map<int, std::function<bool(int)>> &callbacks) {
    // prepare the msg receive buffers
    memset(msgs, 0, sizeof(msgs));
    for (int i = 0; i < MAX_MSG; i++) {
        iovecs[i].iov_base         = buffers[i];
        iovecs[i].iov_len          = BUFSIZE;
        msgs[i].msg_hdr.msg_iov    = &iovecs[i];
        msgs[i].msg_hdr.msg_iovlen = 1;
        msgs[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
        msgs[i].msg_hdr.msg_name = &inAddress[i];
    }
    AddFakeListener(DDP_PORT, "DDP", callbacks);
    AddFakeListener(E131_DEST_PORT, "E1.31", callbacks);
}
