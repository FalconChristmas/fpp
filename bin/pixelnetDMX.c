#include "log.h"
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

extern char fileData[65536];

pthread_t pixelnetDMXthread;
char PixelnetDMXcontrolHeader[] = {0x55,0x55,0x55,0x55,0x55,0xCC};
char PixelnetDMXdataHeader[] = {0xCC,0xCC,0xCC,0xCC,0xCC,0x55};


char pixelnetDMXhasBeenSent = 0;
char sendPixelnetDMXdata = 0;

PixelnetDMXentry pixelnetDMX[MAX_PIXELNET_DMX_PORTS];
int pixelnetDMXcount =0;

char bufferPixelnetDMX[PIXELNET_DMX_BUF_SIZE]; 

void CreatePixelnetDMXfile(const char * file)
{
  FILE *fp;
	char settings[16];
	char command[16];
	int i;
	int startChannel=1;
  fp = fopen(file, "w");
	LogWrite("Creating file: %s\n",file);
		
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


void InitializePixelnetDMX()
{
	int err;
	LoadPixelnetDMXsettingsFromFile();
	if (wiringPiSPISetup (0,8000000) < 0)
	{
	    LogWrite("Unable to open SPI device\n") ;
		return;
	}
	wiringPiSetupSys();
	SendPixelnetDMXConfig();
}

void SendPixelnetDMX(char sendBlankingData)
{
	int i;
	memcpy(bufferPixelnetDMX,PixelnetDMXdataHeader,PIXELNET_HEADER_SIZE);
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
  LogWrite("Opening PixelnetDMX File\n");
  if (fp == NULL) 
  {
    LogWrite("Error Opening PixelnetDMX File\n");
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
    LogWrite("%d,%d,%d\n",pixelnetDMX[i].active,pixelnetDMX[i].type,pixelnetDMX[i].startChannel);
  }
}
