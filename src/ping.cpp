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

#include <arpa/inet.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <iostream>
#include <poll.h>
#include <unistd.h>

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
    volatile int ret = -2;
    PingManager::INSTANCE.ping(target, timeoutMs, [&ret](int i) { ret = i; });
    while (ret == -2 && timeoutMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        --timeoutMs;
    }
    return ret;
}

class PingTarget {
public:
    std::string hostname;
    u_char outpack[MAXPACKET];
    struct sockaddr_in to;
    struct icmp* icp;
    bool valid = true;
    std::function<void(int)> callback;
    long long startTime;
    long long timeoutMS;
    int period = 0;

    PingTarget(const std::string& target, int timeout, std::function<void(int)> cb) :
        callback(cb) {
        startTime = GetTimeMS();
        timeoutMS = timeout;
        hostname = target;
        memset(outpack, 0, sizeof(outpack));
        memset(&to, 0, sizeof(to));

        to.sin_family = AF_INET;

        // try to convert as dotted decimal address, else if that fails assume it's a hostname
        to.sin_addr.s_addr = inet_addr(target.c_str());
        if (to.sin_addr.s_addr != (u_int)-1)
            hostname = target;
        else {
            struct hostent* hp;
            hp = gethostbyname(target.c_str());
            if (!hp) {
                valid = false;
                cb(-1);
                return;
            }
            to.sin_family = hp->h_addrtype;
            bcopy(hp->h_addr, (caddr_t)&to.sin_addr, hp->h_length);
        }

        icp = (struct icmp*)outpack;
        icp->icmp_type = ICMP_ECHO;
        icp->icmp_code = 0;
        icp->icmp_cksum = 0;

        int ipad = to.sin_addr.s_addr;
        ipad >>= 16;
        ipad &= 0xFFFF;
        icp->icmp_seq = ipad; /* seq and id must be reflected */
        icp->icmp_id = std::rand() & 0xFF;

        int cc = DEFDATALEN + ICMP_MINLEN;
        in_cksum(icp, cc);

        icp = (struct icmp*)outpack;
        icp->icmp_type = ICMP_ECHO;
        icp->icmp_code = 0;
        icp->icmp_cksum = 0;
    }
    void revalidate() {
        if (!valid) {
            struct hostent* hp;
            hp = gethostbyname(hostname.c_str());
            if (!hp) {
                callback(-1);
            } else {
                valid = true;
            }
        }
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
        for (auto& t : targets) {
            delete t;
        }
        targets.clear();
    }

    void addPeriodicPing(const std::string& target, int timeout, int period, std::function<void(int)> callback) {
        std::unique_lock<std::mutex> lock(targetLock);
        for (auto t : periodics) {
            if (t->hostname == target && t->period) {
                t->timeoutMS = timeout;
                t->period = period;
                targets.push_back(t);
                lock.unlock();
                sendPing(t);
                return;
            }
        }
        // didn't find it
        PingTarget* t = new PingTarget(target, timeout, callback);
        if (t->valid) {
            t->callback = callback;
            t->period = period;
            targets.push_back(t);
            periodics.push_back(t);
            lock.unlock();
            sendPing(t);
        } else {
            delete t;
        }
    }
    void removePeriodicPing(const std::string& target) {
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

            int retval = poll(rfds, 1, 250);
            long long curTime = GetTimeMS();
            if (retval == -1) {
                printf("select() -1\n");
            } else if (retval) {
                int fromlen = sizeof(sockaddr_in);
                int ret = recvfrom(pingSocket, (char*)packet, sizeof(packet), 0, (struct sockaddr*)&from, (socklen_t*)&fromlen);
                if (ret < 0) {
                    continue;
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
                            if (t->icp->icmp_seq == icp->icmp_seq && ipad == ipadf && icp->icmp_id == t->icp->icmp_id) {
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
            } else {
                std::unique_lock<std::mutex> lock(targetLock);
                for (auto t : targets) {
                    long long tt = t->startTime;
                    tt += t->timeoutMS;
                    if (curTime > tt) {
                        t->callback(0);
                        toRemove.push_back(t);
                    }
                }
                lock.unlock();
            }
            std::unique_lock<std::mutex> lock(targetLock);
            for (auto t : toRemove) {
                targets.remove(t);
                if (t->period == 0) {
                    delete t;
                }
            }
            for (auto t : periodics) {
                long long st = t->startTime;
                st += t->period;
                if (curTime > st) {
                    if (!t->valid) {
                        t->revalidate();
                    }
                    if (t->valid) {
                        t->startTime = GetTimeMS();
                        targets.push_back(t);
                        sendPing(t);
                    }
                }
            }
            lock.unlock();
        }
        isRunning = -1;
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
            std::unique_lock<std::mutex> lock(targetLock);
            targets.push_back(t);
            lock.unlock();
            sendPing(t);
        } else {
            delete t;
        }
    }

    int pingSocket = -1;
    volatile int isRunning = 0;
    std::thread* pingThread;
    std::list<PingTarget*> targets;
    std::list<PingTarget*> periodics;
    std::mutex targetLock;
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
