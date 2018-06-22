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

#include <fstream>
#include <sstream>
#include <string>

#include <jsoncpp/json/json.h>

#include "channeloutput.h"
#include "channeloutputthread.h"
#include "common.h"
#include "DDP.h"
#include "e131bridge.h"
#include "log.h"
#include "PixelOverlay.h"
#include "Sequence.h"
#include "settings.h"
#include "command.h"
#include "Universe.h"

#define BRIDGE_INVALID_UNIVERSE_INDEX 0xFFFFFF

struct sockaddr_in addr;
socklen_t addrlen;
int bridgeSock = -1;
int ddpSock = -1;


#define MAX_MSG 48
#define BUFSIZE 1500
struct mmsghdr msgs[MAX_MSG];
struct iovec iovecs[MAX_MSG];
unsigned char buffers[MAX_MSG][BUFSIZE+1];

unsigned int UniverseCache[65536];


UniverseEntry InputUniverses[MAX_UNIVERSE_COUNT];
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

	LogDebug(VB_CHANNELOUT, "Opening File Now %s\n", filename);

	if (!FileExists(filename))
	{
		LogErr(VB_CHANNELOUT, "Universe file %s does not exist\n",
			filename);
		return;
	}

	Json::Value root;
    Json::Reader reader;
	std::ifstream t(filename);
	std::stringstream buffer;

	buffer << t.rdbuf();

	std::string config = buffer.str();

	bool success = reader.parse(buffer.str(), root);
	if (!success)
	{
		LogErr(VB_CHANNELOUT, "Error parsing %s\n", filename);
		return;
	}

	Json::Value outputs = root["channelInputs"];

	std::string type;
	int start = 0;
	int count = 0;

	for (int c = 0; c < outputs.size(); c++)
	{
		if (outputs[c]["type"].asString() != "universes")
			continue;

		if (outputs[c]["enabled"].asInt() == 0)
			continue;

		Json::Value univs = outputs[c]["universes"];

		for (int i = 0; i < univs.size(); i++)
		{
			Json::Value u = univs[i];

			if(u["active"].asInt())
			{
				InputUniverses[InputUniverseCount].active = u["active"].asInt();
				InputUniverses[InputUniverseCount].universe = u["id"].asInt();
				InputUniverses[InputUniverseCount].startAddress = u["startChannel"].asInt();
				InputUniverses[InputUniverseCount].size = u["channelCount"].asInt();
				InputUniverses[InputUniverseCount].type = u["type"].asInt();

				switch (InputUniverses[InputUniverseCount].type) {
					case 0: // Multicast
							strcpy(InputUniverses[InputUniverseCount].unicastAddress,"\0");
							break;
					case 1: //UnicastAddress
							strcpy(InputUniverses[InputUniverseCount].unicastAddress,
								u["address"].asString().c_str());
							break;
					default: // ArtNet
							continue;
				}
	
				InputUniverses[InputUniverseCount].priority = u["priority"].asInt();

			    InputUniverseCount++;
			}
		}
	}

	InputUniversesPrint();
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

void Bridge_Initialize(int &eSock, int &dSock)
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
	for (int i = 0; i < 65536; i++)
		UniverseCache[i] = BRIDGE_INVALID_UNIVERSE_INDEX;

	LoadInputUniversesFromFile();
	LogInfo(VB_E131BRIDGE, "Universe Count = %d\n",InputUniverseCount);
	InputUniversesPrint();
    
    ddpSock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (ddpSock < 0) {
        LogDebug(VB_E131BRIDGE, "e131bridge DDP socket failed: %s", strerror(errno));
        exit(1);
    }
    bzero((char *)&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DDP_PORT);
    addrlen = sizeof(addr);
    // Bind the socket to address/port
    if (bind(ddpSock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        LogDebug(VB_E131BRIDGE, "e131bridge DDP bind failed: %s", strerror(errno));
        exit(1);
    }

	int            UniverseOctet[2];
	int            i;
	struct ip_mreq mreq;
	char           strMulticastGroup[16];

	// FIXME FIXME FIXME FIXME
	// This is a total hack to get a file descriptor greater than 2
	// because otherwise, the bind() call later will fail for some reason.
	// FIXME FIXME FIXME FIXME
	i = socket(AF_INET, SOCK_DGRAM, 0);
	i = socket(AF_INET, SOCK_DGRAM, 0);
	i = socket(AF_INET, SOCK_DGRAM, 0);

	/* set up socket */
	bridgeSock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if (bridgeSock < 0) {
		LogDebug(VB_E131BRIDGE, "e131bridge socket failed: %s", strerror(errno));
		exit(1);
	}

	// FIXME, move this to /etc/sysctl.conf or our startup script
	system("sudo sysctl net/ipv4/igmp_max_memberships=512");

	bzero((char *)&addr, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(E131_DEST_PORT);
	addrlen = sizeof(addr);
	// Bind the socket to address/port
	if (bind(bridgeSock, (struct sockaddr *) &addr, sizeof(addr)) < 0) 
	{
		LogDebug(VB_E131BRIDGE, "e131bridge bind failed: %s", strerror(errno));
		exit(1);
	}
    
	char address[16];
	// Join the multicast groups
	for(i=0;i<InputUniverseCount;i++)
	{
		if(InputUniverses[i].type == E131_TYPE_MULTICAST)
		{
			UniverseOctet[0] = InputUniverses[i].universe/256;
			UniverseOctet[1] = InputUniverses[i].universe%256;
			sprintf(strMulticastGroup, "239.255.%d.%d", UniverseOctet[0],UniverseOctet[1]);
			mreq.imr_multiaddr.s_addr = inet_addr(strMulticastGroup);

			LogInfo(VB_E131BRIDGE, "Adding group %s\n", strMulticastGroup);

			// add group to groups to listen for on eth0 and wlan0 if it exists
			int multicastJoined = 0;

			GetInterfaceAddress("eth0", address, NULL, NULL);
			if (strcmp(address, "127.0.0.1"))
			{
				LogDebug(VB_E131BRIDGE, "binding to eth0: '%s'\n", address);
				mreq.imr_interface.s_addr = inet_addr(address);
				if (setsockopt(bridgeSock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq, sizeof(mreq)) < 0) 
				{
					perror("e131bridge setsockopt mreq eth0");
					exit(1);
				}
				multicastJoined = 1;
			}

			// FIXME, need to handle other interface names
			GetInterfaceAddress("wlan0", address, NULL, NULL);
			if (strcmp(address, "127.0.0.1"))
			{
				LogDebug(VB_E131BRIDGE, "binding to wlan0: '%s'\n", address);
				mreq.imr_interface.s_addr = inet_addr(address);
				if (setsockopt(bridgeSock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq, sizeof(mreq)) < 0) 
				{
					perror("e131bridge setsockopt mreq wlan0");
					exit(1);
				}
				multicastJoined = 1;
			}

			if (!multicastJoined)
			{
				LogDebug(VB_E131BRIDGE, "binding to default interface\n");
				mreq.imr_interface.s_addr = htonl(INADDR_ANY);
				if (setsockopt(bridgeSock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq, sizeof(mreq)) < 0) 
				{
					perror("e131bridge setsockopt mreq generic");
					exit(1);
				}
			}
		}
	}

	StartChannelOutputThread();

    eSock = bridgeSock;
    dSock = ddpSock;
}

void Bridge_Shutdown(void)
{
    close(bridgeSock);
    close(ddpSock);
    bridgeSock = -1;
    ddpSock = -1;
}

bool Bridge_StoreData(char *bridgeBuffer)
{
    if (bridgeBuffer[E131_VECTOR_INDEX] == VECTOR_ROOT_E131_DATA) {
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
        } else {
            unknownUniverse.packetsReceived++;
            int len = bridgeBuffer[16] & 0xF;
            len <<= 8;
            len += bridgeBuffer[17];
            unknownUniverse.bytesReceived += len;
            LogDebug(VB_E131BRIDGE, "Received data packet for unconfigured universe %d\n", universe);
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
        LogDebug(VB_E131BRIDGE, "Unknown e1.31 packet type %d\n", (int)bridgeBuffer[E131_VECTOR_INDEX]);
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
        
        ddpBytesReceived += len;
    }
    return push;
}


inline int Bridge_GetIndexFromUniverseNumber(int universe)
{
	int index;
	int val=BRIDGE_INVALID_UNIVERSE_INDEX;

	if (UniverseCache[universe] != BRIDGE_INVALID_UNIVERSE_INDEX)
		return UniverseCache[universe];

	for(index=0;index<InputUniverseCount;index++)
	{
		if(universe == InputUniverses[index].universe)
		{
			val = index;
			UniverseCache[universe] = index;
			break;
		}
	}

	return val;
}

void ResetBytesReceived()
{
	int i;

	for(i=0;i<InputUniverseCount;i++)
	{
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

	for(i = 0; i < InputUniverseCount; i++)
	{
		Json::Value universe;

		universe["id"] = InputUniverses[i].universe;
		universe["startChannel"] = InputUniverses[i].startAddress;

		// FIXME, use to_string on Squeeze
		//universe["bytesReceived"] = std::to_string(InputUniverses[i].bytesReceived);
		std::stringstream ss;
		ss << InputUniverses[i].bytesReceived;
		std::string bytesReceived = ss.str();
		universe["bytesReceived"] = bytesReceived;

		// FIXME, use to_string on Squeeze
		//universe["packetsReceived"] = std::to_string(InputUniverses[i].packetsReceived);
		std::stringstream pr;
		pr << InputUniverses[i].packetsReceived;
		std::string packetsReceived = pr.str();
		universe["packetsReceived"] = packetsReceived;
        
        std::stringstream er;
        er << InputUniverses[i].errorPackets;
        std::string errors = er.str();
        universe["errors"] = errors;

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
	int i=0;
	int h;

	for(i=0;i<InputUniverseCount;i++)
	{
		LogDebug(VB_CHANNELOUT, "E1.31 Universe: %d:%d:%d:%d:%d  %s\n",
				  InputUniverses[i].active,
				  InputUniverses[i].universe,
				  InputUniverses[i].startAddress,
				  InputUniverses[i].size,
				  InputUniverses[i].type,
				  InputUniverses[i].unicastAddress);
	}
}

