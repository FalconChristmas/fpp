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

#include <sys/ioctl.h>
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <fcntl.h>
#include <mutex>
#include <termios.h>
#include <thread>
#include <unistd.h>

#include "../Warnings.h"
#include "../common.h"
#include "../log.h"

#include "SerialChannelOutput.h"

#include "LOR.h"

#define LOR_INTENSITY_SIZE 6
#define LOR_HEARTBEAT_SIZE 5
#define LOR_MAX_CHANNELS 3840
#define LOR_MAX_UNIT_ID 0xF0

// Marks a channel whose state on the unit is not known: never sent since the
// output opened, or its command was cut off by a short write.
#define LOR_UNKNOWN 0x100

// A frame's commands go out in one write(), capped below what the tty can
// queue so the non-blocking fd never drops the tail of a batch.
#define LOR_MAX_BATCH_SIZE 4096
#define LOR_MAX_BATCH_CMDS (LOR_MAX_BATCH_SIZE / LOR_INTENSITY_SIZE)

// Units go inactive after 2 seconds without a heartbeat and can need several
// before they act on commands again, so the heartbeat runs for as long as the
// port is open, not only while frames are being sent.
#define LOR_HEARTBEAT_INTERVAL 300000

// Frame interval assumed for the first frame, and the range a measured
// interval is clamped to when sizing a frame's byte budget.
#define LOR_DEFAULT_FRAME_TIME 50000
#define LOR_MIN_FRAME_TIME 10000
#define LOR_MAX_FRAME_TIME 100000

// Shortest time the background refresh takes to resend every channel.
#define LOR_REFRESH_PERIOD 1000000

/////////////////////////////////////////////////////////////////////////////

class LOROutputData : public SerialChannelOutput {
public:
    LOROutputData() :
        SerialChannelOutput() {
        for (int i = 0; i < LOR_MAX_CHANNELS; i++) {
            lastValue[i] = LOR_UNKNOWN;
        }
    }
    ~LOROutputData() {
        StopHeartbeat();
    }

    void StartHeartbeat() {
        stopHeartbeat = false;
        heartbeatThread = std::thread([this]() { HeartbeatLoop(); });
    }
    void StopHeartbeat() {
        if (heartbeatThread.joinable()) {
            {
                std::unique_lock<std::mutex> lk(lock);
                stopHeartbeat = true;
            }
            heartbeatCond.notify_all();
            heartbeatThread.join();
        }
    }
    void HeartbeatLoop() {
        static const unsigned char heartbeat[LOR_HEARTBEAT_SIZE] = { 0x00, 0xFF, 0x81, 0x56, 0x00 };
        std::unique_lock<std::mutex> lk(lock);
        while (!stopHeartbeat) {
            long long now = GetTime();
            if (now - lastHeartbeat >= LOR_HEARTBEAT_INTERVAL) {
                // Holding the lock keeps the heartbeat from landing inside a
                // frame's write; it always starts on a command boundary.
                write(getFD(), heartbeat, LOR_HEARTBEAT_SIZE);
                lastHeartbeat = now;
            }
            heartbeatCond.wait_for(lk, std::chrono::milliseconds(50));
        }
    }

    int speed = 19200;
    int controllerOffset = 0;
    long long lastHeartbeat = 0;
    long long lastFrame = 0;
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

    std::mutex lock;
    std::condition_variable heartbeatCond;
    std::thread heartbeatThread;
    bool stopHeartbeat = false;
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

// LOR brightness runs from 0xF0 (off) to 0x01 (full).  This is a linear map
// over that whole range, computed exactly as xLights' LOR outputs do so a
// sequence tested from xLights puts the same bytes on the wire.
static void LOR_SetupIntensityMap(LOROutputData* privData) {
    for (int i = 0; i < 256; i++) {
        privData->intensityMap[i] = (unsigned char)((i / 255.0F) * -0xEF + 0xF0);
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

    // Budget the frame to what the wire can carry before the next one, less
    // whatever is still queued in the tty.  Writing more only builds a backlog
    // that delays the lights behind the audio, and once the tty is full the
    // non-blocking write drops bytes.  Changes that do not fit stay pending
    // (lastValue is untouched) and go out on a later frame.
    long long now = GetTime();
    long long frameTime = data->lastFrame ? now - data->lastFrame : LOR_DEFAULT_FRAME_TIME;
    data->lastFrame = now;
    frameTime = std::clamp(frameTime, (long long)LOR_MIN_FRAME_TIME, (long long)LOR_MAX_FRAME_TIME);
    long long budget = (long long)data->speed / 10 * frameTime / 1000000;
    int queued = 0;
    if (ioctl(data->getFD(), TIOCOUTQ, &queued) == 0 && queued > 0) {
        budget -= queued;
    }
    int maxCmds = (int)std::clamp(budget / LOR_INTENSITY_SIZE, 0LL, (long long)LOR_MAX_BATCH_CMDS);

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
    refreshCmds = std::min(refreshCmds, (int)((count * frameTime + LOR_REFRESH_PERIOD - 1) / LOR_REFRESH_PERIOD));
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
        int written;
        {
            std::unique_lock<std::mutex> lk(data->lock);
            written = write(data->getFD(), data->batchData, len);
        }
        if (written < len) {
            // Every command starts with 0x00, which flushes the unit's
            // buffer, so a cut-off command is discarded.  Its channel and
            // every one after it has to be resent.
            for (int c = std::max(written, 0) / LOR_INTENSITY_SIZE; c < data->batchCount; c++) {
                data->lastValue[data->batchChannel[c]] = LOR_UNKNOWN;
            }
        }
    }
    return m_channelCount;
}
