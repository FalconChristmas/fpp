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
 
#ifdef PLATFORM_OSX
#include <sys/event.h>
#define USE_KQUEUE
#else
#include <sys/epoll.h>
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include <sys/timerfd.h>
#include <syscall.h>
#endif

#include <functional>
#include <map>


class EPollManager {
public:
    static EPollManager INSTANCE;

    EPollManager();
    ~EPollManager();

    // Add a file descriptor to the epoll instance
    void addFileDescriptor(int fd, std::function<bool(int)> &callback);

    // Remove a file descriptor from the epoll instance
    void removeFileDescriptor(int fd);

    // Wait for events and call the appropriate callbacks
    enum class WaitResult {
        TIMEOUT,
        ALL_FALSE,
        SOME_TRUE,
        INTERRUPTED,
        FAILED
    };
    WaitResult waitForEvents(int mstimeout);

    // Register a callback to be invoked when the timer fires
    void setTimerCallback(std::function<void()>&& callback);

    // Arm (or rearm) the single wakeup timer to fire at deadlineMS
    // (absolute time in GetTimeMS() units). Passing 0 disarms.
    void armTimer(long long deadlineMS);

    // Disarm the wakeup timer
    void disarmTimer();

    void shutdown();
private:
    int epollf = -1;
    int timerFd = -1;
    std::function<void()> timerCallback;
    std::map<int, std::function<bool(int)>> callbacks;
#ifdef USE_KQUEUE
    static constexpr uintptr_t KQUEUE_TIMER_IDENT = 0xFFFFFFFE;
#endif
};
