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

#include "LOREnhanced.h"

#define LORE_MAX_PIXELS_PER_UNIT 170
#define LORE_MAX_CHANNELS_PER_UNIT LORE_MAX_PIXELS_PER_UNIT * 3
#define LORE_UNIT_MAX_PACKET 518 // 2 for Unit Address & Enhanced Command header, 510 channels, 4 addressing bytes, 2 null bytes
#define LOR_HEARTBEAT_SIZE 5
#define LOR_ENHANCED_CMD 0x8E

/////////////////////////////////////////////////////////////////////////////

class LOREnhancedOutputUnit {
public:
    LOREnhancedOutputUnit() :
        unitId(1),
        numOfPixels(0),
        lorStartPixel(0),
        fppStartAddr(0) {
        memset(outputBuff, 0, LORE_UNIT_MAX_PACKET);
    }
    int unitId;
    int numOfPixels;
    int lorStartPixel;
    int fppStartAddr;
    unsigned char outputBuff[LORE_UNIT_MAX_PACKET];
};

class LOREnhancedOutputData {
public:
    LOREnhancedOutputData() :
        fd(-1),
        speed(500000),
        lastHeartbeat(0) {
        filename[0] = 0;
    }
    ~LOREnhancedOutputData() {}
    char filename[1024];
    int fd;
    int speed;
    unsigned char intensityMap[256];
    long long lastHeartbeat;
    unsigned char heartbeatData[LOR_HEARTBEAT_SIZE];
    std::vector<LOREnhancedOutputUnit> units;
};

/////////////////////////////////////////////////////////////////////////////

#include "Plugin.h"
class LOREnhancedPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    LOREnhancedPlugin() :
        FPPPlugins::Plugin("LOREnhanced") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new LOREnhancedOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new LOREnhancedPlugin();
}
}

LOREnhancedOutput::LOREnhancedOutput(unsigned int startChannel, unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    data(nullptr) {}
LOREnhancedOutput::~LOREnhancedOutput() {
    if (data) {
        delete data;
    }
}

void LOREnhancedOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

static void LOREnhanced_SetupIntensityMap(LOREnhancedOutputData* privData) {
    int i = 0;

    for (i = 0; i <= 255; i++) {
        if (i == 0)
            privData->intensityMap[i] = 1;
        else
            privData->intensityMap[i] = i;
    }
}

int LOREnhancedOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "LOREnhancedOutput::Close()\n");

    SerialClose(data->fd);
    data->fd = -1;
    return ChannelOutput::Close();
}

void LOREnhancedOutput::DumpConfig(void) {
    ChannelOutput::DumpConfig();
    if (data) {
        LogDebug(VB_CHANNELOUT, "    filename     : %s\n", data->filename);
        LogDebug(VB_CHANNELOUT, "    fd           : %d\n", data->fd);
        LogDebug(VB_CHANNELOUT, "    port speed   : %d\n", data->speed);
        LogDebug(VB_CHANNELOUT, "    lastHeartbeat: %d\n", data->lastHeartbeat);
    }
    for (auto unit : data->units) {
        LogDebug(VB_CHANNELOUT, "    Unit Id     : %d\n", unit.unitId);
        LogDebug(VB_CHANNELOUT, "      Pixels    : %d\n", unit.numOfPixels);
        LogDebug(VB_CHANNELOUT, "      Lor Start : %d\n", unit.lorStartPixel);
        LogDebug(VB_CHANNELOUT, "      FPP Start : %d\n", unit.fppStartAddr);
    }
}

int LOREnhancedOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "LOREnhancedOutput::Init()\n");
    data = new LOREnhancedOutputData();

    std::string deviceName = "UNKNOWN";
    if (config.isMember("device")) {
        deviceName = config["device"].asString();
        LogDebug(VB_CHANNELOUT, "Using %s for LOR output\n", deviceName.c_str());
    }
    if (config.isMember("speed")) {
        data->speed = config["speed"].asInt();
    }

    if (config.isMember("units")) {
        auto root = config["units"];
        for (Json::Value::iterator it = root.begin(); it != root.end(); ++it) {
            LOREnhancedOutputUnit newUnit;
            if ((*it).isMember("unitId")) {
                newUnit.unitId = (*it)["unitId"].asInt();
            }
            if ((*it).isMember("numOfPixels")) {
                newUnit.numOfPixels = (*it)["numOfPixels"].asInt();
            }
            if ((*it).isMember("lorStartPixel")) {
                newUnit.lorStartPixel = (*it)["lorStartPixel"].asInt();
            }
            if ((*it).isMember("fppStartAddr")) {
                newUnit.fppStartAddr = (*it)["fppStartAddr"].asInt();
            }
            data->units.push_back(newUnit);
        }
    }

    if (deviceName == "UNKNOWN") {
        LogErr(VB_CHANNELOUT, "Missing Device Name\n");
        WarningHolder::AddWarning("LOREnhanced: Missing Device Name");
        return 0;
    }

    strcpy(data->filename, "/dev/");
    strcat(data->filename, deviceName.c_str());

    data->fd = SerialOpen(data->filename, data->speed, "8N1");
    if (data->fd < 0) {
        LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
               errno, data->filename, strerror(errno));
        WarningHolder::AddWarning("LOREnhanced: Error opening device: " + deviceName);
        return 0;
    }

    LOREnhanced_SetupIntensityMap(data);

    data->heartbeatData[0] = 0x00;
    data->heartbeatData[1] = 0xFF;
    data->heartbeatData[2] = 0x81;
    data->heartbeatData[3] = 0x56;
    data->heartbeatData[4] = 0x00;

    return ChannelOutput::Init(config);
}

static void LOR_SendHeartbeat(LOREnhancedOutputData* privData) {
    long long now = GetTime();

    // Only send a heartbeat every 300ms
    if (privData->lastHeartbeat > (now - 300000))
        return;

    if (privData->fd > 0) {
        write(privData->fd, privData->heartbeatData, LOR_HEARTBEAT_SIZE);
        privData->lastHeartbeat = now;
    }
}

/*
 * Enhanced Command Format:
 * Start at channel 1, then fill in channel data as the stream describes it.
 * The format is basically Command + Count in one or two bytes, then channel
 * data as needed by the command, repeat through the whole buffer
 *
 * Command Byte [7:0]:
 * [7:5]
 * 000 Straight list of values for the given number of channels
 * 001 Jump the given number channels
 * 010 Repeat the given 3 values the given number of times
 *
 * [4:0]
 * Count - if 11111 then use the next byte instead of the lower bits of the first one
 */

void LOREnhancedOutput::SendUnitData(unsigned char* channelData, LOREnhancedOutputUnit* unit) {
    if (unit->lorStartPixel < 0 || (unit->lorStartPixel * 3) > 510) {
        LogErr(VB_CHANNELOUT, "Invalid LOR Start Pixel. Unit %d, Start Pixel %d\n",
               unit->unitId, unit->lorStartPixel);
        return;
    }

    if ((unit->lorStartPixel - 1) * 3 + unit->numOfPixels * 3 > 510) {
        LogErr(VB_CHANNELOUT, "More than 510 Channels, ie 170 pixels, per unit. Unit %d, Total channels: %d\n",
               unit->unitId, (unit->lorStartPixel - 1) * 3 + unit->numOfPixels * 3);
        return;
    }

    if ((unit->lorStartPixel - 1) * 3 + unit->numOfPixels * 3 + unit->fppStartAddr - 1 > m_channelCount) {
        LogErr(VB_CHANNELOUT, "Invalid FPP Channel Range. Unit %d\n",
               unit->unitId);
        return;
    }

    if (unit->unitId < 1 || unit->unitId > 240) {
        LogErr(VB_CHANNELOUT, "Invalid Unit ID. Unit %d\n",
               unit->unitId);
        return;
    }

    int bufAt = 1;
    memset(unit->outputBuff, 0, LORE_UNIT_MAX_PACKET);

    unit->outputBuff[bufAt++] = unit->unitId;
    unit->outputBuff[bufAt++] = LOR_ENHANCED_CMD;

    int jumpsLeft = (unit->lorStartPixel - 1) * 3;
    while (jumpsLeft > 0) {
        int thisJump = std::min(jumpsLeft, 255);
        jumpsLeft -= thisJump;

        if (thisJump > 30) {
            unit->outputBuff[bufAt++] = 0x3F;
            unit->outputBuff[bufAt++] = thisJump;
        } else {
            unit->outputBuff[bufAt++] = 0x20 | thisJump;
        }
    }

    int remainingChannels = unit->numOfPixels * 3;
    int channelAt = 0;

    while (remainingChannels > 0) {
        int thisChannelCount = std::min(remainingChannels, 255);
        remainingChannels -= thisChannelCount;

        if (thisChannelCount > 30) {
            unit->outputBuff[bufAt++] = 0x1F;
            unit->outputBuff[bufAt++] = thisChannelCount;
        } else {
            unit->outputBuff[bufAt++] = thisChannelCount;
        }

        for (int i = 0; i < thisChannelCount; i++) {
            int channelValue = channelData[unit->fppStartAddr - 1 + channelAt++];
            channelValue = data->intensityMap[channelValue];
            unit->outputBuff[bufAt++] = channelValue;
        }
    }

    write(data->fd, unit->outputBuff, bufAt + 1);
}

int LOREnhancedOutput::SendData(unsigned char* channelData) {
    LogDebug(VB_CHANNELDATA, "LOREnhancedOutput::SendData()\n");

    for (auto unit : data->units) {
        SendUnitData(channelData, &unit);
    }

    LOR_SendHeartbeat(data);
    return m_channelCount;
}
