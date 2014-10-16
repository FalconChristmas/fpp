/*
 *   GPIO Pin Channel Output driver for Falcon Pi Player (FPP)
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

#include "GPIO.h"

#ifdef USEWIRINGPI
#   include "wiringPi.h"
#else
#   define pinMode(a, b)
#   define digitalWrite(a, b)
#endif

/////////////////////////////////////////////////////////////////////////////

typedef struct gpioPrivData {
	int  pin;
} GPIOPrivData;

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void GPIO_Dump(GPIOPrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	LogDebug(VB_CHANNELOUT, "    pin: %d\n", privData->pin);
}

/*
 *
 */
int GPIO_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "GPIO_Open('%s')\n", configStr);

	GPIOPrivData *privData =
		(GPIOPrivData *)malloc(sizeof(GPIOPrivData));
	bzero(privData, sizeof(GPIOPrivData));
	privData->pin = -1;

	char *s = strtok(configStr, ";");

	while (s) {
		char tmp[128];
		char *div = NULL;

		strcpy(tmp, s);
		div = strchr(tmp, '=');

		if (div) {
			*div = '\0';
			div++;

			if (!strcmp(tmp, "gpio")) {
				LogDebug(VB_CHANNELOUT, "Using BCM GPIO %s for output\n", div);

				privData->pin = atoi(div);
			}
		}
		s = strtok(NULL, ";");
	}


	if (privData->pin == -1)
	{
		LogErr(VB_CHANNELOUT, "Invalid Config Str: %s\n", configStr);
		free(privData);
		return 0;
	}

	pinMode(privData->pin, OUTPUT);
	digitalWrite(privData->pin, LOW);

	GPIO_Dump(privData);

	*privDataPtr = privData;

	return 1;
}

/*
 *
 */
int GPIO_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "GPIO_Close(%p)\n", data);

	GPIOPrivData *privData = (GPIOPrivData*)data;
	GPIO_Dump(privData);

	privData->pin = -1;

	free(privData);
}

/*
 *
 */
int GPIO_IsConfigured(void) {
	return 0;
}

/*
 *
 */
int GPIO_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "GPIO_IsActive(%p)\n", data);
	GPIOPrivData *privData = (GPIOPrivData*)data;

	if (!privData)
		return 0;

	GPIO_Dump(privData);

	if (privData->pin >= 0)
		return 1;

	return 0;
}

/*
 *
 */
int GPIO_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "GPIO_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	GPIOPrivData *privData = (GPIOPrivData*)data;

	if (channelCount > 1) {
		LogErr(VB_CHANNELOUT,
			"GPIO_SendData() tried to send %d bytes when max is 1\n",
			channelCount);
		return 0;
	}

	digitalWrite(privData->pin, channelData[0]);
}

/*
 *  *
 *   */
int GPIO_MaxChannels(void *data)
{
	return 1;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput GPIOOutput = {
	.maxChannels  = GPIO_MaxChannels,
	.open         = GPIO_Open,
	.close        = GPIO_Close,
	.isConfigured = GPIO_IsConfigured,
	.isActive     = GPIO_IsActive,
	.send         = GPIO_SendData,
	.startThread  = NULL,
	.stopThread   = NULL,
	};

