#include "log.h"
#include "pixelnetDMX.h"
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

void CreatePixelnetDMXfile(const char * file)
{
	LogWrite("Create PixelnetDMX File stub: %s\n",file);
}


void InitializePixelnetDMX()
{
	LogWrite("Init stub\n");
}

void SendPixelnetDMX(void)
{
//	LogWrite("Send DMX data stub\n");
}

void SendPixelnetDMXConfig()
{
	LogWrite("Send DMX config stub\n");
}

int IsPixelnetDMXActive(void)
{
	return 0;
}

void LoadPixelnetDMXsettingsFromFile()
{
	LogWrite("Load PixelnetDMX file stub\n");
}
