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

    void shutdown();
private:
    int epollf = -1;
    std::map<int, std::function<bool(int)>> callbacks;
};
