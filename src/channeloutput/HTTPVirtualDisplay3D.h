#pragma once

#include <atomic>
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
#include <vector>

#include "VirtualDisplayBase.h"

#define HTTPVIRTUALDISPLAY3DPORT 32329

class HTTPVirtualDisplay3DOutput : public VirtualDisplayBaseOutput {
public:
    HTTPVirtualDisplay3DOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~HTTPVirtualDisplay3DOutput();

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

    int m_socket;

    std::string m_sseData;

    std::atomic<bool> m_running;
    std::atomic<bool> m_connListChanged;
    std::thread* m_connThread;
    std::thread* m_selectThread;

    std::mutex m_connListLock;
    std::vector<int> m_connList;
};
