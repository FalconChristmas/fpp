/*
 *   Renard USB handler for Falcon Pi Player (FPP)
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

#include "log.h"
#include "settings.h"
#include "serialutil.h"

#include "USBRenard.h"

/////////////////////////////////////////////////////////////////////////////

typedef struct usbRenardPrivData {
	char filename[1024];
	char *outputData;
	int  fd;
	int  maxChannels;
	int  speed;
	char parm[4];
} USBRenardPrivData;

// Assume clocks are accurate to 1%, so insert a pad byte every 100 bytes.
#define PAD_DISTANCE 100

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void USBRenard_Dump(USBRenardPrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	LogDebug(VB_CHANNELOUT, "    filename   : %s\n", privData->filename);
	LogDebug(VB_CHANNELOUT, "    fd         : %d\n", privData->fd);
	LogDebug(VB_CHANNELOUT, "    maxChannels: %i\n", privData->maxChannels);
	LogDebug(VB_CHANNELOUT, "    speed      : %d\n", privData->speed);
	LogDebug(VB_CHANNELOUT, "    serial Parm: %s\n", privData->parm);
}

/*
 *
 */
int USBRenard_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "USBRenard_Open('%s')\n", configStr);

	USBRenardPrivData *privData =
		(USBRenardPrivData *)malloc(sizeof(USBRenardPrivData));
	if (privData == NULL)
	{
		LogErr(VB_CHANNELOUT, "Error %d allocating private memory: %s\n",
			errno, strerror(errno));
		
		return 0;
	}
	bzero(privData, sizeof(USBRenardPrivData));
	privData->fd = -1;

	char deviceName[32];
	char *s = strtok(configStr, ";");

	strcpy(deviceName, "UNKNOWN");
	strcpy(privData->parm, "8N1");

	while (s) {
		char tmp[128];
		char *div = NULL;

		strcpy(tmp, s);
		div = strchr(tmp, '=');

		if (div) {
			*div = '\0';
			div++;

			if (!strcmp(tmp, "device")) {
				LogDebug(VB_CHANNELOUT, "Using %s for Renard output\n", div);
				strcpy(deviceName, div);
			} else if (!strcmp(tmp, "renardspeed")) {
				privData->speed = strtoll(div, NULL, 10);
				if (privData->speed) {
					LogDebug(VB_CHANNELOUT, "Sending Renard at %d speed\n", privData->speed);
				} else {
					privData->speed = 57600;
					LogWarn(VB_CHANNELOUT,
						"Unable to parse Renard speed, falling back to %d\n", privData->speed);
				}
			} else if (!strcmp(tmp, "renardparm")) {
				if (strlen(div) == 3)
					strcpy(privData->parm, div);
				else
					LogWarn(VB_CHANNELOUT, "Invalid length on serial parameters: %s\n", div);
			}
		}
		s = strtok(NULL, ";");
	}

	if (!strcmp(deviceName, "UNKNOWN"))
	{
		LogErr(VB_CHANNELOUT, "Invalid Config Str: %s\n", configStr);
		free(privData);
		return 0;
	}

	strcpy(privData->filename, "/dev/");
	strcat(privData->filename, deviceName);

	LogInfo(VB_CHANNELOUT, "Opening %s for Renard output\n",
		privData->filename);

	privData->fd = SerialOpen(privData->filename, privData->speed,
		privData->parm);

	if (privData->fd < 0)
	{
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, privData->filename, strerror(errno));

		free(privData);
		return 0;
	}
	
	privData->outputData = (char *)malloc(USBRenard_MaxChannels(privData));
	if (privData->outputData == NULL)
	{
		LogErr(VB_CHANNELOUT, "Error %d allocating channel memory: %s\n",
			errno, strerror(errno));

		free(privData);
		return 0;
	}
	bzero(privData->outputData, privData->maxChannels);

	USBRenard_Dump(privData);

	*privDataPtr = privData;

	return 1;
}

/*
 *
 */
int USBRenard_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "USBRenard_Close(%p)\n", data);

	USBRenardPrivData *privData = (USBRenardPrivData*)data;
	USBRenard_Dump(privData);

	SerialClose(privData->fd);
	privData->fd = -1;
}

/*
 *
 */
int USBRenard_IsConfigured(void) {
	return 0;
}

/*
 *
 */
int USBRenard_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "USBRenard_IsActive(%p)\n", data);
	USBRenardPrivData *privData = (USBRenardPrivData*)data;

	if (!privData)
		return 0;

	USBRenard_Dump(privData);

	if (privData->fd > 0)
		return 1;

	return 0;
}

/*
 *
 */
int USBRenard_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "USBRenard_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	USBRenardPrivData *privData = (USBRenardPrivData*)data;

	if (channelCount <= privData->maxChannels) {
		bzero(privData->outputData, privData->maxChannels);
	} else {
		LogErr(VB_CHANNELOUT,
			"USBRenard_SendData() tried to send %d bytes when max is 4096\n",
			channelCount);
		return 0;
	}

	memcpy(privData->outputData, channelData, channelCount);

	// Act like "Renard (modified)" and don't output special codes.  There are
	// 3 we need to worry about.
	// 0x7D - Pad Byte    - map to 0x7C
	// 0x7E - Sync Byte   - map to 0x7C
	// 0x7F - Escape Byte - map to 0x80
	char *dptr = privData->outputData;
	int i = 0;
	for( i = 0; i < privData->maxChannels; i++ ) {
		if (*dptr == '\x7D')
			*dptr = '\x7C';
		else if (*dptr == '\x7E')
			*dptr = '\x7C';
		else if (*dptr == '\x7F')
			*dptr = '\x80';

		dptr++;
	}

	// Send start of packet byte
	write(privData->fd, "\x7E\x80", 2);

	dptr = privData->outputData;

	// Assume clocks are accurate to 1%, so insert a pad byte every 100 bytes.
	for ( i = 0; i*PAD_DISTANCE < channelCount; i++ )
	{
		// Send our pad byte
		write(privData->fd, "\x7D", 1);

		// Send Renard Data (Only send the channels we're given, not max)
		if ( (i+1)*PAD_DISTANCE > channelCount )
			write(privData->fd, dptr, (channelCount - (i * PAD_DISTANCE)));
		else
			write(privData->fd, dptr, PAD_DISTANCE);

		dptr += PAD_DISTANCE;
	}
}

/*
 * Data for this was gathered from the DIYC wiki pages on Renard channels at
 * 50ms refresh rate.  For the newer (faster) speeds the PX1 page was
 * referenced.
 *
 * http://www.doityourselfchristmas.com/wiki/index.php?title=Renard#Number_of_Circuits_.28Channels.29_per_Serial_Port
 * http://www.doityourselfchristmas.com/wiki/index.php?title=Renard_PX1_Pixel_Controller#Maximum_Number_of_Pixels_per_Controller
 *
 */
int USBRenard_MaxChannels(void *data)
{
	USBRenardPrivData *privData = (USBRenardPrivData*)data;

	if (privData->maxChannels != 0)
		return privData->maxChannels;
	
	switch (privData->speed) {
		case 921600:	privData->maxChannels = 4584;
						break;
		case 460800:	privData->maxChannels = 2292;
						break;
		case 230400:	privData->maxChannels = 1146;
						break;
		case 115200:	privData->maxChannels = 574;
						break;
		case  57600:	privData->maxChannels = 286;
						break;
		case  38400:	privData->maxChannels = 190;
						break;
		case  19200:	privData->maxChannels = 94;
						break;
	}
	
	return privData->maxChannels;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput USBRenardOutput = {
	.maxChannels  = USBRenard_MaxChannels,
	.open         = USBRenard_Open,
	.close        = USBRenard_Close,
	.isConfigured = USBRenard_IsConfigured,
	.isActive     = USBRenard_IsActive,
	.send         = USBRenard_SendData,
	.startThread  = NULL,
	.stopThread   = NULL,
	};

