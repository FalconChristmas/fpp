
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../log.h"
#include "../settings.h"

#include "SPIws2801.h"

#ifdef USEWIRINGPI
#   include "wiringPi.h"
#   include "wiringPiSPI.h"
#else
#   define wiringPiSPISetup(a,b)    1
#   define wiringPiSetupSys()       0
#   define wiringPiSPIDataRW(a,b,c) 1
#   define delayMicroseconds(a)     0
#endif

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

	SPIws2801PrivData *privData = malloc(sizeof(SPIws2801PrivData));
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

			if (!strcmp(tmp, "spidevice")) {
				LogDebug(VB_CHANNELOUT, "Using %s for SPI output\n", div);
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
	wiringPiSetupSys();

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

	if (channelCount > 510) {
		LogErr(VB_CHANNELOUT,
			"SPIws2801_SendData() tried to send %d bytes when max is 510\n",
			channelCount);
		return 0;
	}

	wiringPiSPIDataRW(privData->port, channelData, channelCount);
}

/*
 *  *
 *   */
int SPIws2801_MaxChannels(void *data)
{
	return 510;
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
	.send         = SPIws2801_SendData
	};

