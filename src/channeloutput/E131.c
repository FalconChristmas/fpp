/*
 *   E131 output handler for Falcon Pi Player (FPP)
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
#include "E131.h"
#include "FPD.h"
#include "fpp.h"
#include "log.h"
#include "settings.h"
#include "Universe.h"

struct sockaddr_in    localAddress;
struct sockaddr_in    E131address[MAX_UNIVERSE_COUNT];
int                   sendSocket;

const char  E131header[] = {
	0x00,0x10,0x00,0x00,0x41,0x53,0x43,0x2d,0x45,0x31,0x2e,0x31,0x37,0x00,0x00,0x00,
	0x72,0x6e,0x00,0x00,0x00,0x04,'F','A','L','C','O','N',' ','F','P','P',
	0x00,0x00,0x00,0x00,0x00,0x00,0x72,0x58,0x00,0x00,0x00,0x02,'F','A','L','C',
	'O','N','C','H','R','I','S','T','M','A','S','.','C','O','M',' ',
	'B','Y',' ','D','P','I','T','T','S',' ','A','N','D',' ','M','Y',
	'K','R','O','F','T',' ','F','A','L','C','O','N',' ','P','I',' ',
	'P','L','A','Y','E','R',0x00,0x00,0x00,0x00,0x00,0x00,0x64,0x00,0x00,0x00,
	0x00,0x00,0x01,0x72,0x0b,0x02,0xa1,0x00,0x00,0x00,0x01,0x02,0x01,0x00
	};

UniverseEntry universes[MAX_UNIVERSE_COUNT];
int UniverseCount = 0;

char E131LocalAddress[16];

char E131sequenceNumber=1;


struct mmsghdr *e131Msgs = nullptr;
struct iovec *e131Iovecs = nullptr;
unsigned char **e131Buffers = nullptr;

int E131_InitializeNetwork()
{
	int UniverseOctet[2];
	char sOctet1[4];
	char sOctet2[4];
	char sAddress[32];

	sendSocket = socket(AF_INET, SOCK_DGRAM, 0);

	if (!UniverseCount)
		return 1;

	if (sendSocket < 0) 
	{
		LogErr(VB_CHANNELOUT, "Error opening datagram socket\n");

		exit(1);
	}

	localAddress.sin_family = AF_INET;
	localAddress.sin_port = htons(E131_SOURCE_PORT);
	localAddress.sin_addr.s_addr = inet_addr(E131LocalAddress);

	if(bind(sendSocket, (struct sockaddr *) &localAddress, sizeof(struct sockaddr_in)) == -1)
	{
		LogErr(VB_CHANNELOUT, "Error in bind:errno=%d, %s\n", errno, strerror(errno));
	} 

	/* Disable loopback so I do not receive my own datagrams. */
	char loopch = 0;
	if(setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
	{
		LogErr(VB_CHANNELOUT, "Error setting IP_MULTICAST_LOOP error\n");
		close(sendSocket);
		return 0;
	}

	/* Initialize the sockaddr structure. */
	for(int i=0;i<UniverseCount;i++)
	{
		memset((char *) &E131address[i], 0, sizeof(E131address[0]));
		E131address[i].sin_family = AF_INET;
		E131address[i].sin_port = htons(E131_DEST_PORT);

		if(universes[i].type == E131_TYPE_MULTICAST)
		{
			UniverseOctet[0] = universes[i].universe/256;
			UniverseOctet[1] = universes[i].universe%256;
			sprintf(sAddress, "239.255.%d.%d", UniverseOctet[0],UniverseOctet[1]);
			E131address[i].sin_addr.s_addr = inet_addr(sAddress);
		}
		else
		{
			char *c = universes[i].unicastAddress;
			int isAlpha = 0;
			for (; *c && !isAlpha; c++)
				isAlpha = isalpha(*c);

			if (isAlpha)
			{
				struct hostent* uhost = gethostbyname(universes[i].unicastAddress);
				if (!uhost)
				{
					LogErr(VB_CHANNELOUT,
						"Error looking up E1.31 hostname: %s\n",
						universes[i].unicastAddress);
					close(sendSocket);
					return 0;
				}

				E131address[i].sin_addr.s_addr = *((unsigned long*)uhost->h_addr);
			}
			else
			{
				E131address[i].sin_addr.s_addr = inet_addr(universes[i].unicastAddress);
			}
		}
	}
    
    
    e131Msgs = (struct mmsghdr *)calloc(UniverseCount, sizeof(struct mmsghdr));
    e131Iovecs = (struct iovec *)calloc(UniverseCount, sizeof(struct iovec));
    e131Buffers = (unsigned char **)calloc(UniverseCount, sizeof(unsigned char*));
    for (int x = 0; x < UniverseCount; x++) {
        e131Buffers[x] = (unsigned char *)malloc(universes[x].size + E131_HEADER_LENGTH);
        memcpy(e131Buffers[x], E131header, E131_HEADER_LENGTH);
        
        
        e131Buffers[x][E131_PRIORITY_INDEX] = universes[x].priority;
        
        e131Buffers[x][E131_UNIVERSE_INDEX] = (char)(universes[x].universe/256);
        e131Buffers[x][E131_UNIVERSE_INDEX+1] = (char)(universes[x].universe%256);
        
        // Property Value Count
        e131Buffers[x][E131_COUNT_INDEX] = ((universes[x].size+1)/256);
        e131Buffers[x][E131_COUNT_INDEX+1] = ((universes[x].size+1)%256);
        
        // RLP Protocol flags and length
        int count = 638 - 16 - (512 - (universes[x].size));
        e131Buffers[x][E131_RLP_COUNT_INDEX] = (count/256)+0x70;
        e131Buffers[x][E131_RLP_COUNT_INDEX+1] = count%256;
        
        // Framing Protocol flags and length
        count = 638 - 38 - (512 - (universes[x].size));
        e131Buffers[x][E131_FRAMING_COUNT_INDEX] = (count/256)+0x70;
        e131Buffers[x][E131_FRAMING_COUNT_INDEX+1] = count%256;
        
        // DMP Protocol flags and length
        count = 638 - 115 - (512 - (universes[x].size));
        e131Buffers[x][E131_DMP_COUNT_INDEX] = (count/256)+0x70;
        e131Buffers[x][E131_DMP_COUNT_INDEX+1] = count%256;

        e131Iovecs[x].iov_base = e131Buffers[x];
        e131Iovecs[x].iov_len = universes[x].size + E131_HEADER_LENGTH;

        e131Msgs[x].msg_hdr.msg_name = &E131address[x];
        e131Msgs[x].msg_hdr.msg_namelen = sizeof(sockaddr_in);
        e131Msgs[x].msg_hdr.msg_iov = &e131Iovecs[x];
        e131Msgs[x].msg_hdr.msg_iovlen = 1;
        e131Msgs[x].msg_len = universes[x].size + E131_HEADER_LENGTH;
    }

	// Set E131 header Data 

	return 1;
}

void E131_Initialize()
{
	LogInfo(VB_CHANNELOUT, "Initializing E1.31 output\n");

	E131sequenceNumber=1;
	LoadUniversesFromFile();
	if (UniverseCount)
	{
		GetInterfaceAddress(getE131interface(), E131LocalAddress, NULL, NULL);
		LogDebug(VB_CHANNELOUT, "E131LocalAddress = %s\n",E131LocalAddress);
		E131_InitializeNetwork();
	}
}

/*
 *
 */
int E131_SendData(void *data, char *channelData, int channelCount)
{
	struct itimerval tout_val;
	int i = 0;

	LogExcess(VB_CHANNELDATA, "Sending %d E1.31 universes\n",
		UniverseCount);

	for(i=0; i<UniverseCount; i++)
	{
        unsigned char *E131packet = e131Buffers[i];
        E131packet[E131_SEQUENCE_INDEX] = E131sequenceNumber;

		memcpy((void*)(E131packet+E131_HEADER_LENGTH),(void*)(channelData+universes[i].startAddress-1),universes[i].size);

		LogExcess(VB_CHANNELDATA, "  %d) E1.31 universe #%d, %d channels\n",
			i + 1, universes[i].universe, universes[i].size);
	}
    if(sendmmsg(sendSocket, e131Msgs, UniverseCount, 0) != UniverseCount)
    {
        LogErr(VB_CHANNELOUT, "sendto() failed for E1.31 Universe %d with error: %s\n",
               universes[i].universe, strerror(errno));
        return 0;
    }

	E131sequenceNumber++;

	return 1;
}

/*
 *
 */
void LoadUniversesFromFile()
{
	FILE *fp;
	char buf[512];
	char *s;
	UniverseCount=0;
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
			universes[UniverseCount].active = active;
			// Universe
			s=strtok(NULL,",");
			universes[UniverseCount].universe = atoi(s);
			// StartAddress
			s=strtok(NULL,",");
			universes[UniverseCount].startAddress = atoi(s);
			// Size
			s=strtok(NULL,",");
			universes[UniverseCount].size = atoi(s);
			// Type
			s=strtok(NULL,",");
			universes[UniverseCount].type = atoi(s);

			switch (universes[UniverseCount].type) {
				case 0: // Multicast
						strcpy(universes[UniverseCount].unicastAddress,"\0");
						break;
				case 1: //UnicastAddress
						s=strtok(NULL,",");
						strcpy(universes[UniverseCount].unicastAddress,s);
						break;
				default: // ArtNet
						continue;
			}

			// FIXME, read this per-universe from config file later
			if (getSettingInt("E131Priority"))
				universes[UniverseCount].priority
					= getSettingInt("E131Priority");
			else
				universes[UniverseCount].priority = 0;

		    UniverseCount++;
		}
	}
	fclose(fp);
	UniversesPrint();
}

void ResetBytesReceived()
{
	int i;

	for(i=0;i<UniverseCount;i++)
	{
		universes[i].bytesReceived = 0;
	}
}

void WriteBytesReceivedFile()
{
	int i;
	FILE *file;
	file = fopen((const char *)getBytesFile(), "w");
	for(i=0;i<UniverseCount;i++)
	{
		if(i==UniverseCount-1)
		{
			fprintf(file, "%d,%d,%d,",universes[i].universe,universes[i].startAddress,universes[i].bytesReceived);
		}
		else
		{
			fprintf(file, "%d,%d,%d,\n",universes[i].universe,universes[i].startAddress,universes[i].bytesReceived);
		}
	}
	fclose(file);
}


void UniversesPrint()
{
	int i=0;
	int h;

	for(i=0;i<UniverseCount;i++)
	{
		LogDebug(VB_CHANNELOUT, "E1.31 Universe: %d:%d:%d:%d:%d  %s\n",
				  universes[i].active,
				  universes[i].universe,
				  universes[i].startAddress,
				  universes[i].size,
				  universes[i].type,
				  universes[i].unicastAddress);
	}
}

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
int E131_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "E131_Open()\n");

	E131_Initialize();

	*privDataPtr = NULL;

	return 1;
}

/*
 *
 */
int E131_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "E131_Close(%p)\n", data);
    
    
    if (e131Msgs)  {
        free(e131Msgs);
        e131Msgs = nullptr;
    }
    if (e131Iovecs)  {
        free(e131Iovecs);
        e131Iovecs = nullptr;
    }
    if (e131Buffers)  {
        for (int x = 0; x < UniverseCount; x++) {
            free(e131Buffers[x]);
        }
        free(e131Buffers);
        e131Buffers = nullptr;
    }
}

/*
 *
 */
int E131_IsConfigured(void) {
	LogDebug(VB_CHANNELOUT, "E131_IsConfigured()\n");

	if (!getSettingInt("E131Enabled"))
		return 0;

	LoadUniversesFromFile();

	if (UniverseCount > 0);
		return 1;

	return 0;
}

/*
 *
 */
int E131_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "E131_IsActive(%p)\n", data);

	if (UniverseCount > 0);
		return 1;

	return 0;
}

/*
 *
 */
int E131_MaxChannels(void *data) {
	return 32768;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput E131Output = {
	E131_MaxChannels, //maxChannels
	E131_Open, //open
	E131_Close, //close
	E131_IsConfigured, //isConfigured
	E131_IsActive, //isActive
	E131_SendData, //send
	NULL, //startThread
	NULL, //stopThread
};
