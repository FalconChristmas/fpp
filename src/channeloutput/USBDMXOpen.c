/*
 *   USB open DMX handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

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
	int  fd;
	char filename[1024];
	char outputData[513];
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
}

/*
 *
 */
int USBDMXOpen_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "USBDMXOpen_Open('%s')\n", configStr);

	USBDMXOpenPrivData *privData = malloc(sizeof(USBDMXOpenPrivData));
	bzero(privData, sizeof(USBDMXOpenPrivData));
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

	privData->outputData[0] = '\0';
	
	privData->fd = SerialOpen(privData->filename, 250000, "8N2");
	if (privData->fd < 0)
	{
		free(privData);
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, privData->filename, strerror(errno));
		return 0;
	}

	USBDMXOpen_Dump(privData);

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
		bzero(privData->outputData + 1, 512);
	}

	memcpy(privData->outputData + 1, channelData, channelCount);

	// DMX512-A-2004 recommends 176us minimum
	SerialSendBreak(privData->fd, 200);

	// Then need to sleep a minimum of 8us
	usleep(20);

	write(privData->fd, privData->outputData, 513);
}

/*
 *
 */
int USBDMXOpen_MaxChannels(void *data)
{
	return 512;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput USBDMXOpenOutput = {
	.maxChannels  = USBDMXOpen_MaxChannels,
	.open         = USBDMXOpen_Open,
	.close        = USBDMXOpen_Close,
	.isConfigured = USBDMXOpen_IsConfigured,
	.isActive     = USBDMXOpen_IsActive,
	.send         = USBDMXOpen_SendData
	};

