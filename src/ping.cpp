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
#include <atomic>
#include <cstring>

#include <arpa/inet.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <iostream>
#include <poll.h>
#include <unistd.h>

#include <mutex>
#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <thread>

#include "common.h"
#include "ping.h"
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <ctype.h>
#include <netdb.h>
#include <stdint.h>

using namespace std;

#define DEFDATALEN (64 - ICMP_MINLEN) /* default data length */
#define MAXIPLEN 60
#define MAXICMPLEN 76
#define MAXPACKET (65536 - 60 - ICMP_MINLEN) /* max packet size */

inline void in_cksum(struct icmp* icp, unsigned len) {
    icp->icmp_cksum = 0;
    uint16_t* addr = (uint16_t*)icp;
    uint16_t answer = 0;

    /*
     * Our algorithm is simple, using a 32 bit accumulator (sum), we add
     * sequential 16 bit words to it, and at the end, fold back all the
     * carry bits from the top 16 bits into the lower 16 bits.
     */
    uint32_t sum = 0;
    while (len > 1) {
        sum += *addr++;
        len -= 2;
    }

    // mop up an odd byte, if necessary
    if (len == 1) {
        *(unsigned char*)&answer = *(unsigned char*)addr;
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add high 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits
    icp->icmp_cksum = answer;
}

int ping(std::string target, int timeoutMs) {
    // The result lives in state the callback shares, not in this frame: the
    // wait below gives up on its own count, and the ping thread still times
    // the ping out (and calls back) after this has returned.  A captured
    // reference to a local was a write into a dead stack frame.
    auto ret = std::make_shared<std::atomic_int>(-2);
    PingManager::INSTANCE.ping(target, timeoutMs, [ret](int i) { *ret = i; });
    while (*ret == -2 && timeoutMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        --timeoutMs;
    }
    return *ret;
}

class PingTarget {
public:
    std::string hostname;
    u_char outpack[MAXPACKET];
    struct sockaddr_in to;
    struct icmp* icp;
    bool valid = true;
    std::function<void(int)> callback;
    // GetMonotonicTimeMS(): on the wall clock a step back stopped the periodic
    // pings (and the timeouts) for the size of the step, so a controller that
    // had dropped out was not added back to the output when it returned.
    long long startTime;
    long long timeoutMS;
    int period = 0;
    // in PingInfo::targets awaiting a reply or timeout; guarded by targetLock
    bool pending = false;

    // Resolves the name on the calling thread, which can take seconds for one
    // that doesn't exist: never construct one under targetLock.  A name that
    // doesn't resolve leaves the target !valid, and it is the caller's to
    // report that.
    PingTarget(const std::string& target, int timeout, std::function<void(int)> cb) :
        callback(cb) {
        startTime = GetMonotonicTimeMS();
        timeoutMS = timeout;
        hostname = target;
        memset(outpack, 0, sizeof(outpack));
        memset(&to, 0, sizeof(to));

        to.sin_family = AF_INET;

        // The header is filled in whether or not the name resolves, so a
        // target that resolves later only needs its address (setAddress()).
        icp = (struct icmp*)outpack;
        icp->icmp_type = ICMP_ECHO;
        icp->icmp_code = 0;
        icp->icmp_cksum = 0;
        icp->icmp_id = FPPrand() & 0xFF;

        struct in_addr addr;
        valid = resolve(target, addr);
        if (valid) {
            setAddress(addr);
        }
    }

    static bool resolve(const std::string& target, struct in_addr& addr) {
        // try to convert as dotted decimal address, else if that fails assume it's a hostname
        addr.s_addr = inet_addr(target.c_str());
        if (addr.s_addr != (u_int)-1) {
            return true;
        }
        // gethostbyname() is not thread-safe (shared static hostent); the
        // ping loop runs on its own thread, so use the reentrant
        // getaddrinfo() instead.
        struct addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        struct addrinfo* res = nullptr;
        if (getaddrinfo(target.c_str(), nullptr, &hints, &res) != 0 || res == nullptr) {
            return false;
        }
        addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
        freeaddrinfo(res);
        return true;
    }

    void setAddress(const struct in_addr& addr) {
        to.sin_addr = addr;
        int ipad = to.sin_addr.s_addr;
        ipad >>= 16;
        ipad &= 0xFFFF;
        icp->icmp_seq = ipad; /* seq and id must be reflected */
    }
};

class PingManager::PingInfo {
public:
    PingInfo() {
    }
    ~PingInfo() {}

    void Initialize() {
        int flags = 0;
#ifndef PLATFORM_OSX
        flags = SOCK_NONBLOCK;
#endif
        if (getuid()) {
            pingSocket = socket(AF_INET, SOCK_DGRAM | flags, IPPROTO_ICMP);
        } else {
            pingSocket = socket(AF_INET, SOCK_RAW | flags, IPPROTO_ICMP);
        }
        isRunning = 1;

        pingThread = new std::thread(&PingManager::PingInfo::runPingLoop, this);
    }
    void Cleanup() {
        isRunning = 0;
        PingTarget t("127.0.0.1", 100, [](int i) {});
        sendPing(&t); // this will trigger a response which will speed up shutdown
        if (pingThread->joinable()) {
            pingThread->join();
        }
        delete pingThread;
        pingThread = nullptr;
        if (pingSocket >= 0) {
            close(pingSocket);
        }
        isRunning = 0;
        // a periodic target is in targets only while a ping is pending, and
        // one whose name never resolved never is
        for (auto& t : targets) {
            if (t->period == 0) {
                delete t;
            }
        }
        targets.clear();
        for (auto& t : periodics) {
            delete t;
        }
        periodics.clear();
    }

    // caller holds targetLock; true if `target` already has a periodic ping
    bool refreshPeriodic(const std::string& target, int timeout, int period) {
        for (auto t : periodics) {
            if (t->hostname == target && t->period) {
                t->timeoutMS = timeout;
                t->period = period;
                // One ping in flight per target.  Callers re-request on every
                // send error (UDPOutput, ~13/s against a saturated controller,
                // once per universe), and each re-push used to add another copy
                // to targets, every one of which fired the callback on timeout
                // -- hundreds per second, each failure able to start an HTTP
                // HEAD probe, until fppd ran out of file descriptors.  The
                // pending ping answers this request too.  A name that hasn't
                // resolved yet is left to the ping thread, which retries it
                // once a period.
                if (!t->pending && t->valid) {
                    // a fresh deadline; the stale one timed the ping out at
                    // the next poll, before the host could possibly answer
                    t->startTime = GetMonotonicTimeMS();
                    queuePing(t);
                }
                return true;
            }
        }
        return false;
    }
    void addPeriodicPing(const std::string& target, int timeout, int period, std::function<void(int)> callback) {
        {
            std::unique_lock<std::mutex> lock(targetLock);
            if (refreshPeriodic(target, timeout, period)) {
                return;
            }
        }
        // Didn't find it.  The new target resolves its name with the lock
        // released: that can stall for seconds on a name that doesn't exist,
        // and under the lock it stalled the ping thread and every other
        // caller with it.  A name that doesn't resolve is kept rather than
        // dropped, so the callers' repeated requests find it above instead of
        // each paying for the lookup again; the ping thread retries it.
        PingTarget* t = new PingTarget(target, timeout, callback);
        t->period = period;
        std::unique_lock<std::mutex> lock(targetLock);
        if (refreshPeriodic(target, timeout, period)) {
            // another thread added it while this one was resolving
            delete t;
            return;
        }
        periodics.push_back(t);
        if (t->valid) {
            queuePing(t);
            return;
        }
        lock.unlock();
        callback(-1);
    }
    void removePeriodicPing(const std::string& target) {
        // Waits out the ping thread's current pass: it holds dispatchLock
        // while it calls callbacks and while it still holds the targets it
        // collected, so once this returns the removed target's callback is
        // neither running nor about to run.  The caller (an output being
        // deleted) frees what that callback captured right after this.
        std::lock_guard<std::recursive_mutex> dispatch(dispatchLock);
        std::unique_lock<std::mutex> lock(targetLock);
        for (auto t : periodics) {
            if (t->hostname == target && t->period) {
                targets.remove(t);
                periodics.remove(t);
                delete t;
                return;
            }
        }
    }
    void runPingLoop() {
        SetThreadName("FPP-PingThread");
        u_char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];
        struct sockaddr_in from;
        struct ip* ip;
        struct icmp* icp;

        memset(packet, 0, sizeof(packet));
        memset(&from, 0, sizeof(from));

        struct pollfd rfds[2];
        rfds[0].fd = pingSocket;
        rfds[0].events = POLLIN;

        while (isRunning == 1) {
            std::list<PingTarget*> toRemove;

            std::vector<std::pair<std::string, std::pair<bool, struct in_addr>>> resolved = resolveDuePeriodics();

            int retval = poll(rfds, 1, 250);
            // held for the rest of the pass, from the callbacks through the
            // last use of toCall/toRemove; see removePeriodicPing()
            std::lock_guard<std::recursive_mutex> dispatch(dispatchLock);
            long long curTime = GetMonotonicTimeMS();
            if (retval == -1) {
                printf("select() -1\n");
            } else if (retval) {
                // Read every reply that is waiting, not just one, before the
                // timeout sweep below.  With one per pass, any stall of this
                // thread longer than a ping's timeout -- a name lookup in
                // resolveDuePeriodics(), a busy single-core board -- left
                // answered pings queued here, and the sweep then timed them
                // out: controllers that were up were dropped from the output.
                // Bounded, so a flood of ICMP can't hold the sweep off.
                for (int n = 0; n < 256; n++) {
                    int fromlen = sizeof(sockaddr_in);
                    int ret = recvfrom(pingSocket, (char*)packet, sizeof(packet), MSG_DONTWAIT, (struct sockaddr*)&from, (socklen_t*)&fromlen);
                    if (ret < 0) {
                        break;
                    }
                    // Check the IP header
                    ip = (struct ip*)((char*)packet);
                    int hlen = sizeof(struct ip);
                    if (ret >= (hlen + ICMP_MINLEN)) {
                        // Now the ICMP part
                        icp = (struct icmp*)(packet + hlen);
                        if (icp->icmp_type == ICMP_ECHOREPLY) {
                            std::list<PingTarget*> toCall;
                            std::unique_lock<std::mutex> lock(targetLock);
                            int ipadf = from.sin_addr.s_addr;
                            for (auto t : targets) {
                                int ipad = t->to.sin_addr.s_addr;
                                if (t->icp->icmp_seq == icp->icmp_seq && ipad == ipadf && icp->icmp_id == t->icp->icmp_id &&
                                    std::find(toRemove.begin(), toRemove.end(), t) == toRemove.end()) {
                                    // (a duplicate reply read in the same pass
                                    // is not counted, or removed, twice)
                                    toCall.push_back(t);
                                    toRemove.push_back(t);
                                }
                            }
                            lock.unlock();
                            for (auto t : toCall) {
                                long long l = curTime - t->startTime;
                                if (l == 0) {
                                    l = 1;
                                }
                                t->callback((int)l);
                            }
                        } else {
                            // printf("Not echo\n");
                        }
                    }
                }
            }
            // Time out on every pass, not only when poll() saw nothing for
            // 250ms: this raw socket receives every ICMP packet sent to the
            // box, and a few a second (port-unreachables from a device getting
            // output on a port it doesn't listen on) used to keep this sweep
            // from ever running, so a dead controller was never reported down.
            {
                // Called after targetLock is released, as replies are: a
                // callback that pings again (ping(), addPeriodicPing()) takes
                // targetLock, which is not recursive, and deadlocked here.
                // dispatchLock, still held, keeps the targets alive.
                std::list<PingTarget*> timedOut;
                std::unique_lock<std::mutex> lock(targetLock);
                for (auto t : targets) {
                    long long tt = t->startTime;
                    tt += t->timeoutMS;
                    if (curTime > tt && std::find(toRemove.begin(), toRemove.end(), t) == toRemove.end()) {
                        timedOut.push_back(t);
                        toRemove.push_back(t);
                    }
                }
                lock.unlock();
                for (auto t : timedOut) {
                    t->callback(0);
                }
            }
            std::unique_lock<std::mutex> lock(targetLock);
            for (auto t : toRemove) {
                targets.remove(t);
                t->pending = false;
                if (t->period == 0) {
                    delete t;
                }
            }
            std::list<PingTarget*> unresolved;
            for (auto& r : resolved) {
                for (auto t : periodics) {
                    if (t->hostname == r.first && !t->valid) {
                        if (r.second.first) {
                            t->setAddress(r.second.second);
                            t->valid = true;
                            t->startTime = GetMonotonicTimeMS();
                            queuePing(t);
                        } else {
                            unresolved.push_back(t);
                        }
                    }
                }
            }
            for (auto t : periodics) {
                long long st = t->startTime;
                st += t->period;
                if (curTime > st && !t->pending && t->valid) {
                    t->startTime = GetMonotonicTimeMS();
                    queuePing(t);
                }
            }
            lock.unlock();
            // still under dispatchLock, like the other callbacks
            for (auto t : unresolved) {
                t->callback(-1);
            }
        }
        isRunning = -1;
    }

    // Looks up, with no lock held, the names of periodic targets that haven't
    // resolved, each at most once a period.  A lookup can stall for seconds
    // (no DNS server reachable); under targetLock that stalled every caller
    // adding a ping, and under dispatchLock it would stall removePeriodicPing().
    // The results are applied by name in the pass that follows, since a
    // target can be removed (and freed) in the meantime.
    std::vector<std::pair<std::string, std::pair<bool, struct in_addr>>> resolveDuePeriodics() {
        std::vector<std::string> names;
        {
            std::unique_lock<std::mutex> lock(targetLock);
            long long curTime = GetMonotonicTimeMS();
            for (auto t : periodics) {
                if (!t->valid && curTime > t->startTime + t->period) {
                    // the next retry is a period from now, whatever the outcome
                    t->startTime = curTime;
                    names.push_back(t->hostname);
                }
            }
        }
        std::vector<std::pair<std::string, std::pair<bool, struct in_addr>>> results;
        for (auto& n : names) {
            struct in_addr addr{};
            bool ok = PingTarget::resolve(n, addr);
            results.emplace_back(n, std::make_pair(ok, addr));
        }
        return results;
    }

    // caller holds targetLock
    void queuePing(PingTarget* t) {
        t->pending = true;
        targets.push_back(t);
        sendPing(t);
    }
    void sendPing(PingTarget* target) {
        int cc = DEFDATALEN + ICMP_MINLEN;
        target->icp->icmp_seq++;
        in_cksum(target->icp, cc);
        sendto(pingSocket, (char*)target->outpack, cc, 0, (struct sockaddr*)&target->to, (socklen_t)sizeof(struct sockaddr_in));
    }
    void ping(const std::string& target, int timeout, std::function<void(int)> callback) {
        PingTarget* t = new PingTarget(target, timeout, callback);
        if (t->valid) {
            t->callback = callback;
            // The send has to happen under the same lock that publishes the
            // target, not after it.  A one-shot target (period == 0) is owned
            // by the ping loop the moment it is in `targets`: the loop matches
            // the reply -- or simply times it out, which it can do before this
            // thread has sent anything at all, since startTime is set by the
            // constructor above -- fires the callback, removes it and deletes
            // it.  Releasing the lock first therefore raced the delete, and
            // sendPing() then read the freed target (confirmed under ASan:
            // 64-byte read in sendto of a region freed by the ping thread).
            // The periodic path below already sends while holding the lock.
            std::unique_lock<std::mutex> lock(targetLock);
            queuePing(t);
        } else {
            delete t;
            callback(-1);
        }
    }

    int pingSocket = -1;
    // The ping thread loops on it; Cleanup() clears it from another thread.
    std::atomic_int isRunning{ 0 };
    std::thread* pingThread;
    std::list<PingTarget*> targets;
    std::list<PingTarget*> periodics;
    std::mutex targetLock;
    // taken before targetLock; recursive so a callback could remove a ping
    // on the ping thread without deadlocking (none does today)
    std::recursive_mutex dispatchLock;
};

PingManager PingManager::INSTANCE;

PingManager::PingManager() {
    info = new PingInfo();
}

PingManager::~PingManager() {
    delete info;
}

void PingManager::Initialize() {
    info->Initialize();
}
void PingManager::Cleanup() {
    info->Cleanup();
}

void PingManager::addPeriodicPing(const std::string& target, int timeout, int period, std::function<void(int)>&& callback) {
    info->addPeriodicPing(target, timeout, period, [callback](int i) { callback(i); });
}

void PingManager::removePeriodicPing(const std::string& target) {
    info->removePeriodicPing(target);
}

void PingManager::ping(const std::string& target, int timeout, std::function<void(int)>&& callback) {
    info->ping(target, timeout, callback);
}
