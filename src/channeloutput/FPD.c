#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "../common.h"
#include "E131.h"
#include "FPD.h"
#include "../log.h"
#include "../sequence.h"
#include "../settings.h"

#ifdef USEWIRINGPI
#	include "wiringPi.h"
#	include "wiringPiSPI.h"
#else
#	define wiringPiSPISetup(a,b)    1
#	define wiringPiSetupSys()       0
#	define wiringPiSPIDataRW(a,b,c) 1
#	define delayMicroseconds(a)     0
#endif

#define MAX_PIXELNET_DMX_PORTS          12 
#define PIXELNET_DMX_DATA_SIZE          32768
#define PIXELNET_HEADER_SIZE                6       
#define PIXELNET_DMX_BUF_SIZE               (PIXELNET_DMX_DATA_SIZE+PIXELNET_HEADER_SIZE)

#define PIXELNET_DMX_COMMAND_CONFIG 0
#define PIXELNET_DMX_COMMAND_DATA       0xFF

typedef struct {
	char active;
	char type;
	int startChannel;
} PixelnetDMXentry;


pthread_t pixelnetDMXthread;
char PixelnetDMXcontrolHeader[] = {0x55,0x55,0x55,0x55,0x55,0xCC};
char PixelnetDMXdataHeader[] = {0xCC,0xCC,0xCC,0xCC,0xCC,0x55};


PixelnetDMXentry pixelnetDMX[MAX_PIXELNET_DMX_PORTS];
int pixelnetDMXcount =0;
int pixelnetDMXactive = 0;

char bufferPixelnetDMX[PIXELNET_DMX_BUF_SIZE];

/* Prototypes defined below */
void LoadPixelnetDMXsettingsFromFile();
void PixelnetDMXPrint();


/*
 *
 */
void CreatePixelnetDMXfile(const char * file)
{
	FILE *fp;
	char settings[16];
	char command[16];
	int i;
	int startChannel=1;
	fp = fopen(file, "w");
	if ( ! fp )
	{
		LogErr(VB_CHANNELOUT, "Error: Unable to create pixelnet file.\n");
		exit(EXIT_FAILURE);
	}
	LogDebug(VB_CHANNELOUT, "Creating file: %s\n",file);

	for(i=0;i<MAX_PIXELNET_DMX_PORTS;i++,startChannel+=4096)
	{
		if(i==MAX_PIXELNET_DMX_PORTS-1)
		{
			sprintf(settings,"1,0,%d,",startChannel);
		}
		else
		{
			sprintf(settings,"1,0,%d,\n",startChannel);
		}
		fwrite(settings,1,strlen(settings),fp);
	}
	fclose(fp);
	sprintf(command,"sudo chmod 775 %s",file);
	system(command);
}


int InitializePixelnetDMX()
{
	int err;
	LogInfo(VB_CHANNELOUT, "Initializing SPI for FPD output\n");

	LoadPixelnetDMXsettingsFromFile();
	if (wiringPiSPISetup (0,8000000) < 0)
	{
	    LogErr(VB_CHANNELOUT, "Unable to open SPI device\n") ;
		return 0;
	}
	wiringPiSetupSys();
	SendFPDConfig();

	return 1;
}

void SendFPDConfig()
{
	int i,index;
	memset(bufferPixelnetDMX,0,PIXELNET_DMX_BUF_SIZE);
	memcpy(bufferPixelnetDMX,PixelnetDMXcontrolHeader,PIXELNET_HEADER_SIZE);
	index = PIXELNET_HEADER_SIZE;
	for(i=0;i<pixelnetDMXcount;i++)
	{
		bufferPixelnetDMX[index++] = pixelnetDMX[i].type;
		bufferPixelnetDMX[index++] = (char)(pixelnetDMX[i].startChannel%256);
		bufferPixelnetDMX[index++] = (char)(pixelnetDMX[i].startChannel/256);
	}
	wiringPiSPIDataRW (0, bufferPixelnetDMX, PIXELNET_DMX_BUF_SIZE);
	delayMicroseconds (10000) ;
	wiringPiSPIDataRW (0, bufferPixelnetDMX, PIXELNET_DMX_BUF_SIZE);

}

void LoadPixelnetDMXsettingsFromFile()
{
  FILE *fp;
  char buf[512];
  char *s;
  fp = fopen((const char *)getPixelnetFile(), "r");
  LogDebug(VB_CHANNELOUT, "Opening PixelnetDMX File\n");
  if (fp == NULL) 
  {
    LogErr(VB_CHANNELOUT, "Error Opening PixelnetDMX File\n");
	  return;
  }
  pixelnetDMXactive = 0;
  while(fgets(buf, 512, fp) != NULL)
  {
		if(pixelnetDMXcount >= MAX_PIXELNET_DMX_PORTS)
		{
			break;
		}

		//	active
		s=strtok(buf,",");
		pixelnetDMX[pixelnetDMXcount].active = atoi(s);

		if (pixelnetDMX[pixelnetDMXcount].active)
			pixelnetDMXactive = 1;

		//	type
		s=strtok(NULL,",");
		pixelnetDMX[pixelnetDMXcount].type = atoi(s);

		// Start Channel
		s=strtok(NULL,",");
		pixelnetDMX[pixelnetDMXcount].startChannel = atoi(s);
		pixelnetDMXcount++;
  }
  fclose(fp);
  PixelnetDMXPrint();
}

void PixelnetDMXPrint()
{
  int i=0;
  int h;
  for(i=0;i<pixelnetDMXcount;i++)
  {
    LogDebug(VB_CHANNELOUT, "%d,%d,%d\n",pixelnetDMX[i].active,pixelnetDMX[i].type,pixelnetDMX[i].startChannel);
  }
}


/*
 *
 */
int FPD_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "FPD_Open()\n");

	if (!FileExists(getPixelnetFile())) {
		LogDebug(VB_CHANNELOUT, "FPD config file does not exist, creating it.\n");
		CreatePixelnetDMXfile(getPixelnetFile());
	}

	if (!InitializePixelnetDMX())
		return 0;

	*privDataPtr = NULL;

	return 1;
}

/*
 *
 */
int FPD_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "FPD_Close(%p)\n", data);
}

/*
 *
 */
int FPD_IsConfigured(void) {
	LogDebug(VB_CHANNELOUT, "FPD_IsConfigured()\n");

	if (!getSettingInt("FPDEnabled"))
		return 0;

	LoadPixelnetDMXsettingsFromFile();
	return pixelnetDMXactive;
}

/*
 *
 */
int FPD_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "FPD_IsActive(%p)\n", data);

	return pixelnetDMXactive;
}

/*
 *
 */
int FPD_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "FPD_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	int i;
	memcpy(bufferPixelnetDMX,PixelnetDMXdataHeader,PIXELNET_HEADER_SIZE);
	memcpy(&bufferPixelnetDMX[PIXELNET_HEADER_SIZE],channelData,PIXELNET_DMX_DATA_SIZE);
	for(i=0;i<PIXELNET_DMX_BUF_SIZE;i++)
	{
		if (bufferPixelnetDMX[i] == 170)
		{
			bufferPixelnetDMX[i] = 171;
		}
	}
	wiringPiSPIDataRW (0, bufferPixelnetDMX, PIXELNET_DMX_BUF_SIZE);

	return 1;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput FPDOutput = {
	.maxChannels  = 32768,
	.open         = FPD_Open,
	.close        = FPD_Close,
	.isConfigured = FPD_IsConfigured,
	.isActive     = FPD_IsActive,
	.send         = FPD_SendData
	};

