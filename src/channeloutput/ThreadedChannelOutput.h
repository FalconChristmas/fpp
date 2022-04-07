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
#include <vector>

#include <pthread.h>

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
    int StartOutputThread(void);
    int StopOutputThread(void);
    int SendOutputBuffer(void);

    unsigned int m_maxWait;
    unsigned int m_threadIsRunning;
    unsigned int m_runThread;
    volatile unsigned int m_dataWaiting;
    unsigned int m_useDoubleBuffer;

    pthread_t m_threadID;
    pthread_mutex_t m_bufLock;
    pthread_mutex_t m_sendLock;
    pthread_cond_t m_sendCond;

    unsigned char* m_inBuf;
    unsigned char* m_outBuf;
};
