/*
 *   WS2801 SPI handler for Falcon Pi Player (FPP)
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "settings.h"

#include "SPIws2801.h"

#ifdef USEWIRINGPI
#   include "wiringPi.h"
#   include "wiringPiSPI.h"
#else
#   define wiringPiSPISetup(a,b)    1
#   define wiringPiSPIDataRW(a,b,c) 1
#   define delayMicroseconds(a)     0
#endif

#define SPIWS2801_MAX_CHANNELS  1530

/////////////////////////////////////////////////////////////////////////////

typedef struct spiWS2801PrivData {
	int  port;
} SPIws2801PrivData;

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void SPIws2801_Dump(SPIws2801PrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	LogDebug(VB_CHANNELOUT, "    port    : %d\n", privData->port);
}

/*
 *
 */
int SPIws2801_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "SPIws2801_Open('%s')\n", configStr);

	SPIws2801PrivData *privData =
		(SPIws2801PrivData *)malloc(sizeof(SPIws2801PrivData));
	bzero(privData, sizeof(SPIws2801PrivData));
	privData->port = 0;

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
				LogDebug(VB_CHANNELOUT, "Using %s for SPI output\n", div);
				strcpy(deviceName, div);
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

	if (!sscanf(deviceName, "spidev0.%d", &privData->port))
	{
		LogErr(VB_CHANNELOUT, "Unable to parse SPI device name: %s\n", deviceName);
		free(privData);
		return 0;
	}

	LogDebug(VB_CHANNELOUT, "Using SPI Port %d\n", privData->port);

	if (wiringPiSPISetup(privData->port, 1000000) < 0)
	{
		LogErr(VB_CHANNELOUT, "Unable to open SPI device\n") ;
		return 0;
	}

	SPIws2801_Dump(privData);

	*privDataPtr = privData;

	return 1;
}

/*
 *
 */
int SPIws2801_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "SPIws2801_Close(%p)\n", data);

	SPIws2801PrivData *privData = (SPIws2801PrivData*)data;
	SPIws2801_Dump(privData);

	privData->port = -1;
}

/*
 *
 */
int SPIws2801_IsConfigured(void) {
//  FIXME, needs work once new UI is figured out
//	if ((strcmp(getUSBDonglePort(),"DISABLED")) &&
//		(!strcmp(getUSBDongleType(), "Pixelnet")))
//		return 1;

	return 0;
}

/*
 *
 */
int SPIws2801_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "SPIws2801_IsActive(%p)\n", data);
	SPIws2801PrivData *privData = (SPIws2801PrivData*)data;

	if (!privData)
		return 0;

	SPIws2801_Dump(privData);

	if (privData->port >= 0)
		return 1;

	return 0;
}

/*
 *
 */
int SPIws2801_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "SPIws2801_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	SPIws2801PrivData *privData = (SPIws2801PrivData*)data;

	if (channelCount > SPIWS2801_MAX_CHANNELS) {
		LogErr(VB_CHANNELOUT,
			"SPIws2801_SendData() tried to send %d bytes when max is %d\n",
			channelCount, SPIWS2801_MAX_CHANNELS);
		return 0;
	}

	wiringPiSPIDataRW(privData->port, (unsigned char *)channelData, channelCount);
}

/*
 *  *
 *   */
int SPIws2801_MaxChannels(void *data)
{
	return SPIWS2801_MAX_CHANNELS;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput SPIws2801Output = {
	.maxChannels  = SPIws2801_MaxChannels,
	.open         = SPIws2801_Open,
	.close        = SPIws2801_Close,
	.isConfigured = SPIws2801_IsConfigured,
	.isActive     = SPIws2801_IsActive,
	.send         = SPIws2801_SendData,
	.startThread  = NULL,
	.stopThread   = NULL,
	};

