#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

// Serial plumbing shared by the LOR and LOR Enhanced outputs.  Each is its
// own plugin and compiles this in; it is not part of the plugin API.

#include <sys/ioctl.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unistd.h>

#include "../common.h"

#include "SerialChannelOutput.h"

#define LOR_HEARTBEAT_SIZE 5

// Units go inactive after 2 seconds without a heartbeat, blank their
// outputs, and can need several heartbeats before they act on commands
// again, so the heartbeat runs for as long as the port is open, not only
// while frames are being sent.
#define LOR_HEARTBEAT_INTERVAL 300000

// Marks a channel whose state on the unit is not known: never sent since
// the output opened, or its command was cut off by a short write.
#define LOR_UNKNOWN 0x100

// A frame's commands go out in one write(), capped below what the tty can
// queue so the non-blocking fd does not drop the tail of a batch.
#define LOR_MAX_BATCH_SIZE 4096

// Frame interval assumed for the first frame, and the range a measured
// interval is clamped to when sizing a frame's byte budget.
#define LOR_DEFAULT_FRAME_TIME 50000
#define LOR_MIN_FRAME_TIME 10000
#define LOR_MAX_FRAME_TIME 100000

// Shortest time the background refresh takes to resend every channel.
#define LOR_REFRESH_PERIOD 1000000

class LORSerialOutput : public SerialChannelOutput {
public:
    virtual ~LORSerialOutput() {
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

    // Bytes the line can carry before the next frame, less whatever is
    // still queued in the tty.  Writing more only builds a backlog that
    // delays the lights behind the audio, and once the tty is full the
    // non-blocking write drops bytes.  Call once per frame; it also records
    // the frame interval in frameTime.
    int FrameBudget() {
        long long now = GetMonotonicTimeMicros();
        long long ft = lastFrame ? now - lastFrame : LOR_DEFAULT_FRAME_TIME;
        lastFrame = now;
        frameTime = std::clamp(ft, (long long)LOR_MIN_FRAME_TIME, (long long)LOR_MAX_FRAME_TIME);
        long long budget = (long long)speed / 10 * frameTime / 1000000;
        int queued = 0;
        if (ioctl(getFD(), TIOCOUTQ, &queued) == 0 && queued > 0) {
            budget -= queued;
        }
        return (int)std::clamp(budget, 0LL, (long long)LOR_MAX_BATCH_SIZE);
    }

    // Writes a frame's batch, serialised against the heartbeat so the
    // heartbeat always lands between frames.  Returns the bytes written,
    // never negative; anything short of len did not go out.
    int WriteBatch(const unsigned char* buf, int len) {
        std::unique_lock<std::mutex> lk(lock);
        int written = write(getFD(), buf, len);
        return written < 0 ? 0 : written;
    }

    int speed = 19200;
    long long frameTime = LOR_DEFAULT_FRAME_TIME;

private:
    void HeartbeatLoop() {
        static const unsigned char heartbeat[LOR_HEARTBEAT_SIZE] = { 0x00, 0xFF, 0x81, 0x56, 0x00 };
        std::unique_lock<std::mutex> lk(lock);
        while (!stopHeartbeat) {
            // monotonic: on the wall clock a step back stopped the heartbeat
            // for the size of the step, and the controllers dropped out
            long long now = GetMonotonicTimeMicros();
            if (now - lastHeartbeat >= LOR_HEARTBEAT_INTERVAL) {
                write(getFD(), heartbeat, LOR_HEARTBEAT_SIZE);
                lastHeartbeat = now;
            }
            heartbeatCond.wait_for(lk, std::chrono::milliseconds(50));
        }
    }

    long long lastFrame = 0;
    long long lastHeartbeat = 0;
    std::mutex lock;
    std::condition_variable heartbeatCond;
    std::thread heartbeatThread;
    bool stopHeartbeat = false;
};
