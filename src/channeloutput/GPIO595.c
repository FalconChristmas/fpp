/*
 *   GPIO attached 74HC595 Shift Register handler for Falcon Pi Player (FPP)
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

#include "GPIO595.h"

#ifdef USEWIRINGPI
#   include "wiringPi.h"
#else
#   define pinMode(a, b)
#   define digitalWrite(a, b)
#   define delayMicroseconds(a)     0
#endif

#define GPIO595_MAX_CHANNELS  128

/////////////////////////////////////////////////////////////////////////////

typedef struct gpio595PrivData {
	int  clockPin;
	int  dataPin;
	int  latchPin;
} GPIO595PrivData;

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void GPIO595_Dump(GPIO595PrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	LogDebug(VB_CHANNELOUT, "    clockPin: %d\n", privData->clockPin);
	LogDebug(VB_CHANNELOUT, "    dataPin : %d\n", privData->dataPin);
	LogDebug(VB_CHANNELOUT, "    latchPin: %d\n", privData->latchPin);
}

/*
 *
 */
int GPIO595_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "GPIO595_Open('%s')\n", configStr);

	GPIO595PrivData *privData =
		(GPIO595PrivData *)malloc(sizeof(GPIO595PrivData));
	bzero(privData, sizeof(GPIO595PrivData));
	privData->clockPin = -1;
	privData->dataPin = -1;
	privData->latchPin = -1;

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

			if (!strcmp(tmp, "gpio")) {
				LogDebug(VB_CHANNELOUT, "Using GPIO %s for output\n", div);

				if (strlen(div) == 8)
				{
					div[2] = '\0';
					privData->clockPin = atoi(div);

					div += 3;
					div[2] = '\0';
					privData->dataPin = atoi(div);

					div += 3;
					privData->latchPin = atoi(div);
				}
			}
		}
		s = strtok(NULL, ";");
	}


	if ((privData->clockPin == -1) ||
		(privData->dataPin == -1) ||
		(privData->latchPin == -1))
	{
		LogErr(VB_CHANNELOUT, "Invalid Config Str: %s\n", configStr);
		free(privData);
		return 0;
	}

	digitalWrite(privData->clockPin, LOW);
	digitalWrite(privData->dataPin,  LOW);
	digitalWrite(privData->latchPin, HIGH);

	pinMode(privData->clockPin, OUTPUT);
	pinMode(privData->dataPin,  OUTPUT);
	pinMode(privData->latchPin, OUTPUT);

	GPIO595_Dump(privData);

	*privDataPtr = privData;

	return 1;
}

/*
 *
 */
int GPIO595_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "GPIO595_Close(%p)\n", data);

	GPIO595PrivData *privData = (GPIO595PrivData*)data;
	GPIO595_Dump(privData);

	privData->clockPin = -1;
	privData->dataPin = -1;
	privData->latchPin = -1;

	free(privData);
}

/*
 *
 */
int GPIO595_IsConfigured(void) {
	return 0;
}

/*
 *
 */
int GPIO595_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "GPIO595_IsActive(%p)\n", data);
	GPIO595PrivData *privData = (GPIO595PrivData*)data;

	if (!privData)
		return 0;

	GPIO595_Dump(privData);

	if ((privData->clockPin >= 0) &&
		(privData->dataPin >= 0) &&
		(privData->latchPin >= 0))
		return 1;

	return 0;
}

/*
 *
 */
int GPIO595_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "GPIO595_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	GPIO595PrivData *privData = (GPIO595PrivData*)data;

	if (channelCount > GPIO595_MAX_CHANNELS) {
		LogErr(VB_CHANNELOUT,
			"GPIO595_SendData() tried to send %d bytes when max is %d\n",
			channelCount, GPIO595_MAX_CHANNELS);
		return 0;
	}

	// Drop the latch low
	digitalWrite(privData->latchPin, LOW);
	delayMicroseconds(1);

	// Output one bit per channel along with a clock tick
	int i = 0;
	for (i = channelCount - 1; i >= 0; i--)
	{
		// We only support basic On/Off.  non-zero channel value == On
		digitalWrite(privData->dataPin, channelData[i]);

		// Send a clock tick
		digitalWrite(privData->clockPin, HIGH);
		delayMicroseconds(1);
		digitalWrite(privData->clockPin, LOW);
		delayMicroseconds(1);
	}

	// Bring the latch high to push the bits out of the chip
	digitalWrite(privData->latchPin, HIGH);
	delayMicroseconds(1);
}

/*
 *  *
 *   */
int GPIO595_MaxChannels(void *data)
{
	return GPIO595_MAX_CHANNELS;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput GPIO595Output = {
	.maxChannels  = GPIO595_MaxChannels,
	.open         = GPIO595_Open,
	.close        = GPIO595_Close,
	.isConfigured = GPIO595_IsConfigured,
	.isActive     = GPIO595_IsActive,
	.send         = GPIO595_SendData,
	.startThread  = NULL,
	.stopThread   = NULL,
	};

