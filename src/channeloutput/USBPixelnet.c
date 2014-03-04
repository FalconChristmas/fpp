
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
	char filename[1024];
	char outputData[4096];
	int  fd;
} USBPixelnetPrivData;

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void USBPixelnet_Dump(USBPixelnetPrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	LogDebug(VB_CHANNELOUT, "    filename: %s\n", privData->filename);
	LogDebug(VB_CHANNELOUT, "    fd      : %d\n", privData->fd);
}

/*
 *
 */
int USBPixelnet_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "USBPixelnet_Open('%s')\n", configStr);

	USBPixelnetPrivData *privData = malloc(sizeof(USBPixelnetPrivData));
	bzero(privData, sizeof(USBPixelnetPrivData));
	privData->fd = -1;

	strcpy(privData->filename, "/dev/");
	strcat(privData->filename, configStr);

	LogInfo(VB_CHANNELOUT, "Opening %s for Pixelnet output\n",
		privData->filename);

	privData->fd = open(privData->filename, O_RDWR | O_NOCTTY | O_SYNC);
	if (privData->fd < 0)
	{
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, privData->filename, strerror(errno));

		free(privData);
		return 0;
	}

	USBPixelnet_Dump(privData);

	// set speed to 115,200 bps, 8n1 (no parity)
	set_interface_attribs(privData->fd, B115200, 0);

	// set non-blocking (if we ever need to read)
	set_blocking(privData->fd, 0);

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

	close(privData->fd);
	privData->fd = -1;
}

/*
 *
 */
int USBPixelnet_IsConfigured(void) {
	if ((strcmp(getUSBDonglePort(),"DISABLED")) &&
		(!strcmp(getUSBDongleType(), "Pixelnet")))
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
		bzero(privData->outputData, 4096);
	} else {
		LogErr(VB_CHANNELOUT,
			"USBPixelnet_SendData() tried to send %d bytes when max is 4096\n",
			channelCount);
		return 0;
	}

	memcpy(privData->outputData, channelData, channelCount);

	// 0xAA is start of Pixelnet packet, so convert 0xAA (170) to 0xAB (171)
	char *dptr = privData->outputData;
	int i = 0;
	for( i = 0; i < 4096; i++ ) {
		if (*dptr == '\xAA')
			*dptr = '\xAB';

		dptr++;
	}

	// Send start of packet byte
	write(privData->fd, "\xAA", 1);

	// Send Pixelnet Data
	write(privData->fd, privData->outputData, 4096);
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput USBPixelnetOutput = {
	.maxChannels  = 4096,
	.open         = USBPixelnet_Open,
	.close        = USBPixelnet_Close,
	.isConfigured = USBPixelnet_IsConfigured,
	.isActive     = USBPixelnet_IsActive,
	.send         = USBPixelnet_SendData
	};

