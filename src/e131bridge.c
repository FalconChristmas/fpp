/*
 *   E131 bridge for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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


#define MAX_MSG 48
//DMX packet is under 700 bytes
#define BUFSIZE 700
struct mmsghdr msgs[MAX_MSG];
struct iovec iovecs[MAX_MSG];
unsigned char buffers[MAX_MSG][BUFSIZE+1];

unsigned int   UniverseCache[65536];


UniverseEntry InputUniverses[MAX_UNIVERSE_COUNT];
int InputUniverseCount;

// prototypes for functions below
void Bridge_StoreData(int universe, char *bridgeBuffer);
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
				InputUniverses[InputUniverseCount].startAddress = u["startChannel"].asInt() - 1;
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
void Bridge_ReceiveData(void)
{
//	LogExcess(VB_E131BRIDGE, "Bridge_ReceiveData()\n");

	int universe;

    int msgcnt = recvmmsg(bridgeSock, msgs, MAX_MSG, 0, nullptr);
    while (msgcnt > 0) {
        for (int x = 0; x < msgcnt; x++) {
            universe = ((int)buffers[x][E131_UNIVERSE_INDEX] << 8) + buffers[x][E131_UNIVERSE_INDEX + 1];
            Bridge_StoreData(universe, (char*)buffers[x]);
        }
        msgcnt = recvmmsg(bridgeSock, msgs, MAX_MSG, 0, nullptr);
    }
}

int Bridge_Initialize(void)
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

	return bridgeSock;
}

void Bridge_Shutdown(void)
{
	close(bridgeSock);
	bridgeSock = -1;
}

void Bridge_StoreData(int universe, char *bridgeBuffer)
{
	int universeIndex = Bridge_GetIndexFromUniverseNumber(universe);
	if(universeIndex!=BRIDGE_INVALID_UNIVERSE_INDEX)
	{
		memcpy((void*)(sequence->m_seqData+InputUniverses[universeIndex].startAddress-1),
			   (void*)(bridgeBuffer+E131_HEADER_LENGTH),
			   InputUniverses[universeIndex].size);
		InputUniverses[universeIndex].bytesReceived+=InputUniverses[universeIndex].size;
	}
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
	}
}

void WriteBytesReceivedFile()
{
	int i;
	FILE *file;
	file = fopen((const char *)getBytesFile(), "w");
	for(i=0;i<InputUniverseCount;i++)
	{
		if(i==InputUniverseCount-1)
		{
			fprintf(file, "%d,%d,%d,",InputUniverses[i].universe,InputUniverses[i].startAddress,InputUniverses[i].bytesReceived);
		}
		else
		{
			fprintf(file, "%d,%d,%d,\n",InputUniverses[i].universe,InputUniverses[i].startAddress,InputUniverses[i].bytesReceived);
		}
	}
	fclose(file);
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

