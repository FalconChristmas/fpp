/*
 *   nRF24L01 SPI handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
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

class RF24 {
public:
RF24(int,int,int) {}
~RF24() {}
void begin() {}
void setDataRate(int) {}
int  getDataRate(void) { return RF24_250KBPS;}
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

#endif

//TODO: Check max channels
#define SPINRF24L01_MAX_CHANNELS  512
const uint64_t pipes[2] = {
	  0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL
};


extern "C" {
    SPInRF24L01Output *createSPI_nRF24L01Output(unsigned int startChannel,
                                     unsigned int channelCount) {
        return new SPInRF24L01Output(startChannel, channelCount);
    }
}

/////////////////////////////////////////////////////////////////////////////

class SPInRF24L01PrivData {
public:
    SPInRF24L01PrivData() : radio(nullptr), speed(0), channel(0) {}
	RF24 *radio;
	rf24_datarate_e speed;
	int channel;
};

/////////////////////////////////////////////////////////////////////////////


SPInRF24L01Output::SPInRF24L01Output(unsigned int startChannel, unsigned int channelCount)
: ChannelOutputBase(startChannel, channelCount), data(nullptr)
{
}
SPInRF24L01Output::~SPInRF24L01Output() {
    if (data) {
        delete data;
    }
}

int SPInRF24L01Output::Init(Json::Value config) {
    int chanNum = -1;
    rf24_datarate_e ss = RF24_250KBPS;
    if (config.isMember("speed")) {
        int i = config["speed"].asInt();
        if (i == 0) {
            ss = RF24_250KBPS;
        } else if (i == 1) {
            ss = RF24_1MBPS;
        } else if (i == 2) {
            ss = RF24_2MBPS;
        } else {
            LogErr(VB_CHANNELOUT, "Invalid speed '%s' from config\n", i);
            return 0;
        }
    }
    if (config.isMember("channel")) {
        chanNum = config["channel"].asInt();
    }
    
    if (chanNum > 83 && chanNum < 101) {
        LogWarn(VB_CHANNELOUT, "FCC RESTRICTED FREQUENCY RANGE OF %dMHz\n", 2400 + chanNum);
    }
    
    if (chanNum < 0 || chanNum > 125) {
        LogErr(VB_CHANNELOUT, "Invalid channel '%d'\n", chanNum);
        return 0;
    }

    RF24 *radio = new RF24(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_26, BCM2835_SPI_SPEED_8MHZ);
    if (!radio) {
        LogErr(VB_CHANNELOUT, "Failed to create our radio instance, unable to continue!\n");
        return 0;
    }
    data = new SPInRF24L01PrivData();
    data->channel = chanNum;
    data->speed = ss;

    // This section of code was basically lifted from the initialization in
    // Komby's RF1 library.  This was tested on a Pi using a similar version
    // of the RF24 library and works, so no further investigation to whether
    // or not all of these are required was done.
    radio->begin();
    
    // Attempt to detect whether or not the radio is present:
    radio->setDataRate(RF24_250KBPS);
    if (radio->getDataRate() != RF24_250KBPS) {
        LogErr(VB_CHANNELOUT, "Failed to detect nRF Radio by setting speed to 250k!\n");
        delete data;
        data = nullptr;
        return 0;
    }
    
    radio->setDataRate(data->speed);
    radio->setRetries(0,0);
    radio->setPayloadSize(32);
    radio->setAutoAck(0);
    radio->setChannel(data->channel);
    radio->setCRCLength(RF24_CRC_16);
    
    radio->openWritingPipe(pipes[0]);
    radio->openReadingPipe(1,pipes[1]);
    radio->setPALevel(RF24_PA_MAX);
    
    radio->flush_tx();
    radio->powerUp();
    
    data->radio = radio;
    
    if (logMask & VB_CHANNELOUT && LogLevelIsSet(LOG_DEBUG)) {
        data->radio->printDetails();
    }
    
    return ChannelOutputBase::Init(config);
}

int SPInRF24L01Output::Close(void) {
    LogDebug(VB_CHANNELOUT, "SPInRF24L01Output::Close\n");
    
    delete data->radio;
    data->radio = NULL;
    return ChannelOutputBase::Close();
}

int SPInRF24L01Output::SendData(unsigned char *channelData) {
    char packet[32];
    int i = 0;
    RF24 *radio = data->radio;
    
    for (i = 0; i*30 < m_channelCount; i++) {
        LogDebug(VB_CHANNELOUT,
                 "Shipping off packet %d (offset: %d/%d, size: %d)\n",
                 i, i*30, m_channelCount,
                 ((i*30)+30 > m_channelCount ? m_channelCount - i*30 : 30));
        
        bzero(packet, sizeof(packet));
        
        memcpy(packet, channelData+(i*30), ((i*30)+30 > m_channelCount ? m_channelCount - i*30 : 30));
        packet[30] = i;
        
        radio->write(packet, 32);
    }
    return m_channelCount;
}

void SPInRF24L01Output::DumpConfig(void) {
    ChannelOutputBase::DumpConfig();
    if (data) {
        if ( data->speed == RF24_2MBPS )
            LogDebug(VB_CHANNELOUT, "    speed   : 2MBPS\n");
        else if ( data->speed == RF24_1MBPS )
            LogDebug(VB_CHANNELOUT, "    speed   : 1MBPS\n");
        else if ( data->speed == RF24_250KBPS )
            LogDebug(VB_CHANNELOUT, "    speed   : 250KBPS\n");
        LogDebug(VB_CHANNELOUT, "    channel : %d\n", data->channel);
    }
}

void SPInRF24L01Output::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}


