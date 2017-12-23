/*
 *   ArtNet output handler for Falcon Pi Player (FPP)
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

#include "channeloutput.h"
#include "channeloutputthread.h"
#include "common.h"
#include "ArtNet.h"
#include "FPD.h"
#include "fpp.h"
#include "log.h"
#include "settings.h"
#include "Universe.h"


#define MAX_ARTNET_UNIVERSE_COUNT    512
#define ARTNET_HEADER_LENGTH         18
#define ARTNET_SYNC_PACKET_LENGTH    14

#define ARTNET_DEST_PORT        6454
#define ARTNET_SOURCE_PORT      58302
#define ARTNET_SYNC_SOURCE_PORT 58303

#define ARTNET_SEQUENCE_INDEX   12
#define ARTNET_UNIVERSE_INDEX   14
#define ARTNET_LENGTH_INDEX     16

struct sockaddr_in    ArtNetLclAddress;
struct sockaddr_in    ArtNetLclSyncAddress;
struct sockaddr_in    ArtNetAddress[MAX_ARTNET_UNIVERSE_COUNT+1];
int                   ArtNetSendSocket;
int                   ArtNetSyncSocket;

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

char ArtNetPacket[ARTNET_HEADER_LENGTH + 512];

UniverseEntry ArtNetUniverses[MAX_ARTNET_UNIVERSE_COUNT];
int ArtNetUniverseCount = 0;

char ArtNetLocalAddress[16];

char ArtNetSequenceNumber=1;

void LoadArtNetUniversesFromFile();
void PrintArtNetUniverses();

int ArtNet_InitializeSyncSocket(void)
{
	ArtNetLclSyncAddress.sin_family = AF_INET;
	ArtNetLclSyncAddress.sin_port = htons(ARTNET_SYNC_SOURCE_PORT);
	ArtNetLclSyncAddress.sin_addr.s_addr = inet_addr(ArtNetLocalAddress);

	ArtNetSyncSocket = socket(AF_INET, SOCK_DGRAM, 0);

	if (ArtNetSyncSocket < 0) 
	{
		LogErr(VB_CHANNELOUT, "Error opening sync socket\n");

		exit(1);
	}

	if(bind(ArtNetSyncSocket, (struct sockaddr *) &ArtNetLclSyncAddress, sizeof(struct sockaddr_in)) == -1)
	{
		LogErr(VB_CHANNELOUT, "Error in bind:errno=%d\n",errno);
	} 

	/* Disable loopback so I do not receive my own datagrams. */
	char loopch = 0;
	if(setsockopt(ArtNetSyncSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
	{
		LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
		close(ArtNetSyncSocket);
		exit(1);
	}

	/* Enable Broadcast */
	int broadcast = 1;
	if(setsockopt(ArtNetSyncSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
	{
		LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
		exit(1);
	}

	memset((char *) &ArtNetAddress[ArtNetUniverseCount], 0, sizeof(ArtNetAddress[0]));
	ArtNetAddress[ArtNetUniverseCount].sin_family = AF_INET;
	ArtNetAddress[ArtNetUniverseCount].sin_port = htons(ARTNET_DEST_PORT);
	ArtNetAddress[ArtNetUniverseCount].sin_addr.s_addr = inet_addr("255.255.255.255");

	return 0;
}

int ArtNet_InitializeNetwork()
{
	int UniverseOctet[2];
	char sOctet1[4];
	char sOctet2[4];
	char sAddress[32];
	int i = 0;

	ArtNetSendSocket = socket(AF_INET, SOCK_DGRAM, 0);

	if (!ArtNetUniverseCount)
		return 1;

	if (ArtNetSendSocket < 0) 
	{
		LogErr(VB_CHANNELOUT, "Error opening datagram socket\n");

		exit(1);
	}

	ArtNetLclAddress.sin_family = AF_INET;
	ArtNetLclAddress.sin_port = htons(ARTNET_SOURCE_PORT);
	ArtNetLclAddress.sin_addr.s_addr = inet_addr(ArtNetLocalAddress);

	if(bind(ArtNetSendSocket, (struct sockaddr *) &ArtNetLclAddress, sizeof(struct sockaddr_in)) == -1)
	{
		LogErr(VB_CHANNELOUT, "Error in bind:errno=%d\n",errno);
	} 

	/* Disable loopback so I do not receive my own datagrams. */
	char loopch = 0;
	if(setsockopt(ArtNetSendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
	{
		LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
		close(ArtNetSendSocket);
		return 0;
	}

	/* Initialize the sockaddr structure. */
	for(i=0;i<ArtNetUniverseCount;i++)
	{
		memset((char *) &ArtNetAddress[i], 0, sizeof(ArtNetAddress[0]));
		ArtNetAddress[i].sin_family = AF_INET;
		ArtNetAddress[i].sin_port = htons(ARTNET_DEST_PORT);

		if(ArtNetUniverses[i].type == ARTNET_TYPE_BROADCAST)
		{
			ArtNetAddress[i].sin_addr.s_addr = inet_addr("255.255.255.255");

			int broadcast = 1;
			if(setsockopt(ArtNetSendSocket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
				LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
				exit(1);
			}
		}
		else
		{
			char *c = ArtNetUniverses[i].unicastAddress;
			int isAlpha = 0;
			for (; *c && !isAlpha; c++)
				isAlpha = isalpha(*c);

			if (isAlpha)
			{
				struct hostent* uhost = gethostbyname(ArtNetUniverses[i].unicastAddress);
				if (!uhost)
				{
					LogErr(VB_CHANNELOUT,
						"Error looking up ArtNet hostname: %s\n",
						ArtNetUniverses[i].unicastAddress);
					close(ArtNetSendSocket);
					return 0;
				}

				ArtNetAddress[i].sin_addr.s_addr = *((unsigned long*)uhost->h_addr);
			}
			else
			{
				ArtNetAddress[i].sin_addr.s_addr = inet_addr(ArtNetUniverses[i].unicastAddress);
			}
		}
	}

	// Set ArtNet header Data 
	memcpy((void*)ArtNetPacket,ArtNetHeader,ARTNET_HEADER_LENGTH);

	ArtNet_InitializeSyncSocket();

	return 1;
}

void ArtNet_Initialize()
{
	LogInfo(VB_CHANNELOUT, "Initializing ArtNet output\n");

	ArtNetSequenceNumber=1;
	LoadArtNetUniversesFromFile();
	if (ArtNetUniverseCount)
	{
		GetInterfaceAddress(getE131interface(), ArtNetLocalAddress, NULL, NULL);
		LogDebug(VB_CHANNELOUT, "ArtNetLocalAddress = %s\n",ArtNetLocalAddress);
		ArtNet_InitializeNetwork();
	}
}

/*
 *
 */
int ArtNet_SendSyncPacket(void)
{
	if(sendto(ArtNetSyncSocket, ArtNetSyncPacket, ARTNET_SYNC_PACKET_LENGTH, 0, (struct sockaddr*)&ArtNetAddress[ArtNetUniverseCount], sizeof(ArtNetAddress[ArtNetUniverseCount])) < 0)
	{
		LogErr(VB_CHANNELOUT, "sendto() failed for ArtNet Sync Packet with error: %s\n",
			strerror(errno));
		return 0;
	}

	return 0;
}

/*
 *
 */
int ArtNet_SendData(void *data, char *channelData, int channelCount)
{
	struct itimerval tout_val;
	int i = 0;

	LogExcess(VB_CHANNELDATA, "Sending %d ArtNet universes\n",
		ArtNetUniverseCount);

	for(i=0;i<ArtNetUniverseCount;i++)
	{
		memcpy((void*)(ArtNetPacket+ARTNET_HEADER_LENGTH),(void*)(channelData+ArtNetUniverses[i].startAddress-1),ArtNetUniverses[i].size);

		ArtNetPacket[ARTNET_SEQUENCE_INDEX]   = ArtNetSequenceNumber;

		ArtNetPacket[ARTNET_UNIVERSE_INDEX]   = (char)(ArtNetUniverses[i].universe%256);
		ArtNetPacket[ARTNET_UNIVERSE_INDEX+1] = (char)(ArtNetUniverses[i].universe/256);

		ArtNetPacket[ARTNET_LENGTH_INDEX]     = (char)(ArtNetUniverses[i].size/256);
		ArtNetPacket[ARTNET_LENGTH_INDEX+1]   = (char)(ArtNetUniverses[i].size%256);

		LogExcess(VB_CHANNELDATA, "  %d) ArtNet universe #%d, %d channels\n",
			i + 1, ArtNetUniverses[i].universe, ArtNetUniverses[i].size);

		if(sendto(ArtNetSendSocket, ArtNetPacket, ArtNetUniverses[i].size + ARTNET_HEADER_LENGTH, 0, (struct sockaddr*)&ArtNetAddress[i], sizeof(ArtNetAddress[i])) < 0)
		{
			LogErr(VB_CHANNELOUT, "sendto() failed for ArtNet Universe %d with error: %s\n",
				ArtNetUniverses[i].universe, strerror(errno));
			return 0;
		}
	}

	if (ArtNetUniverseCount)
		ArtNet_SendSyncPacket();

	ArtNetSequenceNumber++;

	return 1;
}

/*
 *
 */
void LoadArtNetUniversesFromFile()
{
	FILE *fp;
	char buf[512];
	char *s;
	ArtNetUniverseCount=0;
	char active =0;

	LogDebug(VB_CHANNELOUT, "Opening File Now %s\n", getUniverseFile());
	fp = fopen((const char *)getUniverseFile(), "r");

	if (fp == NULL) 
	{
		LogErr(VB_CHANNELOUT, "Could not open universe file %s\n",getUniverseFile());
		return;
	}

	while(fgets(buf, 512, fp) != NULL)
	{
		// Enable
		s = strtok(buf,",");
		active = atoi(s);

		if(active)
		{
			// Active
			ArtNetUniverses[ArtNetUniverseCount].active = active;
			// Universe
			s=strtok(NULL,",");
			ArtNetUniverses[ArtNetUniverseCount].universe = atoi(s);
			// StartAddress
			s=strtok(NULL,",");
			ArtNetUniverses[ArtNetUniverseCount].startAddress = atoi(s);
			// Size
			s=strtok(NULL,",");
			ArtNetUniverses[ArtNetUniverseCount].size = atoi(s);

			// ArtNet DMX packets must contain an even number of bytes
			if (ArtNetUniverses[ArtNetUniverseCount].size % 2)
				ArtNetUniverses[ArtNetUniverseCount].size += 1;

			// Type
			s=strtok(NULL,",");
			ArtNetUniverses[ArtNetUniverseCount].type = atoi(s);

			switch (ArtNetUniverses[ArtNetUniverseCount].type) {
				case 2: // Broadcast
						strcpy(ArtNetUniverses[ArtNetUniverseCount].unicastAddress,"\0");
						break;
				case 3: //UnicastAddress
						s=strtok(NULL,",");
						strcpy(ArtNetUniverses[ArtNetUniverseCount].unicastAddress,s);
						break;
				default: // ArtNet
						continue;
			}

		    ArtNetUniverseCount++;
		}
	}
	fclose(fp);
	PrintArtNetUniverses();
}

void PrintArtNetUniverses()
{
	int i=0;
	int h;

	for(i=0;i<ArtNetUniverseCount;i++)
	{
		LogDebug(VB_CHANNELOUT, "ArtNet Universe: %d:%d:%d:%d:%d  %s\n",
				  ArtNetUniverses[i].active,
				  ArtNetUniverses[i].universe,
				  ArtNetUniverses[i].startAddress,
				  ArtNetUniverses[i].size,
				  ArtNetUniverses[i].type,
				  ArtNetUniverses[i].unicastAddress);
	}
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
int ArtNet_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "ArtNet_Open()\n");

	ArtNet_Initialize();

	*privDataPtr = NULL;

	return 1;
}

/*
 *
 */
int ArtNet_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "ArtNet_Close(%p)\n", data);
}

/*
 *
 */
int ArtNet_IsConfigured(void) {
	LogDebug(VB_CHANNELOUT, "ArtNet_IsConfigured()\n");

	if (!getSettingInt("E131Enabled"))
		return 0;

	LoadArtNetUniversesFromFile();

	if (ArtNetUniverseCount > 0);
		return 1;

	return 0;
}

/*
 *
 */
int ArtNet_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "ArtNet_IsActive(%p)\n", data);

	if (ArtNetUniverseCount > 0);
		return 1;

	return 0;
}

/*
 *
 */
int ArtNet_MaxChannels(void *data) {
	return MAX_ARTNET_UNIVERSE_COUNT * 512;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput ArtNetOutput = {
	ArtNet_MaxChannels, //maxChannels
	ArtNet_Open, //open
	ArtNet_Close, //close
	ArtNet_IsConfigured, //isConfigured
	ArtNet_IsActive, //isActive
	ArtNet_SendData, //send
	NULL, //startThread
	NULL, //stopThread
};

