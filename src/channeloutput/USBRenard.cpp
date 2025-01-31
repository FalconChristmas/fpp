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

#include <termios.h>
#include <unistd.h>

#include "../Warnings.h"
#include "../log.h"

#include "SerialChannelOutput.h"
#include "USBRenard.h"

/////////////////////////////////////////////////////////////////////////////

class USBRenardOutputData : public SerialChannelOutput {
public:
    USBRenardOutputData() :
        maxChannels(0),
        speed(0),
        outputData(nullptr) {
    }
    ~USBRenardOutputData() {
        if (outputData) {
            delete[] outputData;
        }
    }
    char* outputData;
    int maxChannels;
    int speed;
    char parm[4];
};

// Assume clocks are accurate to 1%, so insert a pad byte every 100 bytes.
#define PAD_DISTANCE 100

/////////////////////////////////////////////////////////////////////////////
#include "Plugin.h"
class USBRenardPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    USBRenardPlugin() :
        FPPPlugins::Plugin("USBRenard") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new USBRenardOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new USBRenardPlugin();
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
static int USBRenard_MaxChannels(USBRenardOutputData* privData) {
    if (privData->maxChannels != 0)
        return privData->maxChannels;

    switch (privData->speed) {
    case 921600:
        privData->maxChannels = 4584;
        break;
    case 460800:
        privData->maxChannels = 2292;
        break;
    case 230400:
        privData->maxChannels = 1146;
        break;
    case 115200:
        privData->maxChannels = 574;
        break;
    case 57600:
        privData->maxChannels = 286;
        break;
    case 38400:
        privData->maxChannels = 190;
        break;
    case 19200:
        privData->maxChannels = 94;
        break;
    }
    return privData->maxChannels;
}

USBRenardOutput::USBRenardOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    data(nullptr) {
}
USBRenardOutput::~USBRenardOutput() {
    if (data) {
        delete data;
    }
}
void USBRenardOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}
int USBRenardOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "USBRenard_Open()\n");

    data = new USBRenardOutputData();
    strcpy(data->parm, "8N1");
    if (config.isMember("renardspeed")) {
        data->speed = config["renardspeed"].asInt();
        LogDebug(VB_CHANNELOUT, "Sending Renard at %d speed\n", data->speed);
    }
    if (config.isMember("renardparm")) {
        std::string parm = config["renardparm"].asString();
        if (parm.length() != 3) {
            LogWarn(VB_CHANNELOUT, "Invalid length on serial parameters: %s\n", parm.c_str());
        } else {
            strcpy(data->parm, parm.c_str());
        }
    }
    if (!data->setupSerialPort(config, data->speed, data->parm)) {
        return 0;
    }

    data->outputData = new char[USBRenard_MaxChannels(data)];
    if (data->outputData == NULL) {
        LogErr(VB_CHANNELOUT, "Error %d allocating channel memory: %s\n",
               errno, strerror(errno));

        return 0;
    }
    bzero(data->outputData, data->maxChannels);

    return ChannelOutput::Init(config);
}

int USBRenardOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "USBRenard_Close()\n");
    if (data) {
        data->closeSerialPort();
    }
    return ChannelOutput::Close();
}

int USBRenardOutput::SendData(unsigned char* channelData) {
    LogDebug(VB_CHANNELDATA, "USBRenard_SendData()\n");

    if (m_channelCount <= data->maxChannels) {
        bzero(data->outputData, data->maxChannels);
    } else {
        LogErr(VB_CHANNELOUT,
               "USBRenard_SendData() tried to send %d bytes when max is %d\n",
               m_channelCount, data->maxChannels);
        return 0;
    }

    memcpy(data->outputData, channelData, m_channelCount);

    // Act like "Renard (modified)" and don't output special codes.  There are
    // 3 we need to worry about.
    // 0x7D - Pad Byte    - map to 0x7C
    // 0x7E - Sync Byte   - map to 0x7C
    // 0x7F - Escape Byte - map to 0x80
    char* dptr = data->outputData;
    int i = 0;
    for (i = 0; i < data->maxChannels; i++) {
        if (*dptr == '\x7D')
            *dptr = '\x7C';
        else if (*dptr == '\x7E')
            *dptr = '\x7C';
        else if (*dptr == '\x7F')
            *dptr = '\x80';
        dptr++;
    }

    // Send start of packet byte
    write(data->getFD(), "\x7E\x80", 2);
    dptr = data->outputData;

    // Assume clocks are accurate to 1%, so insert a pad byte every 100 bytes.
    for (i = 0; i * PAD_DISTANCE < m_channelCount; i++) {
        // Send our pad byte
        write(data->getFD(), "\x7D", 1);

        // Send Renard Data (Only send the channels we're given, not max)
        if ((i + 1) * PAD_DISTANCE > m_channelCount)
            write(data->getFD(), dptr, (m_channelCount - (i * PAD_DISTANCE)));
        else
            write(data->getFD(), dptr, PAD_DISTANCE);

        dptr += PAD_DISTANCE;
    }
    return m_channelCount;
}

void USBRenardOutput::DumpConfig(void) {
    ChannelOutput::DumpConfig();
    if (!data)
        return;
    data->dumpSerialConfig();
    LogDebug(VB_CHANNELOUT, "    maxChannels: %i\n", data->maxChannels);
    LogDebug(VB_CHANNELOUT, "    speed      : %d\n", data->speed);
    LogDebug(VB_CHANNELOUT, "    serial Parm: %s\n", data->parm);
}
