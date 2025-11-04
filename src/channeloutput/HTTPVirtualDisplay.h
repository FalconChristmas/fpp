#pragma once
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
#include <thread>
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
    int WriteSSEPacket(int fd, std::string data);

    int m_port;
    int m_screenSize;
    int m_updateInterval;  // Send update every N frames (1=every frame, 2=every other frame, etc.)

    int m_socket;

    std::string m_sseData;

    volatile bool m_running;
    volatile bool m_connListChanged;
    std::thread* m_connThread;
    std::thread* m_selectThread;

    std::mutex m_connListLock;
    std::vector<int> m_connList;
};
