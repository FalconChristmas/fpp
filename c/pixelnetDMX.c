#include "pixelnetDMX.h"
#include "E131.h"
#include "wiringPi.h"
#include "wiringPiSPI.h"
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

extern char logText[256];
extern char fileData[65536];

pthread_t pixelnetDMXthread;
char PixelnetDMXHeader[] = {0xCC,0x55,0xCC,0x55,0xCC,0};

char pixelnetDMXhasBeenSent = 0;
char sendPixelnetDMXdata = 0;

char * pixelnetDMXFile = "/home/pi/media/pixelnetDMX";
PixelnetDMXentry pixelnetDMX[MAX_PIXELNET_DMX_PORTS];
int pixelnetDMXcount =0;

char bufferPixelnetDMX[PIXELNET_DMX_BUF_SIZE]; 


void InitializePixelnetDMX()
{
	int err;
	LoadPixelnetDMXsettingsFromFile();
	if (wiringPiSPISetup (0,8000000) < 0)
	{
		sprintf (logText, "Unable to open SPI device\n") ;
    LogWrite(logText);
		return;
	}
	wiringPiSetupSys();
	SendPixelnetDMXConfig();
}

void SendPixelnetDMX(char sendBlankingData)
{
	int i;
	memcpy(bufferPixelnetDMX,PixelnetDMXHeader,PIXELNET_HEADER_SIZE);
	bufferPixelnetDMX[PIXELNET_DMX_COMMAND_INDEX]=PIXELNET_DMX_COMMAND_DATA;
	if(sendBlankingData)
	{
		memset(&bufferPixelnetDMX[PIXELNET_HEADER_SIZE],0,PIXELNET_DMX_DATA_SIZE);
	}
	else
	{
		memcpy(&bufferPixelnetDMX[PIXELNET_HEADER_SIZE],fileData,PIXELNET_DMX_DATA_SIZE);
		for(i=0;i<PIXELNET_DMX_BUF_SIZE;i++)
		{
			//if (bufferPixelnetDMX[i] == 170)
			//{
			//	bufferPixelnetDMX[i] = 171;
			//} 
		}
	}
	wiringPiSPIDataRW (0, bufferPixelnetDMX, PIXELNET_DMX_BUF_SIZE);
}

void SendPixelnetDMXConfig()
{
	int i,index;
	memset(bufferPixelnetDMX,0,PIXELNET_DMX_BUF_SIZE);
	memcpy(bufferPixelnetDMX,PixelnetDMXHeader,PIXELNET_HEADER_SIZE);
	bufferPixelnetDMX[PIXELNET_DMX_COMMAND_INDEX]=PIXELNET_DMX_COMMAND_CONFIG;
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
  fp = fopen(pixelnetDMXFile, "r");
  sprintf(logText,"Opening PixelnetDMX File\n");
  LogWrite(logText);
  if (fp == NULL) 
  {
    sprintf(logText,"Error Opening PixelnetDMX File\n");
    LogWrite(logText);
	  return;
  }
  while(fgets(buf, 512, fp) != NULL)
  {
		if(pixelnetDMXcount >= MAX_PIXELNET_DMX_PORTS)
		{
			break;
		}

		//	active
		s=strtok(buf,",");
		pixelnetDMX[pixelnetDMXcount].active = atoi(s);

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
    sprintf(logText,"%d,%d,%d\n",pixelnetDMX[i].active,pixelnetDMX[i].type,pixelnetDMX[i].startChannel);
    LogWrite(logText);
  }
}
