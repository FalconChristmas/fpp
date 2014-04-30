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

#include "channeloutput/channeloutput.h"
#include "channeloutput/E131.h"
#include "e131bridge.h"
#include "log.h"
#include "sequence.h"


char unicastSocketCreated = 0;

struct sockaddr_in addr;
int addrlen, sock, cnt;
fd_set active_fd_set, read_fd_set;
struct timeval timeout;
struct ip_mreq mreq;
char bridgeBuffer[10000];

char strMulticastGroup[16];

extern int runMainFPPDLoop;

extern UniverseEntry universes[MAX_UNIVERSE_COUNT];
extern int UniverseCount;

	void Bridge_Process()
	{
		int universe;
		Bridge_Initialize();
    while (runMainFPPDLoop) 
		{
		  Commandproc();
			read_fd_set = active_fd_set;
			if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout) < 0)
      {
       	LogErr(VB_E131BRIDGE, "Select failed\n");
       	return;
      }
			if (FD_ISSET (sock, &read_fd_set))
			{
				cnt = recvfrom(sock, bridgeBuffer, sizeof(bridgeBuffer), 0, (struct sockaddr *) &addr, &addrlen);
				if (cnt >= 0) 
				{
					universe = ((int)bridgeBuffer[E131_UNIVERSE_INDEX] * 256) + bridgeBuffer[E131_UNIVERSE_INDEX+1];
					Bridge_StoreData(universe);
				} 
			}
	    usleep(250);
		}

    LogInfo(VB_E131BRIDGE, "Main Bridge Process Loop complete, shutting down.\n");
	}

	void Bridge_Initialize()
	{
		LoadUniversesFromFile();
		LogInfo(VB_E131BRIDGE, "Universe Count = %d\n",UniverseCount);
		Bridge_InitializeSockets();
	}

	
void Bridge_InitializeSockets()
{
		int UniverseOctet[2];
		int i;
		UniversesPrint();

   	/* set up socket */
   	sock = socket(AF_INET, SOCK_DGRAM, 0);
		// Set file decriptor
   	if (sock < 0) {
   	  perror("socket");
    	 exit(1);
   	}
		system("sudo sysctl net/ipv4/igmp_max_memberships=150");

   	bzero((char *)&addr, sizeof(addr));
   	addr.sin_family = AF_INET;
   	addr.sin_addr.s_addr = htonl(INADDR_ANY);
   	addr.sin_port = htons(E131_DEST_PORT);
   	addrlen = sizeof(addr);
		// Bind the socket to address/port
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) 
		{        
      perror("bind");
	 		exit(1);
    }    
		// Receive multicast from anywhere		
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);         
		// Join the multicast groups
		for(i=0;i<UniverseCount;i++)
		{
			if(universes[i].type == E131_TYPE_MULTICAST)
			{
				UniverseOctet[0] = universes[i].universe/256;
				UniverseOctet[1] = universes[i].universe%256;
				sprintf(strMulticastGroup, "239.255.%d.%d", UniverseOctet[0],UniverseOctet[1]);
				mreq.imr_multiaddr.s_addr = inet_addr(strMulticastGroup);
				LogInfo(VB_E131BRIDGE, "Adding group %s\n",  strMulticastGroup);       
				// add group to groups to listen for
				if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,&mreq, sizeof(mreq)) < 0) 
				{
					perror("setsockopt mreq");
					exit(1);
				}         
			}
    }
		FD_ZERO (&active_fd_set);
		FD_SET (sock, &active_fd_set);
		timeout.tv_sec = 0;
    timeout.tv_usec = 5;

  }
	
	void Bridge_StoreData(int universe)
	{
		int universeIndex = Bridge_GetIndexFromUniverseNumber(universe);
		if(universeIndex!=BRIDGE_INVALID_UNIVERSE_INDEX)
		{
			memcpy((void *)(seqData+universes[universeIndex].startAddress-1),
			       (void*)(bridgeBuffer+E131_HEADER_LENGTH),
						  universes[universeIndex].size);
			universes[universeIndex].bytesReceived+=universes[universeIndex].size;
			LogDebug(VB_CHANNELDATA, "Storing StartAddress = %d size = %d\n",
				universes[universeIndex].startAddress,universes[universeIndex].size);
		}
		if(universe == universes[UniverseCount-1].universe)
		{
			SendSequenceData();
		}
	}

	int Bridge_GetIndexFromUniverseNumber(int universe)
	{	
		int index;
		int val=BRIDGE_INVALID_UNIVERSE_INDEX;
		for(index=0;index<UniverseCount;index++)
		{
			if(universe == universes[index].universe)
			{
				val = index;
				break;
			} 
		}
		return val;
	}




	
