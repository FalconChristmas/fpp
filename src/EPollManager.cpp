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

#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>

 #include "log.h"

EPollManager EPollManager::INSTANCE;


EPollManager::EPollManager() {
#ifdef USE_KQUEUE
    epollf = kqueue();
#else
    epollf = epoll_create1(EPOLL_CLOEXEC);
#endif
}

EPollManager::~EPollManager() {
    shutdown();
}
void EPollManager::shutdown() {
    if (epollf < 0) {
        return;
    }
    close(epollf);
    epollf = -1;
    callbacks.clear();
}


void EPollManager::addFileDescriptor(int fd, std::function<bool(int)> &callback) {
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
        LogWarn(VB_GENERAL, "Failed to add descripto: %d  %s\n", fd, strerror(errno));
    } else {
        callbacks[fd] = std::move(callback);
    }
}

void EPollManager::removeFileDescriptor(int fd) {
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
        LogWarn(VB_GENERAL, "Failed to remove descripto: %d  %s\n", fd, strerror(errno));
    } else {
        callbacks.erase(fd);
    }
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
            retVal |= callbacks[events[x].ident](events[x].ident);
#else
            retVal |= callbacks[events[x].data.fd](events[x].data.fd);
#endif
        }
    } else {
        return WaitResult::TIMEOUT;
    }
    return retVal ? WaitResult::SOME_TRUE : WaitResult::ALL_FALSE;
}
