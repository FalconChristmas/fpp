/*
 *   Light-O-Rama (LOR) channel output driver for Falcon Pi Player (FPP)
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

#include "common.h"
#include "log.h"
#include "settings.h"
#include "serialutil.h"

#include "LOR.h"

#define LOR_INTENSITY_SIZE 6
#define LOR_HEARTBEAT_SIZE 5
#define LOR_MAX_CHANNELS   3840

/////////////////////////////////////////////////////////////////////////////

typedef struct lorPrivData {
	char          filename[1024];
	int           fd;
	int           speed;
	int           maxIntensity;
	long long     lastHeartbeat;
	unsigned char intensityMap[256];
	unsigned char intensityData[LOR_INTENSITY_SIZE];
	unsigned char heartbeatData[LOR_HEARTBEAT_SIZE];
	char          lastValue[LOR_MAX_CHANNELS];
} LORPrivData;

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void LOR_Dump(LORPrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	LogDebug(VB_CHANNELOUT, "    filename     : %s\n", privData->filename);
	LogDebug(VB_CHANNELOUT, "    fd           : %d\n", privData->fd);
	LogDebug(VB_CHANNELOUT, "    port speed   : %d\n", privData->speed);
	LogDebug(VB_CHANNELOUT, "    maxIntensity : %d\n", privData->maxIntensity);
	LogDebug(VB_CHANNELOUT, "    lastHeartbeat: %d\n", privData->lastHeartbeat);
}

/*
 *
 */
void LOR_SetupIntensityMap(LORPrivData *privData) {
	int t = 0;
	int i = 0;

	for (i = 0; i <= privData->maxIntensity; i++)
	{
		t = (int)(100.0 * (double)i/(double)privData->maxIntensity + 0.5);

		if (!t)
			privData->intensityMap[i] = 0xF0;
		else if (t == 100)
			privData->intensityMap[i] = 0x01;
		else
			privData->intensityMap[i] = 228 - (t * 2);
	}
}

/*
 *
 */
int LOR_Open(char *configStr, void **privDataPtr) {
	LogDebug(VB_CHANNELOUT, "LOR_Open('%s')\n", configStr);

	LORPrivData *privData =
		(LORPrivData *)malloc(sizeof(LORPrivData));
	bzero(privData, sizeof(LORPrivData));
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
			} else if (!strcmp(tmp, "speed")) {
				privData->speed = strtoll(div, NULL, 10);
				if (!privData->speed) {
					LogErr(VB_CHANNELOUT, "Invalid speed: %s\n", tmp);
					free(privData);
					return 0;
				}
			}
		}
		s = strtok(NULL, ";");
	}

	if (!strcmp(deviceName, "UNKNOWN"))
	{
		LogErr(VB_CHANNELOUT, "Missing Device Name: %s\n", configStr);
		free(privData);
		return 0;
	}

	if (!privData->speed)
	{
		LogErr(VB_CHANNELOUT, "Missing Port Speed: %s\n", configStr);
		free(privData);
		return 0;
	}

	strcpy(privData->filename, "/dev/");
	strcat(privData->filename, deviceName);

	privData->fd = SerialOpen(privData->filename, privData->speed, "8N1");
	if (privData->fd < 0)
	{
		free(privData);
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, privData->filename, strerror(errno));
		return 0;
	}

	privData->maxIntensity = 255;
	LOR_SetupIntensityMap(privData);

	privData->intensityData[0] = 0x00;
	privData->intensityData[1] = 0x00;
	privData->intensityData[2] = 0x03;
	privData->intensityData[3] = 0x00;
	privData->intensityData[4] = 0x00;
	privData->intensityData[5] = 0x00;

	privData->heartbeatData[0] = 0x00;
	privData->heartbeatData[1] = 0xFF;
	privData->heartbeatData[2] = 0x81;
	privData->heartbeatData[3] = 0x56;
	privData->heartbeatData[4] = 0x00;

	LOR_Dump(privData);

	*privDataPtr = privData;

	return 1;
}

/*
 *
 */
int LOR_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "LOR_Close(%p)\n", data);

	LORPrivData *privData = (LORPrivData*)data;
	LOR_Dump(privData);

	SerialClose(privData->fd);
	privData->fd = -1;
}

/*
 *
 */
int LOR_IsConfigured(void) {
	return 0;
}

/*
 *
 */
int LOR_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "LOR_IsActive(%p)\n", data);
	LORPrivData *privData = (LORPrivData*)data;

	if (!privData)
		return 0;

	LOR_Dump(privData);

	if (privData->fd > 0)
		return 1;

	return 0;
}

/*
 *
 */
void LOR_SendHeartbeat(LORPrivData *privData)
{
	long long now = GetTime();

	// Only send a heartbeat every 300ms
	if (privData->lastHeartbeat > (now - 300000))
		return;

	if (privData->fd > 0)
	{
		write(privData->fd, privData->heartbeatData, LOR_HEARTBEAT_SIZE);
		privData->lastHeartbeat = now;
	}
}

/*
 *
 */
int LOR_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "LOR_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	LORPrivData *privData = (LORPrivData*)data;

	if (channelCount > LOR_MAX_CHANNELS) {
		LogErr(VB_CHANNELOUT,
			"LOR_SendData() tried to send %d bytes when max is %d at %d baud\n",
			channelCount, LOR_MAX_CHANNELS, privData->speed);
		return 0;
	}

	int i = 0;
	for (i = 0; i < channelCount; i++)
	{
		if (privData->lastValue[i] != channelData[i])
		{
			privData->intensityData[1] = i >> 4;
		
			if (privData->intensityData[1] < 0xF0)
				privData->intensityData[1]++;

			privData->intensityData[3] = privData->intensityMap[channelData[i]];
			privData->intensityData[4] = 0x80 | (i % 16);

			write(privData->fd, privData->intensityData, LOR_INTENSITY_SIZE);

			privData->lastValue[i] = channelData[i];
		}
	}

	LOR_SendHeartbeat(privData);
}

/*
 *
 */
int LOR_MaxChannels(void *data)
{
	(void)data;

	return 3840;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput LOROutput = {
	.maxChannels  = LOR_MaxChannels,
	.open         = LOR_Open,
	.close        = LOR_Close,
	.isConfigured = LOR_IsConfigured,
	.isActive     = LOR_IsActive,
	.send         = LOR_SendData,
	.startThread  = NULL,
	.stopThread   = NULL,
	};

