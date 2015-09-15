/*
 *   nRF24L01 SPI handler for Falcon Pi Player (FPP)
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

#include "SPInRF24L01.h"

#ifdef USENRF
#	include "RF24.h"
#else
#	include "stdint.h"
class RF24 {
public:
RF24(int,int,int) {}
~RF24() {}
void begin() {}
void setDataRate(int) {}
int  getDataRate(void) {}
void setRetries(int,int) {}
void setPayloadSize(int) {}
void setAutoAck(int) {}
void setChannel(int&) {}
void setCRCLength(int) {}
void openWritingPipe(const uint64_t&) {}
void openReadingPipe(int,const uint64_t&) {}
void setPALevel(int) {}
void flush_tx() {}
void powerUp() {}
void printDetails() {}
void write(char*,int) {}
};
#define rf24_datarate_e int
#define RF24_250KBPS 0
#define RF24_1MBPS 1
#define RF24_2MBPS 7
#define RPI_V2_GPIO_P1_15 2
#define RPI_V2_GPIO_P1_24 3
#define RPI_V2_GPIO_P1_26 7
#define BCM2835_SPI_SPEED_8MHZ 4
#define RF24_CRC_16 5
#define RF24_PA_MAX 6
#endif

//TODO: Check max channels
#define SPINRF24L01_MAX_CHANNELS  512
const uint64_t pipes[2] = {
	  0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL
};

/////////////////////////////////////////////////////////////////////////////

typedef struct spiNRF24L01PrivData {
	RF24 *radio;
	rf24_datarate_e speed;
	int channel;
} SPInRF24L01PrivData;

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void SPInRF24L01_Dump(SPInRF24L01PrivData *privData) {
	LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

	if (!privData)
		return;

	if ( privData->speed == RF24_2MBPS )
		LogDebug(VB_CHANNELOUT, "    speed   : 2MBPS\n");
	else if ( privData->speed == RF24_1MBPS )
		LogDebug(VB_CHANNELOUT, "    speed   : 1MBPS\n");
	else if ( privData->speed == RF24_250KBPS )
		LogDebug(VB_CHANNELOUT, "    speed   : 250KBPS\n");
	LogDebug(VB_CHANNELOUT, "    channel : %d\n", privData->channel);
}

/*
 *
 */
int SPInRF24L01_Open(char *configStr, void **privDataPtr) {
//TODO: Add help to the channeloutput page in UI
	LogDebug(VB_CHANNELOUT, "SPInRF24L01_Open('%s')\n", configStr);

	SPInRF24L01PrivData *privData =
		(SPInRF24L01PrivData *)malloc(sizeof(SPInRF24L01PrivData));
	bzero(privData, sizeof(SPInRF24L01PrivData));

	char speed[2] = {0};
	char channel[4] = {0};

	char *s = strtok(configStr, ";");

	while (s) {
		char tmp[128];
		char *div = NULL;

		strcpy(tmp, s);
		div = strchr(tmp, '=');

		if (div) {
			*div = '\0';
			div++;

			if (!strcmp(tmp, "speed")) {
				LogDebug(VB_CHANNELOUT, "Using %s for nRF speed\n", div);
				strcpy(speed, div);
			}
			else if (!strcmp(tmp, "channel")) {
				LogDebug(VB_CHANNELOUT, "Using %s for nRF channel\n", div);
				strcpy(channel, div);
			}
		}
		s = strtok(NULL, ";");
	}

	if (!strcmp(speed, "0"))
	{
		privData->speed = RF24_250KBPS;
	}
	else if (!strcmp(speed, "1"))
	{
		privData->speed = RF24_1MBPS;
	}
	else if (!strcmp(speed, "2"))
	{
		privData->speed = RF24_2MBPS;
	}
	else
	{
		LogErr(VB_CHANNELOUT, "Invalid speed '%s' parsed from config string: %s\n",
			speed, configStr);
		free(privData);
		return 0;
	}

	privData->channel = atoi(channel);
	
	if (privData->channel > 83 && privData->channel < 101)
	{
		LogWarn(VB_CHANNELOUT, "FCC RESTRICTED FREQUENCY RANGE OF %dMHz\n", 2400 + privData->channel);
	}

	if (privData->channel < 0 || privData->channel > 125)
	{
		LogErr(VB_CHANNELOUT, "Invalid channel '%d' parsed from config string: %s\n",
			privData->channel, configStr);
		free(privData);
		return 0;
	}

	RF24 *radio = new RF24(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_26, BCM2835_SPI_SPEED_8MHZ);
	if (!radio)
	{
		LogErr(VB_CHANNELOUT, "Failed to create our radio instance, unable to continue!\n");
		free(privData);
		return 0;
	}

	// This section of code was basically lifted from the initialization in
	// Komby's RF1 library.  This was tested on a Pi using a similar version
	// of the RF24 library and works, so no further investigation to whether
	// or not all of these are required was done.
	radio->begin();

	// Attempt to detect whether or not the radio is present:
	radio->setDataRate(RF24_250KBPS);
	if ( radio->getDataRate() != RF24_250KBPS )
	{
		LogErr(VB_CHANNELOUT, "Failed to detect nRF Radio by setting speed to 250k!\n");
		free(privData);
		return 0;
	}

	radio->setDataRate(privData->speed);
	radio->setRetries(0,0);
	radio->setPayloadSize(32);
	radio->setAutoAck(0);
	radio->setChannel(privData->channel);
	radio->setCRCLength(RF24_CRC_16);

	radio->openWritingPipe(pipes[0]);
	radio->openReadingPipe(1,pipes[1]);
	radio->setPALevel(RF24_PA_MAX);

	radio->flush_tx();
	radio->powerUp();

	privData->radio = radio;

	SPInRF24L01_Dump(privData);

	if (logMask & VB_CHANNELOUT && logLevel & LOG_DEBUG) {
		privData->radio->printDetails();
	}

	*privDataPtr = privData;

	return 1;
}

/*
 *
 */
int SPInRF24L01_Close(void *data) {
	LogDebug(VB_CHANNELOUT, "SPInRF24L01_Close(%p)\n", data);

	SPInRF24L01PrivData *privData = (SPInRF24L01PrivData*)data;
	SPInRF24L01_Dump(privData);

	delete(privData->radio);
	privData->radio = NULL;
}

/*
 *
 */
int SPInRF24L01_IsConfigured(void) {
//  FIXME, needs work once new UI is figured out
//	if ((strcmp(getUSBDonglePort(),"DISABLED")) &&
//		(!strcmp(getUSBDongleType(), "Pixelnet")))
//		return 1;

	return 0;
}

/*
 *
 */
int SPInRF24L01_IsActive(void *data) {
	LogDebug(VB_CHANNELOUT, "SPInRF24L01_IsActive(%p)\n", data);
	SPInRF24L01PrivData *privData = (SPInRF24L01PrivData*)data;

	if (!privData)
		return 0;

	SPInRF24L01_Dump(privData);

	return 0;
}

/*
 *
 */
int SPInRF24L01_SendData(void *data, char *channelData, int channelCount)
{
	LogDebug(VB_CHANNELDATA, "SPInRF24L01_SendData(%p, %p, %d)\n",
		data, channelData, channelCount);

	char packet[32];
	int i = 0;
	SPInRF24L01PrivData *privData = (SPInRF24L01PrivData*)data;
	RF24 *radio = privData->radio;

	if (channelCount > SPINRF24L01_MAX_CHANNELS) {
		LogErr(VB_CHANNELOUT,
			"SPInRF24L01_SendData() tried to send %d bytes when max is %d\n",
			channelCount, SPINRF24L01_MAX_CHANNELS);
		return 0;
	}
	
	for (i = 0; i*30 < channelCount; i++)
	{
		LogDebug(VB_CHANNELOUT,
			"Shipping off packet %d (offset: %d/%d, size: %d)\n",
			i, i*30, channelCount,
			((i*30)+30 > channelCount ? channelCount - i*30 : 30));

		bzero(packet, sizeof(packet));

		memcpy(packet, channelData+(i*30), ((i*30)+30 > channelCount ? channelCount - i*30 : 30));
		packet[30] = i;
			
		radio->write(packet,32);
	}
}

/*
 *  *
 *   */
int SPInRF24L01_MaxChannels(void *data)
{
	return SPINRF24L01_MAX_CHANNELS;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput SPInRF24L01Output = {
	SPInRF24L01_MaxChannels, //maxChannels
	SPInRF24L01_Open, //open
	SPInRF24L01_Close, //close
	SPInRF24L01_IsConfigured, //isConfigured
	SPInRF24L01_IsActive, //isActive
	SPInRF24L01_SendData, //send
	NULL, //startThread
	NULL, //stopThread
};
