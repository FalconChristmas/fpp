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

#include "fpp-json.h"

#include <cstdint>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include "../Warnings.h"
#include "../common.h"
#include "../log.h"

#include "LORSerial.h"

#include "LOR.h"

#define LOR_INTENSITY_SIZE 6
#define LOR_MAX_CHANNELS 3840
#define LOR_MAX_UNIT_ID 0xF0
#define LOR_MAX_BATCH_CMDS (LOR_MAX_BATCH_SIZE / LOR_INTENSITY_SIZE)

/////////////////////////////////////////////////////////////////////////////

class LOROutputData : public LORSerialOutput {
public:
    LOROutputData() {
        for (int i = 0; i < LOR_MAX_CHANNELS; i++) {
            lastValue[i] = LOR_UNKNOWN;
        }
    }

    int controllerOffset = 0;
    unsigned char intensityMap[256];

    // What each unit was last sent, or LOR_UNKNOWN.
    uint16_t lastValue[LOR_MAX_CHANNELS];
    // Frame number a channel was last queued in, so the refresh pass does not
    // send a channel twice in one frame.
    uint32_t queuedFrame[LOR_MAX_CHANNELS] = {};
    uint32_t frameNumber = 0;

    // Where the next frame starts looking for changes, so a frame with more
    // changes than fit the budget does not starve the higher channels.
    int scanStart = 0;
    // Next channel the background refresh resends.
    int refreshAt = 0;

    unsigned char batchData[LOR_MAX_BATCH_SIZE];
    int batchChannel[LOR_MAX_BATCH_CMDS];
    int batchCount = 0;
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

// LOR intensity is a whole percent: 0xF0 is off, 0x01 is full, and 1-99%
// are 228 - 2 * percent.  The Pixie manual's intensity table is per percent
// (101 levels), and this is the table LOR's own software was captured using.
// A linear map over 0xF0..0x01 lands low values at or past the off end (so
// the bottom of a fade goes dark) and reads 50% as about 54%.
static void LOR_SetupIntensityMap(LOROutputData* privData) {
    for (int i = 0; i < 256; i++) {
        int percent = (i * 100 + 127) / 255;
        if (percent == 0) {
            privData->intensityMap[i] = 0xF0;
        } else if (percent == 100) {
            privData->intensityMap[i] = 0x01;
        } else {
            privData->intensityMap[i] = 228 - percent * 2;
        }
    }
}

int LOROutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "LOROutput::Close()\n");
    data->StopHeartbeat();
    data->closeSerialPort();
    return ChannelOutput::Close();
}

void LOROutput::DumpConfig(void) {
    ChannelOutput::DumpConfig();
    if (data) {
        data->dumpSerialConfig();
        LogDebug(VB_CHANNELOUT, "    port speed   : %d\n", data->speed);
        LogDebug(VB_CHANNELOUT, "    controllerOffset: %d\n", data->controllerOffset);
    }
}
int LOROutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "LOROutput::Init()\n");
    data = new LOROutputData();

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
    // SendData() refuses (and logged) every frame of an output larger than
    // LOR can address: say so once, here, and don't start it.
    if (m_channelCount > LOR_MAX_CHANNELS) {
        LogErr(VB_CHANNELOUT, "LOR carries at most %d channels, %u configured\n",
               LOR_MAX_CHANNELS, m_channelCount);
        return 0;
    }
    if (!data->setupSerialPort(config, data->speed, "8N1")) {
        return 0;
    }
    int lastUnit = data->controllerOffset + ((std::min(m_channelCount, (unsigned int)LOR_MAX_CHANNELS) + 15) >> 4);
    if (lastUnit > LOR_MAX_UNIT_ID) {
        LogWarn(VB_CHANNELOUT, "LOR: %d channels starting at unit %d run past unit ID %d; channels beyond it are not sent\n",
                m_channelCount, data->controllerOffset + 1, LOR_MAX_UNIT_ID);
    }

    LOR_SetupIntensityMap(data);
    data->StartHeartbeat();

    return ChannelOutput::Init(config);
}

static void LOR_QueueChannel(LOROutputData* privData, int channel, unsigned char value) {
    unsigned char* cmd = &privData->batchData[privData->batchCount * LOR_INTENSITY_SIZE];
    cmd[0] = 0x00;
    cmd[1] = privData->controllerOffset + (channel >> 4) + 1;
    cmd[2] = 0x03;
    cmd[3] = privData->intensityMap[value];
    cmd[4] = 0x80 | (channel & 0x0F);
    cmd[5] = 0x00;
    privData->batchChannel[privData->batchCount++] = channel;
    privData->lastValue[channel] = value;
    privData->queuedFrame[channel] = privData->frameNumber;
}

int LOROutput::SendData(unsigned char* channelData) {
    LogDebug(VB_CHANNELDATA, "LOROutput::SendData()\n");

    if (m_channelCount > LOR_MAX_CHANNELS) {
        LogErr(VB_CHANNELOUT,
               "LOR_SendData() tried to send %d bytes when max is %d at %d baud\n",
               m_channelCount, LOR_MAX_CHANNELS, data->speed);
        return 0;
    }

    // Channels past unit 0xF0 cannot be addressed (higher IDs are reserved,
    // 0xFF is broadcast), so they are left out rather than wrapped.
    int count = std::min((int)m_channelCount, (LOR_MAX_UNIT_ID - data->controllerOffset) * 16);
    if (count <= 0) {
        return m_channelCount;
    }

    // Changes that do not fit the frame's budget stay pending (lastValue is
    // untouched) and go out on a later frame.
    int maxCmds = data->FrameBudget() / LOR_INTENSITY_SIZE;

    data->frameNumber++;
    data->batchCount = 0;

    int i = data->scanStart;
    for (int checked = 0; checked < count && data->batchCount < maxCmds; checked++) {
        if (data->lastValue[i] != channelData[i]) {
            LOR_QueueChannel(data, i, channelData[i]);
        }
        if (++i == count) {
            i = 0;
        }
    }
    data->scanStart = i;

    // With room left over, resend a few channels that have not changed.  A
    // unit that missed a command (line noise, waking up, power cycled) would
    // otherwise hold the wrong value until the channel next changed, which
    // for a static channel is never.  Capped at a quarter of the frame so
    // refresh never crowds out real changes, and to one pass over the
    // channels per LOR_REFRESH_PERIOD so a small setup is not resent
    // constantly.
    int refreshCmds = std::min(maxCmds - data->batchCount, std::max(1, maxCmds / 4));
    refreshCmds = std::min(refreshCmds, (int)((count * data->frameTime + LOR_REFRESH_PERIOD - 1) / LOR_REFRESH_PERIOD));
    for (int checked = 0; checked < count && refreshCmds > 0; checked++) {
        int ch = data->refreshAt;
        if (++data->refreshAt >= count) {
            data->refreshAt = 0;
        }
        if (data->queuedFrame[ch] != data->frameNumber && data->lastValue[ch] != LOR_UNKNOWN) {
            LOR_QueueChannel(data, ch, channelData[ch]);
            refreshCmds--;
        }
    }

    if (data->batchCount) {
        int len = data->batchCount * LOR_INTENSITY_SIZE;
        int written = data->WriteBatch(data->batchData, len);
        if (written < len) {
            // Every command starts with 0x00, which flushes the unit's
            // buffer, so a cut-off command is discarded.  Its channel and
            // every one after it has to be resent.
            for (int c = written / LOR_INTENSITY_SIZE; c < data->batchCount; c++) {
                data->lastValue[data->batchChannel[c]] = LOR_UNKNOWN;
            }
        }
    }
    return m_channelCount;
}
