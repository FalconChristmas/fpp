/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

// The parts of mDNS that do not depend on which library is doing the talking:
// the service name, what counts as a newly discovered host, and what is done
// with one.  The library-specific halves are in MDNSBackendAvahi.cpp and
// MDNSBackendBonjour.cpp; MDNSBackend.h picks between them.

#include "fpp-pch.h"

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include "MDNSBackend.h"
#include "MDNSManager.h"

#include "EPollManager.h"
#include "MultiSync.h"
#include "common.h"
#include "log.h"
#include "ping.h"

#include <net/if.h>
#include <sys/time.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <unistd.h>
#include <vector>
#ifdef PLATFORM_OSX
#include <net/if_dl.h>
#include <ifaddrs.h>
#endif

MDNSManager MDNSManager::INSTANCE;

MDNSManager::MDNSManager() = default;

MDNSManager::~MDNSManager() {
    Cleanup();
}

// The MAC of the first real interface, in the form the _wled._tcp "mac=" TXT
// record wants: lowercase hex, no separators.  Empty when none could be read,
// in which case the record is published empty rather than omitted -- WLED apps
// key off the field being there.  Both backends advertise the same record, and
// finding the address is a per-platform job rather than a per-library one, so
// it lives here rather than in either backend.
std::string MDNSPrimaryMacAddress() {
#ifdef PLATFORM_OSX
    // There is no /sys on macOS, and the interface list is long: a Mac readily
    // has a dozen link-layer interfaces (awdl, llw, anpi, bridge, ap, the
    // Thunderbolt ens) that are "up" and carry a MAC while being no use to
    // anyone.  Taking the first of those gets a placeholder such as
    // 02:00:00:00:00:00.  The one worth advertising is the interface that
    // actually holds an address, since that is the one a WLED app will talk
    // to, so collect those names first and match the link layer to them.
    std::set<std::string> addressed;
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) {
        return "";
    }
    for (struct ifaddrs* ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET || !ifa->ifa_name) {
            continue;
        }
        if ((ifa->ifa_flags & IFF_LOOPBACK) || !(ifa->ifa_flags & IFF_UP)) {
            continue;
        }
        addressed.insert(ifa->ifa_name);
    }

    std::string mac;
    for (struct ifaddrs* ifa = ifap; ifa && mac.empty(); ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_LINK || !ifa->ifa_name) {
            continue;
        }
        if (addressed.find(ifa->ifa_name) == addressed.end()) {
            continue;
        }
        auto* sdl = (struct sockaddr_dl*)ifa->ifa_addr;
        if (sdl->sdl_alen != 6) {
            continue;
        }
        const unsigned char* a = (const unsigned char*)LLADDR(sdl);
        if (!(a[0] | a[1] | a[2] | a[3] | a[4] | a[5])) {
            continue;
        }
        // macOS hides hardware addresses from a process without the
        // entitlement for them and hands back 02:00:00:00:00:00 instead --
        // from getifaddrs() and from sysctl(NET_RT_IFLIST) alike, for every
        // interface (/sbin/ifconfig still prints the real one, which makes
        // this look like a bug in the code rather than the policy it is).
        // Reporting it would give every Mac running FPP the same identity in
        // the WLED apps that read this field, which is worse than reporting
        // none: an absent value is already what Linux publishes when it cannot
        // find an address.  If a real one is ever wanted here it needs IOKit
        // (IOEthernetInterface/IOMACAddress), not the socket APIs.
        static const unsigned char MASKED[6] = { 0x02, 0, 0, 0, 0, 0 };
        if (memcmp(a, MASKED, 6) == 0) {
            continue;
        }
        char buf[13];
        snprintf(buf, sizeof(buf), "%02x%02x%02x%02x%02x%02x",
                 a[0], a[1], a[2], a[3], a[4], a[5]);
        mac = buf;
    }
    freeifaddrs(ifap);
    if (mac.empty()) {
        LogDebug(VB_SYNC, "MDNS: no hardware address available on this platform; advertising none\n");
    }
    return mac;
#else
    // Pick the first non-loopback interface with a real MAC.  Modern Debian
    // uses predictable names like "enxXX..." and "wlxXX...", so enumerate
    // /sys/class/net rather than guessing at them.
    std::string mac;
    if (DIR* d = opendir("/sys/class/net")) {
        struct dirent* e;
        while ((e = readdir(d)) != nullptr) {
            if (e->d_name[0] == '.' || std::string(e->d_name) == "lo") {
                continue;
            }
            char path[256];
            snprintf(path, sizeof(path), "/sys/class/net/%s/address", e->d_name);
            FILE* f = fopen(path, "r");
            if (!f) {
                continue;
            }
            char buf[32] = {};
            bool got = false;
            if (fgets(buf, sizeof(buf), f)) {
                std::string s(buf);
                s.erase(s.find_last_not_of(" \t\r\n") + 1);
                if (!s.empty() && s != "00:00:00:00:00:00") {
                    mac = s;
                    got = true;
                }
            }
            fclose(f);
            if (got) {
                break;
            }
        }
        closedir(d);
    }
    std::string flat;
    for (char c : mac) {
        if (c != ':') {
            flat.push_back(std::tolower(c));
        }
    }
    return flat;
#endif
}

// ── MDNSManager implementation ──────────────────────────────────────────

void MDNSManager::Initialize(std::map<int, std::function<bool(int)>>& callbacks) {
    if (m_running)
        return;

    // Initialize service name from hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "fppd", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }
    m_serviceName = hostname;

    StartBackend();
}

void MDNSManager::Cleanup() {
    StopBackend();
}

#if !HAVE_AVAHI && !HAVE_BONJOUR
// Neither library is present.  Discovery still works through the MultiSync
// ping and the HTTP scan; only the mDNS half is missing.  These are the same
// entry points the real backends define, so the link is satisfied either way.
void MDNSManager::StartBackend() {
    LogWarn(VB_SYNC, "No mDNS library available (Avahi or Bonjour); MDNS disabled\n");
}
void MDNSManager::StopBackend() { m_running = false; }
void MDNSManager::SetServiceBrowser(void* sb) {}
void MDNSManager::SetWLEDServiceBrowser(void* sb) {}
void MDNSManager::RegisterService(void* client) {}
void MDNSManager::PickAlternativeServiceName() {}
void MDNSManager::NoteClientRunning() {}
void MDNSManager::NoteClientDown() {}
#endif

int MDNSManager::registerCallback(std::function<void(const std::string&)>&& callback) {
    std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
    int id = m_nextCallbackId++;
    m_callbacks[id] = std::move(callback);
    return id;
}

void MDNSManager::removeCallback(int id) {
    std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
    m_callbacks.erase(id);
}

void MDNSManager::HandleResolveIP(const std::string& ip, bool isWled) {
    bool isNew = false;
    {
        std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
        if (m_knownHosts.insert(ip).second) {
            isNew = true;
        }
    }

    if (!isNew)
        return;

    // Avahi resolves this host's own advertisement to 127.0.0.1, so we see
    // ourselves here on every browse.  MultiSync rejects loopback anyway; skip
    // it before spending a ping and a detached HTTP probe on it.
    if (IsLoopbackAddress(ip)) {
        LogDebug(VB_SYNC, "MDNS ignoring loopback resolve for %s\n", ip.c_str());
        return;
    }

    LogDebug(VB_SYNC, "MDNS discovered new host %s%s\n", ip.c_str(), isWled ? " (wled)" : "");

    // ping and notify MultiSync. The FPP discover-ping is a harmless no-op for a
    // pure WLED node (it doesn't speak the protocol), and FPP nodes advertise
    // _wled._tcp too, so we always send it regardless of which browser found the
    // host - that way an FPP node still gets discovered if the _wled._tcp
    // resolve happens to win the race against _fppd._udp.
    PingManager::INSTANCE.ping(ip, 1000, [ip](int result) {
        LogDebug(VB_SYNC, "MDNS ping %s result %d\n", ip.c_str(), result);
    });
    MultiSync::INSTANCE.PingSingleRemote(ip.c_str(), 1);

    // Always probe over HTTP as well. The FPP discover-ping does not carry the
    // remote's System UUID, so without this probe an FPP peer's UUID stays empty
    // in the systems list - HTTP controller detection populates it (needed for
    // device identification). This runs only once per newly discovered host.
    // Harmless for a pure WLED node; detection just identifies it as WLED.
    // PingSingleRemoteViaHTTP returns as soon as the request is queued and calls
    // UpdateSystem itself from the completion, so there is nothing to keep off
    // this thread and no detached thread needed to do it.
    MultiSync::INSTANCE.PingSingleRemoteViaHTTP(ip);

    // Take a snapshot of callbacks while holding the lock, then invoke them without the lock
    // to avoid deadlock/iterator invalidation if callbacks modify m_callbacks
    std::vector<std::function<void(const std::string&)>> callbacksSnapshot;
    {
        std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
        for (auto const& pair : m_callbacks) {
            callbacksSnapshot.push_back(pair.second);
        }
    }

    // Invoke callbacks without holding the lock
    for (auto const& callback : callbacksSnapshot) {
        callback(ip);
    }
}
