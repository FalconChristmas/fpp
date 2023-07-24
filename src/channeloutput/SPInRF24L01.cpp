/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../log.h"
#include "../settings.h"

#include "SPInRF24L01.h"

#ifdef USENRF
#include "RF24.h"
#else
#include "stdint.h"

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
    RF24(int, int, int) {}
    ~RF24() {}
    void begin() {}
    void setDataRate(int) {}
    int getDataRate(void) { return RF24_250KBPS; }
    void setRetries(int, int) {}
    void setPayloadSize(int) {}
    void setAutoAck(int) {}
    void setChannel(int&) {}
    void setCRCLength(int) {}
    void openWritingPipe(const uint64_t&) {}
    void openReadingPipe(int, const uint64_t&) {}
    void setPALevel(int) {}
    void flush_tx() {}
    void powerUp() {}
    void printDetails() {}
    void write(char*, int) {}
};

#endif

//TODO: Check max channels
#define SPINRF24L01_MAX_CHANNELS 512
const uint64_t pipes[2] = {
    0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL
};

#include "Plugin.h"
class SPInRF24L01Plugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    SPInRF24L01Plugin() :
        FPPPlugins::Plugin("SPInRF24L01") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new SPInRF24L01Output(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new SPInRF24L01Plugin();
}
}

/////////////////////////////////////////////////////////////////////////////

class SPInRF24L01PrivData {
public:
    SPInRF24L01PrivData() :
        radio(nullptr),
        speed(RF24_250KBPS),
        channel(0) {}
    RF24* radio;
    rf24_datarate_e speed;
    int channel;
};

/////////////////////////////////////////////////////////////////////////////

SPInRF24L01Output::SPInRF24L01Output(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    data(nullptr) {
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

    RF24* radio = new RF24(RPI_V2_GPIO_P1_15, RPI_V2_GPIO_P1_26, BCM2835_SPI_SPEED_8MHZ);
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
    radio->setRetries(0, 0);
    radio->setPayloadSize(32);
    radio->setAutoAck(0);
    radio->setChannel(data->channel);
    radio->setCRCLength(RF24_CRC_16);

    radio->openWritingPipe(pipes[0]);
    radio->openReadingPipe(1, pipes[1]);
    radio->setPALevel(RF24_PA_MAX);

    radio->flush_tx();
    radio->powerUp();

    data->radio = radio;

    if (WillLog(LOG_DEBUG, VB_CHANNELOUT)) {
        data->radio->printDetails();
    }

    return ChannelOutput::Init(config);
}

int SPInRF24L01Output::Close(void) {
    LogDebug(VB_CHANNELOUT, "SPInRF24L01Output::Close\n");

    delete data->radio;
    data->radio = NULL;
    return ChannelOutput::Close();
}

int SPInRF24L01Output::SendData(unsigned char* channelData) {
    char packet[32];
    int i = 0;
    RF24* radio = data->radio;

    for (i = 0; i * 30 < m_channelCount; i++) {
        LogDebug(VB_CHANNELOUT,
                 "Shipping off packet %d (offset: %d/%d, size: %d)\n",
                 i, i * 30, m_channelCount,
                 ((i * 30) + 30 > m_channelCount ? m_channelCount - i * 30 : 30));

        bzero(packet, sizeof(packet));

        memcpy(packet, channelData + (i * 30), ((i * 30) + 30 > m_channelCount ? m_channelCount - i * 30 : 30));
        packet[30] = i;

        radio->write(packet, 32);
    }
    return m_channelCount;
}

void SPInRF24L01Output::DumpConfig(void) {
    ChannelOutput::DumpConfig();
    if (data) {
        if (data->speed == RF24_2MBPS)
            LogDebug(VB_CHANNELOUT, "    speed   : 2MBPS\n");
        else if (data->speed == RF24_1MBPS)
            LogDebug(VB_CHANNELOUT, "    speed   : 1MBPS\n");
        else if (data->speed == RF24_250KBPS)
            LogDebug(VB_CHANNELOUT, "    speed   : 250KBPS\n");
        LogDebug(VB_CHANNELOUT, "    channel : %d\n", data->channel);
    }
}

void SPInRF24L01Output::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}
