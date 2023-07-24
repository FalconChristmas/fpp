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

#include "fpp-pch.h"

#include <sys/time.h>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <unistd.h>

#include "../commands/Commands.h"
#include "../common.h"
#include "../log.h"

#include "ThreadedChannelOutput.h"

ThreadedChannelOutput::ThreadedChannelOutput(unsigned int startChannel,
                                             unsigned int channelCount) :
    ChannelOutput(startChannel, channelCount),
    m_threadIsRunning(0),
    m_runThread(0),
    m_dataWaiting(0),
    m_useDoubleBuffer(0),
    m_threadID(0),
    m_maxWait(0),
    m_inBuf(NULL),
    m_outBuf(NULL) {
    pthread_mutex_init(&m_bufLock, NULL);
    pthread_mutex_init(&m_sendLock, NULL);
    pthread_cond_init(&m_sendCond, NULL);
}

ThreadedChannelOutput::~ThreadedChannelOutput() {
    pthread_cond_destroy(&m_sendCond);
    pthread_mutex_destroy(&m_bufLock);
    pthread_mutex_destroy(&m_sendLock);
}

int ThreadedChannelOutput::Init(void) {
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput::Init()\n");

    if (m_useDoubleBuffer) {
        m_inBuf = new unsigned char[m_channelCount];
        m_outBuf = new unsigned char[m_channelCount];
    }
    StartOutputThread();
    DumpConfig();

    return 1;
}

int ThreadedChannelOutput::Init(Json::Value config) {
    return ChannelOutput::Init(config) && Init();
}

int ThreadedChannelOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput::Close()\n");

    StopOutputThread();

    if (m_useDoubleBuffer) {
        delete[] m_inBuf;
        delete[] m_outBuf;
    }

    return ChannelOutput::Close();
}

int ThreadedChannelOutput::SendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "ThreadedChannelOutput::SendData(%p)\n", channelData);

    if (m_useDoubleBuffer) {
        pthread_mutex_lock(&m_bufLock);
        memcpy(m_inBuf, channelData, m_channelCount);
        m_dataWaiting = 1;
        pthread_mutex_unlock(&m_bufLock);
    } else {
        m_outBuf = channelData;
        m_dataWaiting = 1;
    }

    pthread_cond_signal(&m_sendCond);
    return 0;
}

int ThreadedChannelOutput::SendOutputBuffer(void) {
    LogExcess(VB_CHANNELOUT, "ChannelOutput::SendOutputBuffer()\n");

    if (m_useDoubleBuffer) {
        pthread_mutex_lock(&m_bufLock);
        memcpy(m_outBuf, m_inBuf, m_channelCount);
        m_dataWaiting = 0;
        pthread_mutex_unlock(&m_bufLock);
    } else {
        m_dataWaiting = 0;
    }

    RawSendData(m_outBuf);
    return m_channelCount;
}

void ThreadedChannelOutput::DumpConfig(void) {
    ChannelOutput::DumpConfig();
    LogDebug(VB_CHANNELOUT, "    Thread Running   : %u\n", m_threadIsRunning);
    LogDebug(VB_CHANNELOUT, "    Run Thread       : %u\n", m_runThread);
    LogDebug(VB_CHANNELOUT, "    Data Waiting     : %u\n", m_dataWaiting);
}

/*
 * pthread wrapper not a member of the class
 */
static void* RunChannelOutputThread(void* data) {
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput::RunChannelOutputThread()\n");

    ThreadedChannelOutput* output = reinterpret_cast<ThreadedChannelOutput*>(data);

    output->OutputThread();
    return nullptr;
}

int ThreadedChannelOutput::StartOutputThread(void) {
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput::StartOutputThread()\n");

    m_runThread = 1;

    int result = pthread_create(&m_threadID, NULL, &RunChannelOutputThread, this);

    if (result) {
        char msg[256];

        m_runThread = 0;
        switch (result) {
        case EAGAIN:
            strcpy(msg, "Insufficient Resources");
            break;
        case EINVAL:
            strcpy(msg, "Invalid settings");
            break;
        case EPERM:
            strcpy(msg, "Invalid Permissions");
            break;
        }
        LogErr(VB_CHANNELOUT, "ERROR creating ChannelOutput thread: %s\n", msg);
    }

    while (!m_threadIsRunning)
        usleep(10000);

    return 0;
}

int ThreadedChannelOutput::StopOutputThread(void) {
    LogDebug(VB_CHANNELOUT, "ChannelOutput::StopOutputThread()\n");

    if (!m_threadID)
        return -1;

    m_runThread = 0;

    pthread_cond_signal(&m_sendCond);

    int loops = 0;
    // Wait up to 110ms for data to be sent
    while ((m_dataWaiting) &&
           (m_threadIsRunning) &&
           (loops++ < 11))
        usleep(10000);

    pthread_mutex_lock(&m_bufLock);

    if (!m_threadID) {
        pthread_mutex_unlock(&m_bufLock);
        return -1;
    }

    pthread_join(m_threadID, NULL);
    m_threadID = 0;
    pthread_mutex_unlock(&m_bufLock);

    return 0;
}

void ThreadedChannelOutput::OutputThread(void) {
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput::OutputThread()\n");

    long long wakeTime = GetTime();
    struct timeval tv;
    struct timespec ts;

    m_threadIsRunning = 1;
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput thread started\n");

    while (m_runThread) {
        // Wait for more data
        pthread_mutex_lock(&m_sendLock);
        long long nowTime = GetTime();
        LogExcess(VB_CHANNELOUT, "ThreadedChannelOutput thread: sent: %lld, elapsed: %lld\n",
                  nowTime, nowTime - wakeTime);

        if (m_useDoubleBuffer)
            pthread_mutex_lock(&m_bufLock);

        if (m_dataWaiting || m_maxWait) {
            if (m_useDoubleBuffer)
                pthread_mutex_unlock(&m_bufLock);

            gettimeofday(&tv, NULL);
            ts.tv_sec = tv.tv_sec;
            if (m_maxWait) {
                ts.tv_nsec = (tv.tv_usec * 1000) + (m_maxWait * 1000000);
            } else {
                ts.tv_nsec = (tv.tv_usec + 200000) * 1000;
            }

            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }

            pthread_cond_timedwait(&m_sendCond, &m_sendLock, &ts);
        } else {
            if (m_useDoubleBuffer)
                pthread_mutex_unlock(&m_bufLock);

            pthread_cond_wait(&m_sendCond, &m_sendLock);
        }

        pthread_mutex_unlock(&m_sendLock);

        if (!m_runThread)
            continue;

        wakeTime = GetTime();
        LogExcess(VB_CHANNELOUT, "ThreadedChannelOutput thread: woke: %lld\n", wakeTime);

        // See if there is any data waiting to process or if we timed out
        if (m_useDoubleBuffer)
            pthread_mutex_lock(&m_bufLock);

        if (m_dataWaiting) {
            if (m_useDoubleBuffer)
                pthread_mutex_unlock(&m_bufLock);

            SendOutputBuffer();
        } else {
            if (m_useDoubleBuffer)
                pthread_mutex_unlock(&m_bufLock);
            WaitTimedOut();
        }
    }

    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput thread complete\n");
    m_threadIsRunning = 0;
}
