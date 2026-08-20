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

#include "fpp-json.h"

#include <cstring>
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
    m_maxWait(0),
    m_inBuf(NULL),
    m_outBuf(NULL) {
    // std::thread/std::mutex/std::condition_variable are default
    // constructed above (member declaration order in the header), no
    // explicit init needed the way pthread_mutex_init()/pthread_cond_init()
    // were required.
}

ThreadedChannelOutput::~ThreadedChannelOutput() {
    // Unlike pthread_t, std::thread's destructor calls std::terminate()
    // if the thread is still joinable (i.e. StopOutputThread() was never
    // called, e.g. a subclass that skips Close()). The pthread version had
    // no such requirement. This is a safety net only; normal shutdown
    // already goes through Close() -> StopOutputThread() -> join().
    if (m_thread.joinable()) {
        StopOutputThread();
    }
}

int ThreadedChannelOutput::Init(void) {
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput::Init()\n");

    if (m_useDoubleBuffer) {
        m_inBuf = new unsigned char[m_channelCount];
        m_outBuf = new unsigned char[m_channelCount];
    }
    if (!StartOutputThread()) {
        // Nothing can consume the buffers without a worker and the caller
        // discards an output whose Init() failed, so release them here.
        // Close() may still run on the way out; nulling keeps its delete[]
        // a no-op rather than a double free.
        if (m_useDoubleBuffer) {
            delete[] m_inBuf;
            delete[] m_outBuf;
            m_inBuf = nullptr;
            m_outBuf = nullptr;
        }
        return 0;
    }
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
        std::lock_guard<std::mutex> lock(m_bufLock);
        memcpy(m_inBuf, channelData, m_channelCount);
    } else {
        m_outBuf = channelData;
    }

    // The flag has to be raised under m_sendLock: a store outside it can land
    // between the worker's predicate check and its wait(), and the notify that
    // follows is then delivered to nobody.  The two locks are taken in
    // sequence, never nested, so no ordering against the worker exists to
    // violate.  Publishing m_outBuf/m_inBuf before the lock is enough for the
    // worker to see them - it reads them only after acquiring m_sendLock and
    // observing the flag.
    {
        std::lock_guard<std::mutex> lock(m_sendLock);
        m_dataWaiting = 1;
    }

    m_sendCond.notify_one();
    return 0;
}

int ThreadedChannelOutput::SendOutputBuffer(void) {
    LogExcess(VB_CHANNELOUT, "ChannelOutput::SendOutputBuffer()\n");

    // Claim the pending frame BEFORE copying it out.  Clearing after the copy
    // opens a window where a SendData landing between the copy and the clear
    // has its flag wiped and its frame is never sent - fatal for a final
    // blanking frame, which has no successor.  Claim-first means a frame
    // arriving mid-send leaves the flag raised and the newer data goes out on
    // the worker's next pass (at worst the newest frame is sent twice).
    {
        std::lock_guard<std::mutex> lock(m_sendLock);
        m_dataWaiting = 0;
    }

    if (m_useDoubleBuffer) {
        std::lock_guard<std::mutex> lock(m_bufLock);
        memcpy(m_outBuf, m_inBuf, m_channelCount);
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

bool ThreadedChannelOutput::StartOutputThread(void) {
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput::StartOutputThread()\n");

    m_runThread = 1;

    // std::thread has no "not a member of the class" trampoline
    // requirement like pthread_create() did (no void*(*)(void*) function
    // pointer signature to satisfy), so OutputThread() can be invoked
    // directly via a lambda. The lambda replicates the trampoline's
    // original debug log line so log output is unchanged.
    //
    // Unlike pthread_create(), which reports failure via its int return
    // value, std::thread's constructor reports failure by throwing
    // std::system_error. Catch it here so the same error-code -> message
    // mapping and the same "leave m_threadID/m_thread unset" behavior on
    // failure are preserved.
    try {
        m_thread = std::thread([this]() {
            LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput::RunChannelOutputThread()\n");
            OutputThread();
        });
    } catch (const std::system_error& ex) {
        char msg[256];

        m_runThread = 0;
        switch (ex.code().value()) {
        case EAGAIN:
            strcpy(msg, "Insufficient Resources");
            break;
        case EINVAL:
            strcpy(msg, "Invalid settings");
            break;
        case EPERM:
            strcpy(msg, "Invalid Permissions");
            break;
        default:
            // Any other code (std::thread isn't limited to the three
            // pthread_create errno values) — report it rather than reading
            // msg uninitialized, which the old pthread version did.
            snprintf(msg, sizeof(msg), "%s (code %d)", ex.what(), ex.code().value());
            break;
        }
        LogErr(VB_CHANNELOUT, "ERROR creating ChannelOutput thread: %s\n", msg);

        // Only OutputThread() ever raises m_threadIsRunning, so falling into
        // the wait below would spin forever.
        return false;
    }

    // m_runThread can only be 0 here if construction failed, which returns
    // above; the check keeps the wait bounded if that ever stops holding.
    while (m_runThread && !m_threadIsRunning)
        usleep(10000);

    return true;
}

int ThreadedChannelOutput::StopOutputThread(void) {
    LogDebug(VB_CHANNELOUT, "ChannelOutput::StopOutputThread()\n");

    if (!m_thread.joinable())
        return -1;

    // Half of the worker's wait predicate, so it must change under m_sendLock;
    // a store outside the lock can be missed by a worker that is between its
    // predicate check and its wait(), leaving it parked until the next frame
    // that will never come.
    {
        std::lock_guard<std::mutex> lock(m_sendLock);
        m_runThread = 0;
    }

    m_sendCond.notify_one();

    // Wait up to 110ms for a pending frame to be sent. The worker flushes
    // m_dataWaiting before it honors the stop, so this is what lets the final
    // blanking frame of a sequence reach the hardware.
    int loops = 0;
    while (loops++ < 11) {
        {
            std::lock_guard<std::mutex> lock(m_sendLock);
            if (!m_dataWaiting || !m_threadIsRunning)
                break;
        }
        usleep(10000);
    }

    if (!m_thread.joinable())
        return -1;

    // m_bufLock must NOT be held across the join(): the worker takes it while
    // flushing that pending frame on its way out. It guards only the contents
    // of m_inBuf/m_outBuf, which join() does not touch, and Close() does not
    // free them until after this returns.
    m_thread.join();

    return 0;
}

void ThreadedChannelOutput::OutputThread(void) {
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput::OutputThread()\n");
    SetThreadName("FPP-" + m_outputType);

    long long wakeTime = GetTime();

    // Deferred: m_sendLock is held only while the predicate state is read or
    // written, never across SendOutputBuffer()/WaitTimedOut(), which can block
    // on hardware for as long as they like.
    std::unique_lock<std::mutex> sendLock(m_sendLock, std::defer_lock);

    sendLock.lock();
    m_threadIsRunning = 1;
    sendLock.unlock();
    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput thread started\n");

    // m_dataWaiting and m_runThread are both owned by m_sendLock, so this
    // closes the window SendData's store-then-notify used to leave open.
    auto ready = [this]() { return m_dataWaiting || !m_runThread; };

    while (true) {
        // Wait for more data
        sendLock.lock();
        long long nowTime = GetTime();
        LogExcess(VB_CHANNELOUT, "ThreadedChannelOutput thread: sent: %lld, elapsed: %lld\n",
                  nowTime, nowTime - wakeTime);

        if (m_dataWaiting || m_maxWait) {
            // Old code computed an absolute CLOCK_REALTIME deadline
            // (now + duration) via gettimeofday()/timespec math for
            // pthread_cond_timedwait(). That duration was:
            //   - m_maxWait milliseconds, when m_maxWait is set
            //   - 200ms otherwise ((tv_usec + 200000us) rolled to nsec)
            // wait_for() takes that same duration directly, so the
            // timespec/overflow-carry arithmetic is no longer needed.
            std::chrono::milliseconds waitDuration(m_maxWait ? m_maxWait : 200);

            // The predicate overload computes the deadline once, so spurious
            // wakeups do not extend it and m_maxWait keeps its meaning: the
            // interval at which a subclass wants WaitTimedOut() called.
            m_sendCond.wait_for(sendLock, waitDuration, ready);
        } else {
            m_sendCond.wait(sendLock, ready);
        }

        // Sampled together under the lock so the two decisions below cannot
        // see a half-updated state.
        bool haveData = m_dataWaiting;
        bool keepRunning = m_runThread;

        sendLock.unlock();

        wakeTime = GetTime();
        LogExcess(VB_CHANNELOUT, "ThreadedChannelOutput thread: woke: %lld\n", wakeTime);

        // A pending frame is flushed even once a stop has been requested: the
        // last frame of a sequence is the blanking frame and StopOutputThread
        // follows it immediately, so honoring the stop first leaves the lights
        // lit.  Reaching here with neither data nor a stop means the wait
        // timed out, which is the only thing WaitTimedOut() is for.
        if (haveData) {
            SendOutputBuffer();
        } else if (keepRunning) {
            WaitTimedOut();
        }

        if (!keepRunning)
            break;
    }

    LogDebug(VB_CHANNELOUT, "ThreadedChannelOutput thread complete\n");
    sendLock.lock();
    m_threadIsRunning = 0;
    sendLock.unlock();
}
