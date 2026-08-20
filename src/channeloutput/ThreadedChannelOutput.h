#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <string>
#include "fpp-json-fwd.h"
#include <vector>

#include <condition_variable>
#include <mutex>
#include <thread>

#include "ChannelOutput.h"

class ThreadedChannelOutput : public ChannelOutput {
public:
    ThreadedChannelOutput(unsigned int startChannel = 1,
                          unsigned int channelCount = 1);
    virtual ~ThreadedChannelOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;

    void OutputThread(void);

private:
    int Init(void);

protected:
    virtual void DumpConfig(void) override;
    virtual int RawSendData(unsigned char* channelData) = 0;
    virtual void WaitTimedOut() {}
    // Returns false if the worker thread could not be created; Init() fails
    // the output in that case rather than waiting for a thread that will
    // never run.
    bool StartOutputThread(void);
    int StopOutputThread(void);
    int SendOutputBuffer(void);

    unsigned int m_maxWait;
    unsigned int m_threadIsRunning;

    // m_runThread and m_dataWaiting are the worker's wait predicate: every
    // write to either, and every read the worker acts on, is under m_sendLock.
    unsigned int m_runThread;
    volatile unsigned int m_dataWaiting;

    unsigned int m_useDoubleBuffer;

    // Converted from pthread_t/pthread_mutex_t/pthread_cond_t to the
    // std:: equivalents. Default-constructed std::thread is
    // non-joinable (equivalent to the old m_threadID == 0 sentinel), and
    // std::mutex/std::condition_variable need no explicit init/destroy.
    std::thread m_thread;
    std::mutex m_bufLock;
    std::mutex m_sendLock;
    std::condition_variable m_sendCond;

    unsigned char* m_inBuf;
    unsigned char* m_outBuf;
};
