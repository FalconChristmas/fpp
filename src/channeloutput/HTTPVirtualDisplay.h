#pragma once

#include <atomic>
#include <cstdint>
#include <map>
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

#include <mutex>
#include "fpp-json-fwd.h"
#include <thread>
#include <utility>
#include <vector>

#include "VirtualDisplayBase.h"

#define HTTPVIRTUALDISPLAYPORT 32328

class HTTPVirtualDisplayOutput : public VirtualDisplayBaseOutput {
public:
    HTTPVirtualDisplayOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~HTTPVirtualDisplayOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int SendData(unsigned char* channelData) override;

    void ConnectionThread(void);
    void SelectThread(void);

private:
    // Returns 1 if the frame was sent, 0 if it was dropped (client is behind
    // but the connection is still good), -1 if the connection is dead.
    // deadlineMS bounds the whole frame's writes across every client: this
    // runs on the channel output thread
    int WriteSSEPacket(int fd, const std::string& data, uint64_t deadlineMS);
    // consecutive frames each client could not take; see SendData()
    std::map<int, int> m_dropCounts;

    int m_port;
    int m_screenSize;
    int m_updateInterval;   // Minimum ms between SSE frames (25 = 40fps)
    long long m_nextSendMS; // GetTimeMS() deadline for the next SSE frame

    // We only send pixels that changed since the last frame, so a payload we
    // fail to deliver takes that pixel state with it.  Remember what each
    // emitted pixel's cached value was before this frame so a dropped frame can
    // roll the cache back and re-send exactly those pixels next time.  Resending
    // *everything* instead would make the payload maximal precisely when the
    // client is already struggling, which spirals.
    std::vector<std::pair<uint32_t, unsigned char>> m_cacheRollback;

    int m_socket;

    std::string m_sseData;

    // Per-frame encoder state, per instance (these were function statics,
    // shared between every display).
    int m_sseId = 0;
    bool m_lastFrameWasBlack = false;
    int m_blackCheckCounter = 0;

    // Each pixel's location key ("xxyy", or "xxxyyy" past 4095) as the page's
    // scaleMap spells it.  Fixed once the layout is loaded, so built in Init()
    // rather than formatted for every changed pixel of every frame.
    struct PixelLoc {
        char s[6];
        uint8_t len;
    };
    std::vector<PixelLoc> m_pixelLocs;
    // Changed pixels of the frame being encoded: their RGB666 colour and pixel
    // index, radix sorted by colour to group them (the tmp pair is the sort's
    // other buffer).  Reused so a frame allocates nothing.
    std::vector<uint32_t> m_changedColor;
    std::vector<uint32_t> m_changedIndex;
    std::vector<uint32_t> m_sortColor;
    std::vector<uint32_t> m_sortIndex;

    std::atomic<bool> m_running;
    std::atomic<bool> m_connListChanged;
    // Set by the connection thread when a client joins; the output thread,
    // which alone touches m_virtualDisplay, clears the cache on its next frame.
    std::atomic<bool> m_resetCache;
    std::thread* m_connThread;
    std::thread* m_selectThread;

    std::mutex m_connListLock;
    std::vector<int> m_connList;
};
