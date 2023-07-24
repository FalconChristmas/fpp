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

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "../Warnings.h"
#include "../common.h"
#include "../log.h"

#include "serialutil.h"

#include "LOR.h"

#define LOR_INTENSITY_SIZE 6
#define LOR_HEARTBEAT_SIZE 5
#define LOR_MAX_CHANNELS 3840

/////////////////////////////////////////////////////////////////////////////

class LOROutputData {
public:
    LOROutputData() :
        fd(-1),
        speed(19200),
        controllerOffset(0),
        lastHeartbeat(0) {
        filename[0] = 0;
        memset(lastValue, 0, LOR_MAX_CHANNELS);
        memset(intensityData, 0, LOR_INTENSITY_SIZE);
        memset(heartbeatData, 0, LOR_HEARTBEAT_SIZE);
    }
    ~LOROutputData() {}
    char filename[1024];
    int fd;
    int speed;
    int controllerOffset;
    int maxIntensity;
    long long lastHeartbeat;
    unsigned char intensityMap[256];
    unsigned char intensityData[LOR_INTENSITY_SIZE];
    unsigned char heartbeatData[LOR_HEARTBEAT_SIZE];
    char lastValue[LOR_MAX_CHANNELS];
};

/////////////////////////////////////////////////////////////////////////////

#include "Plugin.h"
class LORPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    LORPlugin() :
        FPPPlugins::Plugin("LOR") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new LOROutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new LORPlugin();
}
}

LOROutput::LOROutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    data(nullptr) {}
LOROutput::~LOROutput() {
    if (data) {
        delete data;
    }
}

void LOROutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

static void LOR_SetupIntensityMap(LOROutputData* privData) {
    int t = 0;
    int i = 0;

    for (i = 0; i <= privData->maxIntensity; i++) {
        t = (int)(100.0 * (double)i / (double)privData->maxIntensity + 0.5);

        if (!t)
            privData->intensityMap[i] = 0xF0;
        else if (t == 100)
            privData->intensityMap[i] = 0x01;
        else
            privData->intensityMap[i] = 228 - (t * 2);
    }
}

int LOROutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "LOROutput::Close()\n");

    SerialClose(data->fd);
    data->fd = -1;
    return ChannelOutput::Close();
}

void LOROutput::DumpConfig(void) {
    ChannelOutput::DumpConfig();
    if (data) {
        LogDebug(VB_CHANNELOUT, "    filename     : %s\n", data->filename);
        LogDebug(VB_CHANNELOUT, "    fd           : %d\n", data->fd);
        LogDebug(VB_CHANNELOUT, "    port speed   : %d\n", data->speed);
        LogDebug(VB_CHANNELOUT, "    maxIntensity : %d\n", data->maxIntensity);
        LogDebug(VB_CHANNELOUT, "    lastHeartbeat: %d\n", data->lastHeartbeat);
        LogDebug(VB_CHANNELOUT, "    controllerOffset: %d\n", data->controllerOffset);
    }
}
int LOROutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "LOROutput::Init()\n");
    data = new LOROutputData();

    std::string deviceName = "UNKNOWN";
    if (config.isMember("device")) {
        deviceName = config["device"].asString();
        LogDebug(VB_CHANNELOUT, "Using %s for LOR output\n", deviceName.c_str());
    }
    if (config.isMember("speed")) {
        data->speed = config["speed"].asInt();
    }
    if (config.isMember("firstControllerId")) {
        data->controllerOffset = config["firstControllerId"].asInt();
        // Fix for box off by one error reported in issue 820
        if (data->controllerOffset > 0) {
            data->controllerOffset -= 1;
        }
    }

    if (deviceName == "UNKNOWN") {
        LogErr(VB_CHANNELOUT, "Missing Device Name\n");
        WarningHolder::AddWarning("LOR: Missing Device Name");
        return 0;
    }

    strcpy(data->filename, "/dev/");
    strcat(data->filename, deviceName.c_str());

    data->fd = SerialOpen(data->filename, data->speed, "8N1");
    if (data->fd < 0) {
        LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
               errno, data->filename, strerror(errno));
        WarningHolder::AddWarning("LOR: Error opening device: " + deviceName);
        return 0;
    }

    data->maxIntensity = 255;
    LOR_SetupIntensityMap(data);

    data->intensityData[0] = 0x00;
    data->intensityData[1] = 0x00;
    data->intensityData[2] = 0x03;
    data->intensityData[3] = 0x00;
    data->intensityData[4] = 0x00;
    data->intensityData[5] = 0x00;

    data->heartbeatData[0] = 0x00;
    data->heartbeatData[1] = 0xFF;
    data->heartbeatData[2] = 0x81;
    data->heartbeatData[3] = 0x56;
    data->heartbeatData[4] = 0x00;

    return ChannelOutput::Init(config);
}

/*
 *
 */
static void LOR_SendHeartbeat(LOROutputData* privData) {
    long long now = GetTime();

    // Only send a heartbeat every 300ms
    if (privData->lastHeartbeat > (now - 300000))
        return;

    if (privData->fd > 0) {
        write(privData->fd, privData->heartbeatData, LOR_HEARTBEAT_SIZE);
        privData->lastHeartbeat = now;
    }
}

int LOROutput::SendData(unsigned char* channelData) {
    LogDebug(VB_CHANNELDATA, "LOROutput::SendData()\n");

    if (m_channelCount > LOR_MAX_CHANNELS) {
        LogErr(VB_CHANNELOUT,
               "LOR_SendData() tried to send %d bytes when max is %d at %d baud\n",
               m_channelCount, LOR_MAX_CHANNELS, data->speed);
        return 0;
    }

    int i = 0;
    for (i = 0; i < m_channelCount; i++) {
        if (data->lastValue[i] != channelData[i]) {
            data->intensityData[1] = data->controllerOffset + (i >> 4);

            if (data->intensityData[1] < 0xF0)
                data->intensityData[1]++;

            data->intensityData[3] = data->intensityMap[channelData[i]];
            data->intensityData[4] = 0x80 | (i % 16);

            write(data->fd, data->intensityData, LOR_INTENSITY_SIZE);

            data->lastValue[i] = channelData[i];
        }
    }
    LOR_SendHeartbeat(data);
    return m_channelCount;
}
