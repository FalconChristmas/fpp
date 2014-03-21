#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <arpa/inet.h>
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

#include "channeloutput.h"
#include "channeloutputthread.h"
#include "../common.h"
#include "E131.h"
#include "FPD.h"
#include "../fpp.h"
#include "../log.h"
#include "../playList.h"
//#include "../sequence.h"
#include "../settings.h"

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
char E131packet[638];

UniverseEntry universes[MAX_UNIVERSE_COUNT];
int UniverseCount = 0;

char * E131LocalAddress;

char E131sequenceNumber=1;

/* Prototypes for helpers below */
void ShowDiff(void);
void LoadUniversesFromFile();
void UniversesPrint();


int E131_InitializeNetwork()
{
	int UniverseOctet[2];
	char sOctet1[4];
	char sOctet2[4];
	char sAddress[32];
	int i = 0;

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
		LogErr(VB_CHANNELOUT, "Error in bind:errno=%d\n",errno);
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
	for(i=0;i<UniverseCount;i++)
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
			E131address[i].sin_addr.s_addr = inet_addr(universes[i].unicastAddress);
		}
	}

	// Set E131 header Data 
	memcpy((void*)E131packet,E131header,126);

	return 1;
}

void E131_Initialize()
{
	LogInfo(VB_CHANNELOUT, "Initializing E1.31 output\n");

	E131sequenceNumber=1;
	LoadUniversesFromFile();
	if (UniverseCount)
	{
		E131LocalAddress = GetInterfaceAddress(getE131interface());
		LogDebug(VB_CHANNELOUT, "E131LocalAddress = %s\n",E131LocalAddress);
		E131_InitializeNetwork();
		free(E131LocalAddress);
	}
}

/*
 *
 */
int E131_SendData(void *data, char *channelData, int channelCount)
{
	struct itimerval tout_val;
	int i = 0;

	for(i=0;i<UniverseCount;i++)
	{
		memcpy((void*)(E131packet+E131_HEADER_LENGTH),(void*)(channelData+universes[i].startAddress-1),universes[i].size);

		E131packet[E131_SEQUENCE_INDEX] = E131sequenceNumber;
		E131packet[E131_UNIVERSE_INDEX] = (char)(universes[i].universe/256);
		E131packet[E131_UNIVERSE_INDEX+1]	= (char)(universes[i].universe%256);
		E131packet[E131_COUNT_INDEX] = (char)((universes[i].size+1)/256);
		E131packet[E131_COUNT_INDEX+1] = (char)((universes[i].size+1)%256);
		if(sendto(sendSocket, E131packet, universes[i].size + E131_HEADER_LENGTH, 0, (struct sockaddr*)&E131address[i], sizeof(E131address[i])) < 0)
		{
			return 0;
		}
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
			if(universes[UniverseCount].type == 1)
			{
				//UnicastAddress
				s=strtok(NULL,",");
				strcpy(universes[UniverseCount].unicastAddress,s);
			}
			else
			{
				strcpy(universes[UniverseCount].unicastAddress,"\0");
			}

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
				  universes[i].size,
				  universes[i].startAddress,
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
}

/*
 *
 */
int E131_IsConfigured(void) {
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
 * Declare our external interface struct
 */
FPPChannelOutput E131Output = {
	.maxChannels  = 32768,
	.open         = E131_Open,
	.close        = E131_Close,
	.isConfigured = E131_IsConfigured,
	.isActive     = E131_IsActive,
	.send         = E131_SendData
	};


