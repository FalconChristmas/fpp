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
 
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "channeloutput.h"
#include "channeloutputthread.h"
#include "common.h"
#include "E131.h"
#include "e131bridge.h"
#include "log.h"
#include "PixelOverlay.h"
#include "Sequence.h"
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


extern UniverseEntry universes[MAX_UNIVERSE_COUNT];
extern int UniverseCount;

// prototypes for functions below
void Bridge_StoreData(int universe, char *bridgeBuffer);
int Bridge_GetIndexFromUniverseNumber(int universe);

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

	LoadUniversesFromFile();
	LogInfo(VB_E131BRIDGE, "Universe Count = %d\n",UniverseCount);
	UniversesPrint();

	int            UniverseOctet[2];
	int            i;
	struct ip_mreq mreq;
	char           strMulticastGroup[16];

	/* set up socket */
	bridgeSock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
	if (bridgeSock < 0) {
		perror("e131bridge socket");
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
		perror("e131bridge bind");
		exit(1);
	}

	char address[16];
	// Join the multicast groups
	for(i=0;i<UniverseCount;i++)
	{
		if(universes[i].type == E131_TYPE_MULTICAST)
		{
			UniverseOctet[0] = universes[i].universe/256;
			UniverseOctet[1] = universes[i].universe%256;
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
		memcpy((void*)(sequence->m_seqData+universes[universeIndex].startAddress-1),
			   (void*)(bridgeBuffer+E131_HEADER_LENGTH),
			   universes[universeIndex].size);
		universes[universeIndex].bytesReceived+=universes[universeIndex].size;
	}
}

inline int Bridge_GetIndexFromUniverseNumber(int universe)
{
	int index;
	int val=BRIDGE_INVALID_UNIVERSE_INDEX;

	if (UniverseCache[universe] != BRIDGE_INVALID_UNIVERSE_INDEX)
		return UniverseCache[universe];

	for(index=0;index<UniverseCount;index++)
	{
		if(universe == universes[index].universe)
		{
			val = index;
			UniverseCache[universe] = index;
			break;
		}
	}

	return val;
}

