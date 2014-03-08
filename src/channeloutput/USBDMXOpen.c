
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../log.h"
#include "../settings.h"

#include "USBDMXOpen.h"

/////////////////////////////////////////////////////////////////////////////

typedef struct usbDMXOpenPrivData {
	char filename[1024];
	char outputData[512];
	int  fd;
	char dmxHeader[1];
} USBDMXOpenPrivData;

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void USBDMXOpen_Dump(USBDMXOpenPrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	LogDebug(VB_CHANNELOUT, "    filename : %s\n", privData->filename);
	LogDebug(VB_CHANNELOUT, "    fd       : %d\n", privData->fd);
	LogDebug(VB_CHANNELOUT, "    dmxHeader: %02x\n", privData->dmxHeader[0]);
}

/*
 *
 */
int USBDMXOpen_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "USBDMXOpen_Open('%s')\n", configStr);

	USBDMXOpenPrivData *privData = malloc(sizeof(USBDMXOpenPrivData));
	bzero(privData, sizeof(USBDMXOpenPrivData));
	privData->fd = -1;

	strcpy(privData->filename, "/dev/");
	strcat(privData->filename, configStr);
	
	privData->fd = SerialOpen(privData->filename, 250000, "8E2");
	if (privData->fd < 0)
	{
		free(privData);
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, privData->filename, strerror(errno));
		return 0;
	}

	USBDMXOpen_Dump(privData);

	privData->dmxHeader[0] = 0x00;

	*privDataPtr = privData;

	return 1;
}

/*
 *
 */
int USBDMXOpen_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "USBDMXOpen_Close(%p)\n", data);

	USBDMXOpenPrivData *privData = (USBDMXOpenPrivData*)data;
	USBDMXOpen_Dump(privData);

	SerialClose(privData->fd);
	privData->fd = -1;
}

/*
 *
 */
int USBDMXOpen_IsConfigured(void) {
	if ((strcmp(getUSBDonglePort(),"DISABLED")) &&
		(!strcmp(getUSBDongleType(), "DMXOpen")))
		return 1;

	return 0;
}

/*
 *
 */
int USBDMXOpen_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "USBDMXOpen_IsActive(%p)\n", data);
	USBDMXOpenPrivData *privData = (USBDMXOpenPrivData*)data;

	if (!privData)
		return 0;

	USBDMXOpen_Dump(privData);

	if (privData->fd > 0)
		return 1;

	return 0;
}

/*
 *
 */
int USBDMXOpen_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "USBDMXOpen_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	USBDMXOpenPrivData *privData = (USBDMXOpenPrivData*)data;

	if (channelCount > 512) {
		LogErr(VB_CHANNELOUT,
			"USBDMXOpen_SendData() tried to send %d bytes when max is 512\n",
			channelCount);
		return 0;
	}

	if (channelCount < 512) {
		bzero(privData->outputData, 512);
		memcpy(privData->outputData, channelData, channelCount);
	}

	// DMX512-A-2004 recommends 176us minimum
	SerialSendBreak(privData->fd, 200);

	// Need to sleep a minimum of 8us
	usleep(20);

	write(privData->fd, privData->dmxHeader, sizeof(privData->dmxHeader));

	if (channelCount == 512)
		write(privData->fd, channelData, 512);
	else
		write(privData->fd, privData->outputData, 512);
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput USBDMXOpenOutput = {
	.maxChannels  = 512,
	.open         = USBDMXOpen_Open,
	.close        = USBDMXOpen_Close,
	.isConfigured = USBDMXOpen_IsConfigured,
	.isActive     = USBDMXOpen_IsActive,
	.send         = USBDMXOpen_SendData
	};

