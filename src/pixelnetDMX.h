#ifndef _PIXELNET_DMX_H
#define _PIXELNET_DMX_H

#define MAX_PIXELNET_DMX_PORTS  		12 
#define PIXELNET_DMX_DATA_SIZE			32768
#define PIXELNET_HEADER_SIZE				6		
#define PIXELNET_DMX_BUF_SIZE				(PIXELNET_DMX_DATA_SIZE+PIXELNET_HEADER_SIZE)

#define PIXELNET_DMX_COMMAND_CONFIG	0
#define PIXELNET_DMX_COMMAND_DATA		0xFF

void InitializePixelnetDMX();
void CreatePixelnetDMXfile(const char * file);
void SendPixelnetDMX(void);
void SendPixelnetDMXConfig();
void LoadPixelnetDMXsettingsFromFile();
void PixelnetDMXPrint();

typedef struct{
		char active;
		char type;
		int startChannel;
}PixelnetDMXentry;


#endif
