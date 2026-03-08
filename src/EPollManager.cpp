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

#include "EPollManager.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "common_mini.h"
#include "log.h"

EPollManager EPollManager::INSTANCE;

EPollManager::EPollManager() {
#ifdef USE_KQUEUE
    epollf = kqueue();
#else

    if (FileExists("/etc/fpp/container")) {
        // FIXME - On Docker, when running as a daemon, the epol_ctl() in
        // addFileDescriptor() fails when epollf is < 3.  This is a similar
        // issue to what was seen in Bridge_Initialize_Internal() in
        // e131bridge.cpp, so it may be the same issue causing both of
        // these problems and may not be related to Docker at all.
        int f0, f1, f2 = -1;

        f0 = open("/dev/null", O_RDONLY);
        f1 = open("/dev/null", O_RDONLY);
        f2 = open("/dev/null", O_RDONLY);

        epollf = epoll_create1(EPOLL_CLOEXEC);

        close(f0);
        close(f1);
        close(f2);
    } else {
        epollf = epoll_create1(EPOLL_CLOEXEC);
    }

    // Create a single timerfd for waking epoll on timer deadlines
    timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerFd >= 0) {
        epoll_event event;
        memset(&event, 0, sizeof(event));
        event.events = EPOLLIN;
        event.data.fd = timerFd;
        if (epoll_ctl(epollf, EPOLL_CTL_ADD, timerFd, &event) == -1) {
            LogWarn(VB_GENERAL, "Failed to add timerfd to epoll: %s\n", strerror(errno));
            close(timerFd);
            timerFd = -1;
        }
    } else {
        LogWarn(VB_GENERAL, "Failed to create timerfd: %s\n", strerror(errno));
    }
#endif
}

EPollManager::~EPollManager() {
    shutdown();
}
void EPollManager::shutdown() {
    if (epollf < 0) {
        return;
    }
#ifndef USE_KQUEUE
    if (timerFd >= 0) {
        close(timerFd);
        timerFd = -1;
    }
#endif
    close(epollf);
    epollf = -1;
    callbacks.clear();
    timerCallback = nullptr;
}

void EPollManager::addFileDescriptor(int fd, std::function<bool(int)>& callback) {
    if (epollf == -1) {
        // EPoll has been shutdown, cannot add file descriptor
        return;
    }
#ifdef USE_KQUEUE
    struct kevent change;
    EV_SET(&change, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    int rc = kevent(epollf, &change, 1, NULL, 0, NULL);
#else
    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = fd;
    int rc = epoll_ctl(epollf, EPOLL_CTL_ADD, fd, &event);
#endif
    if (rc == -1) {
        LogWarn(VB_GENERAL, "Failed to add descriptor: %d  %s\n", fd, strerror(errno));
    } else {
        callbacks[fd] = std::move(callback);
    }
}

void EPollManager::removeFileDescriptor(int fd) {
    if (epollf == -1) {
        // EPoll has been shutdown, cannot remove file descriptor
        return;
    }
    // Implementation for removing a file descriptor from the epoll instance
#ifdef USE_KQUEUE
    struct kevent change;
    EV_SET(&change, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    int rc = kevent(epollf, &change, 1, NULL, 0, NULL);
#else
    epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLIN;
    event.data.fd = fd;
    int rc = epoll_ctl(epollf, EPOLL_CTL_DEL, fd, &event);
#endif
    if (rc == -1) {
        LogWarn(VB_GENERAL, "Failed to remove descriptor: %d  %s\n", fd, strerror(errno));
    } else {
        callbacks.erase(fd);
    }
}

void EPollManager::setTimerCallback(std::function<void()>&& callback) {
    timerCallback = std::move(callback);
}

void EPollManager::armTimer(long long deadlineMS) {
    if (epollf == -1) {
        return;
    }

    long long nowMS = GetTimeMS();
    long long relativeMS = deadlineMS - nowMS;
    if (relativeMS < 1) {
        relativeMS = 1; // fire ASAP but not zero (which disarms on Linux)
    }

#ifdef USE_KQUEUE
    // kqueue EVFILT_TIMER default unit is milliseconds (no flag needed)
    struct kevent change;
    EV_SET(&change, KQUEUE_TIMER_IDENT, EVFILT_TIMER, EV_ADD | EV_ENABLE | EV_ONESHOT, 0, relativeMS, NULL);
    kevent(epollf, &change, 1, NULL, 0, NULL);
#else
    if (timerFd < 0) {
        return;
    }
    struct itimerspec its = {};
    its.it_value.tv_sec = relativeMS / 1000;
    its.it_value.tv_nsec = (relativeMS % 1000) * 1000000;
    timerfd_settime(timerFd, 0, &its, NULL);
#endif
}

void EPollManager::disarmTimer() {
    if (epollf == -1) {
        return;
    }
#ifdef USE_KQUEUE
    struct kevent change;
    EV_SET(&change, KQUEUE_TIMER_IDENT, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
    kevent(epollf, &change, 1, NULL, 0, NULL);
#else
    if (timerFd < 0) {
        return;
    }
    struct itimerspec its = {};
    timerfd_settime(timerFd, 0, &its, NULL);
#endif
}

EPollManager::WaitResult EPollManager::waitForEvents(int mstimeout) {
    // Implementation for waiting for events and calling the appropriate callbacks
    constexpr int MAX_EVENTS = 40;
#ifdef USE_KQUEUE
    struct kevent events[MAX_EVENTS];
    struct timespec timeoutStruct = { mstimeout / 1000, (mstimeout % 1000) * 1000000 };
    int epollresult = kevent(epollf, NULL, 0, events, MAX_EVENTS, &timeoutStruct);
#else
    epoll_event events[MAX_EVENTS];
    int epollresult = epoll_wait(epollf, events, MAX_EVENTS, mstimeout);
    if (epollresult < 0) {
        if (errno == EINTR) {
            // We get interrupted when media players finish
            return WaitResult::INTERRUPTED;
        } else {
            return WaitResult::FAILED;
        }
    }
#endif
    bool retVal = false;
    if (epollresult > 0) {
        for (int x = 0; x < epollresult; x++) {
#ifdef USE_KQUEUE
            if (events[x].ident == KQUEUE_TIMER_IDENT) {
                if (timerCallback) {
                    timerCallback();
                }
                continue;
            }
            retVal |= callbacks[events[x].ident](events[x].ident);
#else
            if (events[x].data.fd == timerFd) {
                uint64_t expirations;
                read(timerFd, &expirations, sizeof(expirations));
                if (timerCallback) {
                    timerCallback();
                }
                continue;
            }
            retVal |= callbacks[events[x].data.fd](events[x].data.fd);
#endif
        }
    } else {
        return WaitResult::TIMEOUT;
    }
    return retVal ? WaitResult::SOME_TRUE : WaitResult::ALL_FALSE;
}
