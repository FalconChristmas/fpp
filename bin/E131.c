#include "log.h"
#include "E131.h"
#include "playList.h"
#include "settings.h"
#include "lightthread.h"

#include "ogg123.h"
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

// external variables
extern struct mpg123_type mpg123;
extern PlaylistEntry playList[32];
extern int MusicPlayerStatus;
extern MusicStatus musicStatus;
extern currentPlaylistEntry;

// From pixelnetDMX.c
extern char pixelnetDMXhasBeenSent;
extern char sendPixelnetDMXdata;


int E131status = E131_STATUS_IDLE;

struct sockaddr_in    localAddress;
struct sockaddr_in    E131address[MAX_UNIVERSE_COUNT];
int                   sendSocket;

char currentSequenceFile[128];//FIXME


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

int i=0;
char LocalAddress[64];

size_t stepSize=8192;

FILE *seqFile=NULL;
char fileData[65536];
unsigned long filePosition=0;
unsigned long CalculatedMusicFilePosition;
unsigned long currentSequenceFileSize=0;

char stopE131=0;
int MusicLastSecond=0;

int E131secondsElasped = 0;
int E131secondsRemaining = 0;
int E131totalSeconds = 0;
int E131sequenceFramesSent = 0;
char E131sequenceNumber=1;

int syncedToMusic=0;

void ShowDiff(void);

void E131_Initialize()
{
	E131sequenceNumber=1;
	GetLocalWiredIPaddress(LocalAddress);
	LoadUniversesFromFile();
	E131_InitializeNetwork();
	SendBlankingData();
}

void GetLocalWiredIPaddress(char * IPaddress)
{
	FILE *fp;
  size_t len;
	fp = popen("/sbin/ifconfig|grep -v 127.0.0.1|grep -v ::1|grep inet|head -1|sed 's/addr:/ /'|awk '{print $2}'", "r");
 	
	if (fp == NULL) 
	{
		LogWrite("Error getting Local IP Adress. popen returned %d\n",fp);
   	exit;
 	}
	len = fread(IPaddress,1,64,fp);
	// Remove '\n' by replacing with '\0'
	IPaddress[len-1] = '\0';
	LogWrite("IP=%s\n",IPaddress);
 	pclose(fp);
}


int E131_InitializeNetwork()
{
  int UniverseOctet[2];
  char sOctet1[4];
  char sOctet2[4];
  char sAddress[32];
  sendSocket = socket(AF_INET, SOCK_DGRAM, 0);
  if (sendSocket < 0) 
  {
    LogWrite("Error opening datagram socket\n");

    exit(1);
  }

  localAddress.sin_family = AF_INET;
  localAddress.sin_port = htons(E131_SOURCE_PORT);
  localAddress.sin_addr.s_addr = inet_addr(LocalAddress);
  if(bind(sendSocket, (struct sockaddr *) &localAddress, sizeof(struct sockaddr_in)) == -1)
  {
    LogWrite("Error in bind:errno=%d\n",errno);
  } 

  /* Disable loopback so I do not receive my own datagrams. */
  char loopch = 0;
  if(setsockopt(sendSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0)
  {
    LogWrite("Error setting IP_MULTICAST_LOOP error\n");
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

int E131_OpenSequenceFile(const char * file)
{
  size_t bytesRead = 0;
  int seqFileSize;
	syncedToMusic=0;
  if(seqFile!=NULL)
  {
    E131_CloseSequenceFile(); // Close if open
  }
  strcpy(currentSequenceFile,(const char *)getSequenceDirectory());
  strcat(currentSequenceFile,"/");
  strcat(currentSequenceFile,file);
  seqFile = fopen((const char *)currentSequenceFile, "r");
  if (seqFile == NULL) 
  {
		LogWrite("Error opening sequence file: %s fopen returned %d\n",currentSequenceFile,seqFile);
    return 0;
  }
	// Get Step Size
  fseek(seqFile,STEP_SIZE_OFFSET,SEEK_SET);
  bytesRead=fread(fileData,1,4,seqFile);
  stepSize = fileData[0] + (fileData[1]<<8) + (fileData[2]<<16) + (fileData[3]<<24);
	LogWrite("Stepsize %d\n",stepSize);

  fseek(seqFile, 0L, SEEK_END);
  seqFileSize = ftell(seqFile);
  E131totalSeconds = (int)((float)(seqFileSize-CHANNEL_DATA_OFFSET)/((float)stepSize * (float)20));
  fseek(seqFile, CHANNEL_DATA_OFFSET, SEEK_SET);
  filePosition=CHANNEL_DATA_OFFSET;
  stopE131 = 0;
  E131status = E131_STATUS_READING;
  E131sequenceFramesSent = 0;

  E131_ReadData();
  StartLightThread();

  return seqFileSize;
}

void E131_CloseSequenceFile()
{ 
  // Close the file
  if(seqFile !=NULL)
  {
    fclose(seqFile);
    seqFile=NULL;
  }
  E131status = E131_STATUS_IDLE;

  SendBlankingData();
}

int IsSequenceRunning(void)
{
  if (E131status == E131_STATUS_READING)
    return 1;

  return 0;
}

void E131_ReadData(void)
{
	size_t bytesRead = 0;
	if (IsSequenceRunning())
	{
		bytesRead = 0;
		if(filePosition < currentSequenceFileSize - stepSize)
		{
			bytesRead=fread(fileData,1,stepSize,seqFile);
			filePosition+=bytesRead;
		}

		if (bytesRead != stepSize)
		{
			E131_CloseSequenceFile();
		}
	}
	else
	{
		bzero(fileData, sizeof(fileData));
	}
}

void E131_Send()
{
  struct itimerval tout_val;
  ShowDiff();

	if(MusicPlayerStatus==PLAYING_MPLAYER_STATUS && !syncedToMusic && (musicStatus.secondsElasped < 3))
	{
		//Playlist_SyncToMusic();
	}

	for(i=0;i<UniverseCount;i++)
	{
		memcpy((void*)(E131packet+E131_HEADER_LENGTH),(void*)(fileData+universes[i].startAddress-1),universes[i].size);

		E131packet[E131_SEQUENCE_INDEX] = E131sequenceNumber;;
		E131packet[E131_UNIVERSE_INDEX] = (char)(universes[i].universe/256);
		E131packet[E131_UNIVERSE_INDEX+1]	= (char)(universes[i].universe%256);
		E131packet[E131_COUNT_INDEX] = (char)((universes[i].size+1)/256);
		E131packet[E131_COUNT_INDEX+1] = (char)((universes[i].size+1)%256);
		if(sendto(sendSocket, E131packet, universes[i].size + E131_HEADER_LENGTH, 0, (struct sockaddr*)&E131address[i], sizeof(E131address[i])) < 0)
		{
			return;
		}
	}

	E131sequenceNumber++;

	if (IsSequenceRunning())
	{
		E131sequenceFramesSent++;
		E131secondsElasped = (int)((float)(filePosition-CHANNEL_DATA_OFFSET)/((float)stepSize*(float)20.0));
		E131secondsRemaining = E131totalSeconds-E131secondsElasped;
	}

	// Send data to pixelnet board
	E131_SendPixelnetDMXdata();
}

void E131_SendPixelnetDMXdata()
{
	SendPixelnetDMX();
}


	
void Playlist_SyncToMusic(void)
{
  unsigned int diff=0;
	unsigned int absDifference=0;
	float MusicSeconds = (float)((float)musicStatus.secondsElasped + ((float)musicStatus.subSecondsElasped/(float)100));
	syncedToMusic = 1;
  CalculatedMusicFilePosition = ((long)(MusicSeconds * RefreshRate) * stepSize) + CHANNEL_DATA_OFFSET  ;
  LogWrite("Syncing to Music\n");
  filePosition = CalculatedMusicFilePosition;
  fseek(seqFile, CalculatedMusicFilePosition, SEEK_SET);
}

void ShowDiff(void)
{
  unsigned int diff=0;
	unsigned int absDifference=0;
  int secs;
	float MusicSeconds = (float)((float)musicStatus.secondsElasped + ((float)musicStatus.subSecondsElasped/(float)100));
	
	MusicSeconds = customRounding(MusicSeconds, .05);
	
	secs = (int)MusicSeconds;
	if (MusicLastSecond == secs)
	{return;}
	MusicLastSecond = secs;
	

  CalculatedMusicFilePosition = ((long)(MusicSeconds * RefreshRate) * stepSize) + CHANNEL_DATA_OFFSET  ;
	if(CalculatedMusicFilePosition > filePosition)
	{
		diff = -(CalculatedMusicFilePosition - filePosition);
	}
	else
	{
		diff = filePosition-CalculatedMusicFilePosition;
	}
		
  absDifference = abs(diff);
//  LogWrite("RefreshRate= %f MusicSeconds = %f secs=%d diff = %d , abs = %d  CFP= %d FP= %d       
//\n",RefreshRate,MusicSeconds,MusicLastSecond,diff,absDifference,CalculatedMusicFilePosition,filePosition);
}


float customRounding(float value, float roundingValue) 
{
    int mulitpler = floor(value / roundingValue);
    return mulitpler * roundingValue;
}

void LoadUniversesFromFile()
{
  FILE *fp;
  char buf[512];
  char *s;
  UniverseCount=0;
	char active =0;

  LogWrite("Opening File Now %s\n",getUniverseFile());
  fp = fopen((const char *)getUniverseFile(), "r");
  if (fp == NULL) 
  {
    LogWrite("Could not open universe file %s\n",getUniverseFile());
  	return;
  }
  while(fgets(buf, 512, fp) != NULL)
  {
    // Enable
    s=strtok(buf,",");
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
    LogWrite("%d:%d:%d:%d:%d  %s\n",
                                         universes[i].active,
                                          universes[i].universe,
                                          universes[i].size,
                                          universes[i].startAddress,
                                          universes[i].type,
                                          universes[i].unicastAddress
                                          );
  }
}

void SendBlankingData(void)
{
	E131_ReadData();
	E131_Send();
}
