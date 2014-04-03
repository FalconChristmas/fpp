
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../log.h"
#include "../settings.h"

#include "USBDMXPro.h"

/////////////////////////////////////////////////////////////////////////////

typedef struct usbDMXProPrivData {
	char filename[1024];
	int  fd;
	char dmxHeader[5];
	char dmxFooter[1];
} USBDMXProPrivData;

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void USBDMXPro_Dump(USBDMXProPrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	LogDebug(VB_CHANNELOUT, "    filename : %s\n", privData->filename);
	LogDebug(VB_CHANNELOUT, "    fd       : %d\n", privData->fd);
	LogDebug(VB_CHANNELOUT, "    dmxHeader: %02x, %02x, %02x, %02x, %02x\n",
		(unsigned char)privData->dmxHeader[0],
		(unsigned char)privData->dmxHeader[1],
		(unsigned char)privData->dmxHeader[2],
		(unsigned char)privData->dmxHeader[3],
		(unsigned char)privData->dmxHeader[4]);
	LogDebug(VB_CHANNELOUT, "    dmxFooter: %02x\n",
		(unsigned char)privData->dmxFooter[0]);
}

/*
 *
 */
int USBDMXPro_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "USBDMXPro_Open('%s')\n", configStr);

	USBDMXProPrivData *privData = malloc(sizeof(USBDMXProPrivData));
	bzero(privData, sizeof(USBDMXProPrivData));
	privData->fd = -1;

	char deviceName[32];
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
				LogDebug(VB_CHANNELOUT, "Using %s for DMX output\n", div);
				strcpy(deviceName, div);
			}
		}
		s = strtok(NULL, ",");
	}

	if (!strcmp(deviceName, "UNKNOWN"))
	{
		LogErr(VB_CHANNELOUT, "Invalid Config Str: %s\n", configStr);
		free(privData);
		return 0;
	}

	strcpy(privData->filename, "/dev/");
	strcat(privData->filename, deviceName);

	privData->fd = SerialOpen(privData->filename, 115200, "8N1");
	if (privData->fd < 0)
	{
		free(privData);
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, privData->filename, strerror(errno));
		return 0;
	}

	int len = 512; // only support 512 byte DMX for now.
	privData->dmxHeader[0] = 0x7E;
	privData->dmxHeader[1] = 0x06;
	privData->dmxHeader[2] = len & 0xFF;
	privData->dmxHeader[3] = (len >> 8) & 0xFF;
	privData->dmxHeader[4] = 0x00;

	privData->dmxFooter[0] = 0xE7;

	USBDMXPro_Dump(privData);

	*privDataPtr = privData;

	return 1;
}

/*
 *
 */
int USBDMXPro_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "USBDMXPro_Close(%p)\n", data);

	USBDMXProPrivData *privData = (USBDMXProPrivData*)data;
	USBDMXPro_Dump(privData);

	SerialClose(privData->fd);
	privData->fd = -1;
}

/*
 *
 */
int USBDMXPro_IsConfigured(void) {
	if ((strcmp(getUSBDonglePort(),"DISABLED")) &&
		(!strcmp(getUSBDongleType(), "DMXPro")))
		return 1;

	return 0;
}

/*
 *
 */
int USBDMXPro_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "USBDMXPro_IsActive(%p)\n", data);
	USBDMXProPrivData *privData = (USBDMXProPrivData*)data;

	if (!privData)
		return 0;

	USBDMXPro_Dump(privData);

	if (privData->fd > 0)
		return 1;

	return 0;
}

/*
 *
 */
int USBDMXPro_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "USBDMXPro_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	USBDMXProPrivData *privData = (USBDMXProPrivData*)data;

	if (channelCount > 512) {
		LogErr(VB_CHANNELOUT,
			"USBDMXPro_SendData() tried to send %d bytes when max is 512\n",
			channelCount);
		return 0;
	}

	privData->dmxHeader[2] = channelCount & 0xFF;
	privData->dmxHeader[3] = (channelCount >> 8) & 0xFF;

	write(privData->fd, privData->dmxHeader, sizeof(privData->dmxHeader));
	write(privData->fd, channelData, channelCount);
	write(privData->fd, privData->dmxFooter, sizeof(privData->dmxFooter));
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput USBDMXProOutput = {
	.maxChannels  = 512,
	.open         = USBDMXPro_Open,
	.close        = USBDMXPro_Close,
	.isConfigured = USBDMXPro_IsConfigured,
	.isActive     = USBDMXPro_IsActive,
	.send         = USBDMXPro_SendData
	};

