#include "e131bridge.h"
#include "log.h"
#include "E131.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdio.h>


char unicastSocketCreated = 0;

struct sockaddr_in addr;
int addrlen, sock, cnt;
fd_set active_fd_set, read_fd_set;
struct timeval timeout;
struct ip_mreq mreq;
char bridgeBuffer[10000];

char strMulticastGroup[16];
char BridgeRunning=0;

extern UniverseEntry universes[MAX_UNIVERSE_COUNT];
extern int UniverseCount;
extern char fileData[65536];

	void Bridge_Process()
	{
		int universe;
		Bridge_Initialize();
    while(BridgeRunning) 
		{
		  Commandproc();
			read_fd_set = active_fd_set;
			if (select(FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout) < 0)
      {
       	//LogWrite("Select failed\n");
       	return 0;
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
	    usleep(5);
		}
	}

	void Bridge_Initialize()
	{
		LoadUniversesFromFile();
		LogWrite("Universe Count = %d\n",UniverseCount);
		Bridge_InitializeSockets();
		BridgeRunning = 1;
		InitializePixelnetDMX();
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
				LogWrite("Adding group %s\n",  strMulticastGroup);       
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
			memcpy((void *)(fileData+universes[universeIndex].startAddress-1),
			       (void*)(bridgeBuffer+E131_HEADER_LENGTH),
						  universes[universeIndex].size);
			//LogWrite("Storing StartAddress = %d size = %d\n",universes[universeIndex].startAddress,universes[universeIndex].size);
		}
		if(universe == universes[UniverseCount-1].universe)
//		if(universe == 2)
		{
			SendPixelnetDMX(0);
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




	
