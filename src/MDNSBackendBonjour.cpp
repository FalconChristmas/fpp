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

// The Bonjour (dns_sd) mDNS backend, which is what macOS has: the headers are
// in the SDK and the implementation is in libSystem, so it needs no package
// and no extra link flag.  The shared half -- the service name, what counts as
// a newly discovered host, and what is done with one -- stays in
// MDNSManager.cpp.  MDNSBackend.h says how one backend gets chosen.
//
// Bonjour needs far less scaffolding than Avahi.  Every operation is a
// DNSServiceRef with one socket behind it: register it with EPollManager and
// call DNSServiceProcessResult() when it is readable.  There is no poll
// adapter to write, and no reconnect logic either -- mDNSResponder is part of
// the OS and does not go away the way avahi-daemon can.

#include "fpp-pch.h"

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include "MDNSBackend.h"
#include "MDNSManager.h"

#include "EPollManager.h"
#include "common.h"
#include "log.h"

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <cstring>
#include <ifaddrs.h>
#include <string>
#include <unistd.h>
#include <vector>

#if HAVE_BONJOUR

#include <dns_sd.h>

static constexpr const char* FPPD_SERVICE = "_fppd._udp";
static constexpr const char* WLED_SERVICE = "_wled._tcp";
static constexpr uint16_t FPPD_PORT = 32320;
static constexpr uint16_t WLED_PORT = 80;

// Everything this backend owns beyond the handles MDNSManager already has a
// slot for.  Hung off m_backendState.
class BonjourState {
public:
    DNSServiceRef wledRegisterRef = nullptr;
    // Resolves and address lookups are short-lived, but each is a live socket
    // until its answer arrives, so they are tracked to be torn down if fppd
    // stops first.
    std::vector<DNSServiceRef> pending;
};

// One browse/resolve/lookup step.  Heap-allocated because the answer comes back
// long after the call that started it returned.
class BonjourOp {
public:
    MDNSManager* mgr = nullptr;
    BonjourState* st = nullptr;
    bool isWled = false;
    DNSServiceRef ref = nullptr;
};

// True when `ip` belongs to this machine.  Bonjour resolves our own
// advertisement to our real address rather than to loopback the way Avahi
// does, so without this we would discover ourselves on every browse and hand
// MultiSync a remote that is really us.  The loopback check in
// HandleResolveIP() does not catch it.
static bool isOwnAddress(const std::string& ip) {
    bool mine = false;
    struct ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) != 0) {
        return false;
    }
    for (struct ifaddrs* ifa = ifap; ifa && !mine; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        char buf[INET_ADDRSTRLEN] = { 0 };
        auto* sin = (struct sockaddr_in*)ifa->ifa_addr;
        if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf)) && ip == buf) {
            mine = true;
        }
    }
    freeifaddrs(ifap);
    return mine;
}

// Drop a finished operation: unhook its socket, free the ref, forget it.
// Safe to call from inside that socket's own callback -- EPollManager copies a
// callback out of its map before invoking it precisely so a callback may
// remove its own descriptor.
static void finishOp(MDNSManager* mgr, BonjourState* st, DNSServiceRef ref) {
    if (!ref) {
        return;
    }
    int fd = DNSServiceRefSockFD(ref);
    if (fd >= 0) {
        EPollManager::INSTANCE.removeFileDescriptor(fd);
    }
    if (st) {
        for (auto it = st->pending.begin(); it != st->pending.end(); ++it) {
            if (*it == ref) {
                st->pending.erase(it);
                break;
            }
        }
    }
    DNSServiceRefDeallocate(ref); // also closes fd
}

// Put a ref's socket in the main loop.  Returns false if the ref has no usable
// socket, in which case the caller frees it.
static bool watchRef(MDNSManager* mgr, BonjourState* st, DNSServiceRef ref, const char* what) {
    int fd = DNSServiceRefSockFD(ref);
    if (fd < 0) {
        LogWarn(VB_SYNC, "MDNS: no socket for %s\n", what);
        return false;
    }
    std::function<bool(int)> f = [mgr, st, ref, what](int) {
        DNSServiceErrorType err = DNSServiceProcessResult(ref);
        if (err != kDNSServiceErr_NoError) {
            // The ref is dead once this fails; nothing further will arrive on it.
            LogWarn(VB_SYNC, "MDNS: %s failed (%d), dropping it\n", what, (int)err);
            finishOp(mgr, st, ref);
        }
        // Never true: a true return is fppd's signal that bridge data is ready
        // to push out, which this is not.
        return false;
    };
    EPollManager::INSTANCE.addFileDescriptor(fd, f);
    return true;
}

// As watchRef(), and also records the ref as in-flight so that a Cleanup()
// landing mid-resolve takes its socket back out of the main loop.
static bool trackAndWatch(MDNSManager* mgr, BonjourState* st, DNSServiceRef ref, const char* what) {
    if (st) {
        st->pending.push_back(ref);
    }
    if (!watchRef(mgr, st, ref, what)) {
        if (st && !st->pending.empty()) {
            st->pending.pop_back();
        }
        return false;
    }
    return true;
}

// ── Discovery: browse -> resolve -> address ─────────────────────────────

static void DNSSD_API addrinfo_callback(DNSServiceRef ref, DNSServiceFlags flags,
                                        uint32_t interfaceIndex, DNSServiceErrorType err,
                                        const char* hostname, const struct sockaddr* address,
                                        uint32_t ttl, void* context) {
    auto* op = static_cast<BonjourOp*>(context);
    if (!op) {
        return;
    }
    if (err == kDNSServiceErr_NoError && address && address->sa_family == AF_INET &&
        (flags & kDNSServiceFlagsAdd)) {
        char buf[INET_ADDRSTRLEN] = { 0 };
        auto* sin = (const struct sockaddr_in*)address;
        if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) {
            std::string ip = buf;
            if (isOwnAddress(ip)) {
                LogDebug(VB_SYNC, "MDNS ignoring our own advertisement at %s\n", ip.c_str());
            } else {
                op->mgr->HandleResolveIP(ip, op->isWled);
            }
        }
    }
    // One address is all that is wanted; anything more would be the same host.
    if (!(flags & kDNSServiceFlagsMoreComing)) {
        MDNSManager* mgr = op->mgr;
        BonjourState* st = op->st;
        DNSServiceRef r = op->ref;
        delete op;
        finishOp(mgr, st, r);
    }
}

static void DNSSD_API resolve_callback(DNSServiceRef ref, DNSServiceFlags flags,
                                       uint32_t interfaceIndex, DNSServiceErrorType err,
                                       const char* fullname, const char* hosttarget,
                                       uint16_t port, uint16_t txtLen,
                                       const unsigned char* txtRecord, void* context) {
    auto* op = static_cast<BonjourOp*>(context);
    if (!op) {
        return;
    }
    if (err == kDNSServiceErr_NoError && hosttarget) {
        // The resolve gives a host name; turning that into an address is a
        // second lookup.  IPv4 only: MultiSync speaks v4, and the WLED apps
        // that read our advertisement render v4 too.
        auto* next = new BonjourOp{ op->mgr, op->st, op->isWled, nullptr };
        DNSServiceRef aref = nullptr;
        DNSServiceErrorType aerr = DNSServiceGetAddrInfo(&aref, 0, interfaceIndex,
                                                         kDNSServiceProtocol_IPv4, hosttarget,
                                                         addrinfo_callback, next);
        if (aerr == kDNSServiceErr_NoError && aref) {
            next->ref = aref;
            if (!trackAndWatch(op->mgr, op->st, aref, "address lookup")) {
                DNSServiceRefDeallocate(aref);
                delete next;
            }
        } else {
            LogDebug(VB_SYNC, "MDNS address lookup for %s failed (%d)\n", hosttarget, (int)aerr);
            delete next;
        }
    }
    if (!(flags & kDNSServiceFlagsMoreComing)) {
        MDNSManager* mgr = op->mgr;
        BonjourState* st = op->st;
        DNSServiceRef r = op->ref;
        delete op;
        finishOp(mgr, st, r);
    }
}

static void DNSSD_API browse_callback(DNSServiceRef ref, DNSServiceFlags flags,
                                      uint32_t interfaceIndex, DNSServiceErrorType err,
                                      const char* serviceName, const char* regtype,
                                      const char* replyDomain, void* context) {
    auto* owner = static_cast<BonjourOp*>(context);
    if (!owner || err != kDNSServiceErr_NoError) {
        return;
    }
    // Removals are ignored, exactly as the Avahi browser does: a host that
    // stops advertising is aged out by MultiSync's own timeout, not here.
    if (!(flags & kDNSServiceFlagsAdd)) {
        return;
    }

    auto* op = new BonjourOp{ owner->mgr, owner->st, owner->isWled, nullptr };
    DNSServiceRef rref = nullptr;
    DNSServiceErrorType rerr = DNSServiceResolve(&rref, 0, interfaceIndex, serviceName,
                                                 regtype, replyDomain, resolve_callback, op);
    if (rerr == kDNSServiceErr_NoError && rref) {
        op->ref = rref;
        if (!trackAndWatch(owner->mgr, owner->st, rref, "resolve")) {
            DNSServiceRefDeallocate(rref);
            delete op;
        }
    } else {
        LogDebug(VB_SYNC, "MDNS resolve of %s failed (%d)\n", serviceName, (int)rerr);
        delete op;
    }
}

// ── Advertising the local services ──────────────────────────────────────

static void DNSSD_API register_callback(DNSServiceRef ref, DNSServiceFlags flags,
                                        DNSServiceErrorType err, const char* name,
                                        const char* regtype, const char* domain,
                                        void* context) {
    auto* mgr = static_cast<MDNSManager*>(context);
    if (err != kDNSServiceErr_NoError) {
        LogWarn(VB_SYNC, "MDNS registration of %s failed (%d)\n", regtype ? regtype : "?", (int)err);
        return;
    }
    // Bonjour renames on a collision by itself and reports the name it settled
    // on, so there is no equivalent of Avahi's collision dance to run here.
    if (name && mgr && mgr->ServiceName() != name) {
        LogInfo(VB_SYNC, "MDNS service name in use; registered as %s instead\n", name);
        mgr->SetServiceName(name);
    }
}

// ── Backend entry points ────────────────────────────────────────────────

void MDNSManager::StartBackend() {
    auto* st = new BonjourState();
    m_backendState = st;
    m_running = true;

    // _fppd._udp, the advertisement other FPP instances browse for.
    DNSServiceRef reg = nullptr;
    DNSServiceErrorType err = DNSServiceRegister(&reg, 0, 0, m_serviceName.c_str(), FPPD_SERVICE,
                                                 nullptr, nullptr, htons(FPPD_PORT), 0, nullptr,
                                                 register_callback, this);
    if (err == kDNSServiceErr_NoError && reg && watchRef(this, st, reg, "_fppd._udp registration")) {
        m_entryGroup = reg;
        LogInfo(VB_SYNC, "MDNS service registration initiated for %s.%s on port %d\n",
                m_serviceName.c_str(), FPPD_SERVICE, (int)FPPD_PORT);
    } else {
        LogWarn(VB_SYNC, "MDNS could not register %s (%d)\n", FPPD_SERVICE, (int)err);
        if (reg) {
            DNSServiceRefDeallocate(reg);
        }
    }

    // _wled._tcp on the HTTP port, so WLED-compatible apps (notably WLED-iOS /
    // WLED-native) discover this node and treat it as a WLED device.  The mac=
    // TXT record is what those apps key off for device identity.
    {
        std::string mac = MDNSPrimaryMacAddress();
        TXTRecordRef txt;
        TXTRecordCreate(&txt, 0, nullptr);
        TXTRecordSetValue(&txt, "mac", (uint8_t)mac.size(), mac.c_str());
        DNSServiceRef wreg = nullptr;
        DNSServiceErrorType werr = DNSServiceRegister(&wreg, 0, 0, m_serviceName.c_str(),
                                                      WLED_SERVICE, nullptr, nullptr,
                                                      htons(WLED_PORT),
                                                      TXTRecordGetLength(&txt),
                                                      TXTRecordGetBytesPtr(&txt),
                                                      register_callback, this);
        TXTRecordDeallocate(&txt);
        if (werr == kDNSServiceErr_NoError && wreg &&
            watchRef(this, st, wreg, "_wled._tcp registration")) {
            st->wledRegisterRef = wreg;
            LogDebug(VB_SYNC, "MDNS %s on port %d (mac=%s)\n", WLED_SERVICE, (int)WLED_PORT, mac.c_str());
        } else {
            LogWarn(VB_SYNC, "MDNS could not register %s (%d)\n", WLED_SERVICE, (int)werr);
            if (wreg) {
                DNSServiceRefDeallocate(wreg);
            }
        }
    }

    // Browsers for both service types.  The context outlives the call and is
    // freed in StopBackend().
    struct {
        const char* type;
        bool isWled;
        void** slot;
    } browsers[] = {
        { FPPD_SERVICE, false, &m_serviceBrowser },
        { WLED_SERVICE, true, &m_wledServiceBrowser },
    };
    for (auto& b : browsers) {
        auto* op = new BonjourOp{ this, st, b.isWled, nullptr };
        DNSServiceRef bref = nullptr;
        DNSServiceErrorType berr = DNSServiceBrowse(&bref, 0, 0, b.type, nullptr,
                                                    browse_callback, op);
        if (berr == kDNSServiceErr_NoError && bref && watchRef(this, st, bref, b.type)) {
            op->ref = bref;
            *b.slot = op; // the op owns the ref; freed in StopBackend()
            WarningHolder::RemoveWarning(43, std::string("Network discovery: could not browse for ") + b.type);
        } else {
            LogWarn(VB_SYNC, "MDNS could not browse for %s (%d)\n", b.type, (int)berr);
            WarningHolder::AddWarning(43, std::string("Network discovery: could not browse for ") + b.type);
            if (bref) {
                DNSServiceRefDeallocate(bref);
            }
            delete op;
        }
    }
}

void MDNSManager::StopBackend() {
    auto* st = static_cast<BonjourState*>(m_backendState);

    // Anything still resolving first: those refs hold sockets in the main loop.
    if (st) {
        auto pending = st->pending;
        for (auto ref : pending) {
            finishOp(this, st, ref);
        }
        st->pending.clear();
    }

    for (void** slot : { &m_serviceBrowser, &m_wledServiceBrowser }) {
        if (*slot) {
            auto* op = static_cast<BonjourOp*>(*slot);
            finishOp(this, st, op->ref);
            delete op;
            *slot = nullptr;
        }
    }
    if (m_entryGroup) {
        // Deallocating the registration is what sends the mDNS goodbye, so
        // peers drop us now rather than at record TTL.
        finishOp(this, st, static_cast<DNSServiceRef>(m_entryGroup));
        m_entryGroup = nullptr;
    }
    if (st) {
        if (st->wledRegisterRef) {
            finishOp(this, st, st->wledRegisterRef);
            st->wledRegisterRef = nullptr;
        }
        delete st;
        m_backendState = nullptr;
    }
    m_running = false;
}

// Avahi-only plumbing.  Bonjour has no client that can disappear and renames on
// collision by itself, so these have nothing to do here -- they exist because
// the declarations are shared.
bool MDNSManager::StartAvahiClient() { return true; }
void MDNSManager::ScheduleClientReconnect() {}
void MDNSManager::ReconnectAvahiClient() {}
void MDNSManager::NoteClientRunning() {}
void MDNSManager::NoteClientDown() {}
void MDNSManager::PickAlternativeServiceName() {}
void MDNSManager::RegisterService(void* client) {}
void MDNSManager::SetServiceBrowser(void* sb) { m_serviceBrowser = sb; }
void MDNSManager::SetWLEDServiceBrowser(void* sb) { m_wledServiceBrowser = sb; }

#endif // HAVE_BONJOUR
