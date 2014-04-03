
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../log.h"
#include "../settings.h"

#include "USBPixelnet.h"

/////////////////////////////////////////////////////////////////////////////

typedef struct usbPixelnetPrivData {
	char  filename[1024];
	char  outputData[4102]; // Up to 6-bytes of header followed by 4096 bytes of data
	int   outputPacketSize; // Header size + 4096
	char *pixelnetData;
	int   fd;
} USBPixelnetPrivData;

enum DongleType {
	PIXELNET_DVC_UNKNOWN,
	PIXELNET_DVC_LYNX,
	PIXELNET_DVC_OPEN
};
/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void USBPixelnet_Dump(USBPixelnetPrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	LogDebug(VB_CHANNELOUT, "    filename        : %s\n", privData->filename);
	LogDebug(VB_CHANNELOUT, "    fd              : %d\n", privData->fd);
	LogDebug(VB_CHANNELOUT, "    outputPacketSize: %d\n", privData->outputPacketSize);
}

/*
 *
 */
int USBPixelnet_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "USBPixelnet_Open('%s')\n", configStr);

	USBPixelnetPrivData *privData = malloc(sizeof(USBPixelnetPrivData));
	bzero(privData, sizeof(USBPixelnetPrivData));
	privData->fd = -1;

	char deviceName[32];
	int  dongleType = PIXELNET_DVC_UNKNOWN;
	char *s = strtok(configStr, ";");

	strcpy(deviceName, "UNKNOWN");

	while (s) {
		char tmp[128];
		char *div = NULL;

		strcpy(tmp, s);
		div = strchr(tmp, '=');

		if (div) {
			*div = '\0';
			div++;

			if (!strcmp(tmp, "device")) {
				LogDebug(VB_CHANNELOUT, "Using %s for Pixelnet output\n", div);
				strcpy(deviceName, div);
			} else if (!strcmp(tmp, "type")) {
				if (!strcmp(div, "Pixelnet-Lynx")) {
					LogDebug(VB_CHANNELOUT, "Treating device as Pixelnet-Lynx compatible\n");
					dongleType = PIXELNET_DVC_LYNX;
				} else if (!strcmp(div, "Pixelnet-Open")) {
					LogDebug(VB_CHANNELOUT, "Treating device as Pixelnet-Open compatible\n");
					dongleType = PIXELNET_DVC_OPEN;
				}
			}
		}
		s = strtok(NULL, ",");
	}

	if ((!strcmp(deviceName, "UNKNOWN")) ||
		(dongleType == PIXELNET_DVC_UNKNOWN))
	{
		LogErr(VB_CHANNELOUT, "Invalid Config Str: %s\n", configStr);
		free(privData);
		return 0;
	}

	strcpy(privData->filename, "/dev/");
	strcat(privData->filename, deviceName);

	LogInfo(VB_CHANNELOUT, "Opening %s for Pixelnet output\n",
		privData->filename);


	if (dongleType == PIXELNET_DVC_LYNX)
		privData->fd = SerialOpen(privData->filename, 115200, "8N1");
	else if (dongleType == PIXELNET_DVC_OPEN)
		privData->fd = SerialOpen(privData->filename, 1000000, "8N1");

	if (privData->fd < 0)
	{
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, privData->filename, strerror(errno));

		free(privData);
		return 0;
	}

	if (dongleType == PIXELNET_DVC_LYNX) {
		privData->outputData[0] = '\xAA';

		privData->pixelnetData = privData->outputData + 1;
		privData->outputPacketSize = 4097;
	} else if (dongleType == PIXELNET_DVC_OPEN) {
		privData->outputData[0] = '\xAA';
		privData->outputData[1] = '\x55';
		privData->outputData[2] = '\x55';
		privData->outputData[3] = '\xAA';
		privData->outputData[4] = '\x15';
		privData->outputData[5] = '\x5D';

		privData->pixelnetData = privData->outputData + 6;
		privData->outputPacketSize = 4102;
	}

	USBPixelnet_Dump(privData);

	*privDataPtr = privData;

	return 1;
}

/*
 *
 */
int USBPixelnet_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "USBPixelnet_Close(%p)\n", data);

	USBPixelnetPrivData *privData = (USBPixelnetPrivData*)data;
	USBPixelnet_Dump(privData);

	SerialClose(privData->fd);
	privData->fd = -1;
}

/*
 *
 */
int USBPixelnet_IsConfigured(void) {
	if ((strcmp(getUSBDonglePort(),"DISABLED")) &&
		(!strcmp(getUSBDongleType(), "PixelnetLynx")))
		return 1;

	return 0;
}

/*
 *
 */
int USBPixelnet_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "USBPixelnet_IsActive(%p)\n", data);
	USBPixelnetPrivData *privData = (USBPixelnetPrivData*)data;

	if (!privData)
		return 0;

	USBPixelnet_Dump(privData);

	if (privData->fd > 0)
		return 1;

	return 0;
}

/*
 *
 */
int USBPixelnet_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "USBPixelnet_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	USBPixelnetPrivData *privData = (USBPixelnetPrivData*)data;

	if (channelCount <= 4096) {
		bzero(privData->pixelnetData, 4096);
	} else {
		LogErr(VB_CHANNELOUT,
			"USBPixelnet_SendData() tried to send %d bytes when max is 4096\n",
			channelCount);
		return 0;
	}

	memcpy(privData->pixelnetData, channelData, channelCount);

	// 0xAA is start of Pixelnet packet, so convert 0xAA (170) to 0xAB (171)
	char *dptr = privData->pixelnetData;
	int i = 0;
	for( i = 0; i < 4096; i++ ) {
		if (*dptr == '\xAA')
			*dptr = '\xAB';

		dptr++;
	}

	// Send Header and Pixelnet Data
	write(privData->fd, privData->outputData, privData->outputPacketSize);
}

/*
 *
 */
int USBPixelnet_MaxChannels(void *data)
{
	return 4096;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput USBPixelnetOutput = {
	.maxChannels  = USBPixelnet_MaxChannels,
	.open         = USBPixelnet_Open,
	.close        = USBPixelnet_Close,
	.isConfigured = USBPixelnet_IsConfigured,
	.isActive     = USBPixelnet_IsActive,
	.send         = USBPixelnet_SendData
	};

