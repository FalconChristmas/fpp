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

// The Avahi mDNS backend.  Everything here is the Avahi half of MDNSManager;
// the shared half -- the service name, what counts as a newly discovered host,
// and what is done with one -- stays in MDNSManager.cpp.  MDNSBackend.h says
// how one backend gets chosen.

#include "fpp-pch.h"

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

#include "MDNSBackend.h"
#include "MDNSManager.h"

#include "common.h"
#include "EPollManager.h"
#include "log.h"

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <net/if.h>
#include <unistd.h>
#include <vector>

#if HAVE_AVAHI

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/strlst.h>
#include <avahi-common/alternative.h>
#include <sys/timerfd.h>

// ── Custom AvahiPoll adapter ────────────────────────────────────────────
// Avahi forward-declares AvahiWatch and AvahiTimeout as opaque types.
// The poll adapter provides the concrete definitions and wires them into
// the main FPP event loop via EPollManager.
// ─────────────────────────────────────────────────────────────────────────


struct AvahiWatch {
    int fd;
    AvahiWatchEvent requested;
    AvahiWatchEvent last;
    AvahiWatchCallback callback;
    void* userdata;
    std::function<bool(int)> epollCb;
};

struct AvahiTimeout {
    int timerFd;
    AvahiTimeoutCallback callback;
    void* userdata;
    std::function<bool(int)> epollCb;
    bool registered;
};

static uint32_t avahiToEpoll(AvahiWatchEvent e) {
    uint32_t ep = 0;
    if (e & AVAHI_WATCH_IN)  ep |= EPOLLIN;
    if (e & AVAHI_WATCH_OUT) ep |= EPOLLOUT;
    // EPOLLERR and EPOLLHUP are always reported by epoll regardless of mask
    return ep ? ep : EPOLLIN;
}

static void armTimerFd(int tfd, const struct timeval* tv) {
    struct itimerspec its = {};
    if (tv) {
        struct timeval now;
        gettimeofday(&now, NULL);
        int64_t delayUs = ((int64_t)tv->tv_sec - now.tv_sec) * 1000000
                        + (tv->tv_usec - now.tv_usec);
        if (delayUs < 1)
            delayUs = 1;
        its.it_value.tv_sec  = delayUs / 1000000;
        its.it_value.tv_nsec = (delayUs % 1000000) * 1000;
    }
    // tv == NULL → its is all-zero → disarms the timer
    timerfd_settime(tfd, 0, &its, NULL);
}

// ── AvahiPoll function table ──

static AvahiWatch* fpp_watch_new(const AvahiPoll* api, int fd,
                                 AvahiWatchEvent events,
                                 AvahiWatchCallback callback, void* userdata) {
    auto* w = new AvahiWatch{fd, events, (AvahiWatchEvent)0, callback, userdata, {}};

    w->epollCb = [w](int) -> bool {
        w->last = w->requested;
        w->callback(w, w->fd, w->last, w->userdata);
        return false;
    };

    EPollManager::INSTANCE.addFileDescriptor(fd, w->epollCb);
    uint32_t ep = avahiToEpoll(events);
    if (ep != EPOLLIN) {
        EPollManager::INSTANCE.updateFileDescriptorEvents(fd, ep);
    }
    return w;
}

static void fpp_watch_update(AvahiWatch* w, AvahiWatchEvent events) {
    w->requested = events;
    EPollManager::INSTANCE.updateFileDescriptorEvents(w->fd, avahiToEpoll(events));
}

static AvahiWatchEvent fpp_watch_get_events(AvahiWatch* w) {
    return w->last;
}

static void fpp_watch_free(AvahiWatch* w) {
    if (!w)
        return;
    EPollManager::INSTANCE.removeFileDescriptor(w->fd);
    delete w;
}

static AvahiTimeout* fpp_timeout_new(const AvahiPoll* api,
                                     const struct timeval* tv,
                                     AvahiTimeoutCallback callback,
                                     void* userdata) {
    auto* t = new AvahiTimeout{-1, callback, userdata, {}, false};
    t->timerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (t->timerFd < 0) {
        LogWarn(VB_SYNC, "Failed to create timerfd for Avahi timeout\n");
        delete t;
        return nullptr;
    }

    t->epollCb = [t](int) -> bool {
        uint64_t expirations;
        ssize_t n = read(t->timerFd, &expirations, sizeof(expirations));
        (void)n;
        t->callback(t, t->userdata);
        return false;
    };

    EPollManager::INSTANCE.addFileDescriptor(t->timerFd, t->epollCb);
    t->registered = true;
    armTimerFd(t->timerFd, tv);
    return t;
}

static void fpp_timeout_update(AvahiTimeout* t, const struct timeval* tv) {
    if (!t || t->timerFd < 0)
        return;
    armTimerFd(t->timerFd, tv);
}

static void fpp_timeout_free(AvahiTimeout* t) {
    if (!t)
        return;
    if (t->registered) {
        EPollManager::INSTANCE.removeFileDescriptor(t->timerFd);
    }
    if (t->timerFd >= 0) {
        close(t->timerFd);
    }
    delete t;
}


// ── Avahi service callbacks ─────────────────────────────────────────────

static void resolve_callback(AvahiServiceResolver* r,
                             AvahiIfIndex interface,
                             AvahiProtocol protocol,
                             AvahiResolverEvent event,
                             const char* name,
                             const char* type,
                             const char* domain,
                             const char* host_name,
                             const AvahiAddress* a,
                             uint16_t port,
                             AvahiStringList* txt,
                             AvahiLookupResultFlags flags,
                             void* userdata) {
    MDNSManager* mgr = static_cast<MDNSManager*>(userdata);
    if (!mgr)
        return;

    if (event == AVAHI_RESOLVER_FOUND) {
        char addr_buf[AVAHI_ADDRESS_STR_MAX];
        avahi_address_snprint(addr_buf, sizeof(addr_buf), a);
        std::string ip(addr_buf);
        // An IPv6 link-local address (fe80::/10) is only usable together with the
        // interface it was discovered on, so carry the zone id ("%eth0") along.
        // Without it curl can't pick an interface and the address can't be reached
        // on IPv6-only networks.  The zone is harmless to the IPv4-only ping paths
        // (they already can't parse a v6 address).
        if (a->proto == AVAHI_PROTO_INET6 && interface != AVAHI_IF_UNSPEC &&
            ip.rfind("fe80", 0) == 0 && ip.find('%') == std::string::npos) {
            char ifname[IF_NAMESIZE] = {0};
            if (if_indextoname(interface, ifname)) {
                ip += "%";
                ip += ifname;
            }
        }
        // WLED nodes (and FPP's own _wled._tcp advertisement) are probed over
        // HTTP rather than the FPP ping protocol.
        bool isWled = (type != nullptr && strstr(type, "_wled._tcp") != nullptr);
        // delegate to instance method to avoid accessing private members from C callback
        mgr->HandleResolveIP(ip, isWled);
    }

    avahi_service_resolver_free(r);
}

static void browse_callback(AvahiServiceBrowser* b,
                            AvahiIfIndex interface,
                            AvahiProtocol protocol,
                            AvahiBrowserEvent event,
                            const char* name,
                            const char* type,
                            const char* domain,
                            AvahiLookupResultFlags flags,
                            void* userdata) {
    AvahiClient* client = avahi_service_browser_get_client(b);
    MDNSManager* mgr = static_cast<MDNSManager*>(userdata);
    if (!mgr)
        return;

    switch (event) {
    case AVAHI_BROWSER_NEW:
        // resolve service to get address
        avahi_service_resolver_new(client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)0, resolve_callback, mgr);
        break;
    case AVAHI_BROWSER_REMOVE: {
        // callers will remove entries when services disappear via monitor
    } break;
    default:
        break;
    }
}

static void entry_group_callback(AvahiEntryGroup* g, AvahiEntryGroupState state, void* userdata) {
    MDNSManager* mgr = static_cast<MDNSManager*>(userdata);
    if (!mgr)
        return;

    switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
        LogInfo(VB_SYNC, "MDNS service registered successfully\n");
        break;
    case AVAHI_ENTRY_GROUP_COLLISION: {
        LogWarn(VB_SYNC, "MDNS service name collision, picking alternative name\n");

        AvahiClient* client = avahi_entry_group_get_client(g);
        if (client) {
            avahi_entry_group_reset(g);
            // Pick an alternative name to avoid re-colliding
            mgr->PickAlternativeServiceName();
            mgr->RegisterService(client);
        }

        break;
    }
    case AVAHI_ENTRY_GROUP_FAILURE:
        LogErr(VB_SYNC, "MDNS entry group failure\n");
        break;
    default:
        break;
    }
}

// Defined in fppd.cpp; cleared when FPP begins shutting down.  Declared the same way
// httpAPI.cpp does rather than adding a header for one flag.
extern volatile int runMainFPPDLoop;

// WarningHolder matches on the id AND the exact message text, so the add and the
// remove have to use the same string.  Naming them once is what keeps a warning from
// becoming permanent because the two copies drifted apart.
static constexpr int MDNS_WARNING_ID = 43;
static constexpr const char* MDNS_WARN_CLIENT = "Network discovery: mDNS/Avahi client failure (is avahi-daemon running?)";
static constexpr const char* MDNS_WARN_FPPD = "Network discovery: could not browse for FPP devices (_fppd._udp)";
static constexpr const char* MDNS_WARN_WLED = "Network discovery: could not browse for WLED devices (_wled._tcp)";

// Reconnect backoff, and how long the daemon has to stay gone before it counts as an
// abnormal condition.  A restart of avahi-daemon is back inside a second or two, and
// on the way to a reboot it goes down some seconds before fppd is told to stop -
// warning on either of those puts a red banner on screen for something that is not
// wrong.  Retries keep running for as long as fppd does, so a real outage is still
// picked up the moment it ends.
static constexpr int MDNS_RECONNECT_MIN_MS = 2000;
static constexpr int MDNS_RECONNECT_MAX_MS = 30000;
static constexpr long long MDNS_WARN_AFTER_MS = 30000;

// Deliberately not the wall clock: these boxes have no RTC, so the NTP step early in
// a boot would otherwise read as a half-hour outage.
static long long MDNSMonotonicMS() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// -1 is set during teardown, after the main loop is already gone, so it is not
// "running" for our purposes even though it is truthy.
static bool fppdIsRunning() {
    return runMainFPPDLoop > 0;
}

static void client_callback(AvahiClient* c, AvahiClientState state, void* userdata) {
    MDNSManager* mgr = static_cast<MDNSManager*>(userdata);
    if (!mgr)
        return;

    if (state == AVAHI_CLIENT_S_RUNNING) {
        // The client is up, so whatever we said about it being down is no longer
        // true.  Without this the warning stayed on screen for the rest of the
        // session.
        mgr->NoteClientRunning();

        // Register local service
        mgr->RegisterService(c);

        // Create a service browser for _fppd._udp
        void* sb = avahi_service_browser_new(c, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_fppd._udp", NULL, (AvahiLookupFlags)0, browse_callback, mgr);
        if (!sb) {
            LogErr(VB_SYNC, "Failed to create Avahi service browser for _fppd._udp: %s\n",
                   avahi_strerror(avahi_client_errno(c)));
            WarningHolder::AddWarning(MDNS_WARNING_ID, MDNS_WARN_FPPD);
        } else {
            mgr->SetServiceBrowser(sb);
            WarningHolder::RemoveWarning(MDNS_WARNING_ID, MDNS_WARN_FPPD);
        }

        // Also browse for _wled._tcp so WLED nodes show up in the systems
        // list. FPP nodes also advertise _wled._tcp; per-IP dedup in
        // HandleResolveIP keeps them from being processed twice.
        void* wsb = avahi_service_browser_new(c, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_wled._tcp", NULL, (AvahiLookupFlags)0, browse_callback, mgr);
        if (!wsb) {
            LogErr(VB_SYNC, "Failed to create Avahi service browser for _wled._tcp: %s\n",
                   avahi_strerror(avahi_client_errno(c)));
            WarningHolder::AddWarning(MDNS_WARNING_ID, MDNS_WARN_WLED);
        } else {
            mgr->SetWLEDServiceBrowser(wsb);
            WarningHolder::RemoveWarning(MDNS_WARNING_ID, MDNS_WARN_WLED);
        }
    } else if (state == AVAHI_CLIENT_FAILURE) {
        LogWarn(VB_SYNC, "Avahi client failure: %s - will rebuild the client\n",
                avahi_strerror(avahi_client_errno(c)));
        // This client is finished; everything it owns (browsers, entry group) died
        // with it.  Freeing it here would be freeing the object whose callback we are
        // in, so hand the rebuild to the reconnect timer, which runs on the main loop
        // once Avahi is off the stack.
        mgr->NoteClientDown();
    }
}

// Build the connection to avahi-daemon.  Returns false (and schedules a retry) when
// the daemon isn't there to talk to.
bool MDNSManager::StartAvahiClient() {
    if (m_client)
        return true;
    if (!m_pollAdapter)
        return false;

    int error = 0;
    AvahiClient* client = avahi_client_new(static_cast<AvahiPoll*>(m_pollAdapter),
                                           (AvahiClientFlags)0, client_callback, this, &error);
    if (!client) {
        LogWarn(VB_SYNC, "Failed to create Avahi client: %s\n", avahi_strerror(error));
        NoteClientDown();
        return false;
    }
    m_client = client;
    return true;
}

void MDNSManager::NoteClientRunning() {
    if (m_clientDownSinceMS) {
        LogInfo(VB_SYNC, "Avahi client connected after %lld ms down\n",
                MDNSMonotonicMS() - m_clientDownSinceMS);
    }
    m_clientDownSinceMS = 0;
    m_reconnectDelayMS = MDNS_RECONNECT_MIN_MS;
    if (m_reconnectTimerFd >= 0) {
        struct itimerspec its = {};  // all-zero disarms
        timerfd_settime(m_reconnectTimerFd, 0, &its, NULL);
    }
    WarningHolder::RemoveWarning(MDNS_WARNING_ID, MDNS_WARN_CLIENT);
}

void MDNSManager::NoteClientDown() {
    long long now = MDNSMonotonicMS();
    if (m_clientDownSinceMS == 0) {
        m_clientDownSinceMS = now;
        m_reconnectDelayMS = MDNS_RECONNECT_MIN_MS;
    } else {
        m_reconnectDelayMS *= 2;
        if (m_reconnectDelayMS > MDNS_RECONNECT_MAX_MS)
            m_reconnectDelayMS = MDNS_RECONNECT_MAX_MS;

        // Only once the daemon has stayed gone, and never while fppd is on its way
        // down - a shutdown takes avahi-daemon with it, and that is not a fault.
        if ((now - m_clientDownSinceMS) >= MDNS_WARN_AFTER_MS && fppdIsRunning()) {
            WarningHolder::AddWarning(MDNS_WARNING_ID, MDNS_WARN_CLIENT);
        }
    }
    ScheduleClientReconnect();
}

void MDNSManager::ScheduleClientReconnect() {
    if (m_reconnectTimerFd < 0) {
        m_reconnectTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (m_reconnectTimerFd < 0) {
            LogWarn(VB_SYNC, "Failed to create timerfd for Avahi reconnect: %s\n",
                    FPPstrerror(errno));
            return;
        }
        m_reconnectCallback = [this](int) -> bool {
            uint64_t expirations = 0;
            ssize_t n = read(m_reconnectTimerFd, &expirations, sizeof(expirations));
            (void)n;
            ReconnectAvahiClient();
            return false;
        };
        EPollManager::INSTANCE.addFileDescriptor(m_reconnectTimerFd, m_reconnectCallback);
    }

    struct itimerspec its = {};
    its.it_value.tv_sec = m_reconnectDelayMS / 1000;
    its.it_value.tv_nsec = (m_reconnectDelayMS % 1000) * 1000000L;
    timerfd_settime(m_reconnectTimerFd, 0, &its, NULL);
}

// Runs on the main loop, not from inside an Avahi callback, so the dead client can
// safely be freed here.
void MDNSManager::ReconnectAvahiClient() {
    if (m_client) {
        // avahi_client_free() frees every object the client owns, so drop our handles
        // to the browsers and the entry group first rather than free them twice.
        m_serviceBrowser = nullptr;
        m_wledServiceBrowser = nullptr;
        m_entryGroup = nullptr;
        avahi_client_free(static_cast<AvahiClient*>(m_client));
        m_client = nullptr;
    }
    StartAvahiClient();
}

void MDNSManager::SetServiceBrowser(void* sb) {
    std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
    if (m_serviceBrowser) {
        avahi_service_browser_free(static_cast<AvahiServiceBrowser*>(m_serviceBrowser));
        m_serviceBrowser = nullptr;
    }
    m_serviceBrowser = sb;
}

void MDNSManager::SetWLEDServiceBrowser(void* sb) {
    std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
    if (m_wledServiceBrowser) {
        avahi_service_browser_free(static_cast<AvahiServiceBrowser*>(m_wledServiceBrowser));
        m_wledServiceBrowser = nullptr;
    }
    m_wledServiceBrowser = sb;
}

void MDNSManager::PickAlternativeServiceName() {
    char* alt = avahi_alternative_service_name(m_serviceName.c_str());
    if (alt) {
        m_serviceName = alt;
        avahi_free(alt);
    }
}

void MDNSManager::RegisterService(void* client) {
    AvahiClient* c = static_cast<AvahiClient*>(client);
    if (!c)
        return;

    // Free any existing entry group before creating a new one
    if (m_entryGroup) {
        avahi_entry_group_free(static_cast<AvahiEntryGroup*>(m_entryGroup));
        m_entryGroup = nullptr;
    }

    int error;
    AvahiEntryGroup* group = avahi_entry_group_new(c, entry_group_callback, this);
    if (!group) {
        LogWarn(VB_SYNC, "Failed to create Avahi entry group\n");
        return;
    }

    // Register _fppd._udp service on port 32320 (MultiSync port).
    error = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, (AvahiPublishFlags)0,
                                          m_serviceName.c_str(), "_fppd._udp", NULL, NULL, 32320, NULL);
    if (error < 0) {
        LogWarn(VB_SYNC, "Failed to add service: %s\n", avahi_strerror(error));
        avahi_entry_group_free(group);
        return;
    }

    // Also register _wled._tcp on the HTTP port so WLED-compatible apps
    // (notably WLED-iOS / WLED-native) discover the FPP node and treat
    // it as a WLED device. The mac= TXT record is what those apps key
    // off for device identity.
    {
        // Both backends advertise the same record, and finding the MAC is a
        // per-platform job rather than a per-library one, so it lives in
        // MDNSManager.cpp.  Already lowercase hex with no separators, which is
        // the form WLED expects.
        std::string macFlat = MDNSPrimaryMacAddress();
        std::string macTxt = "mac=" + macFlat;

        // Restrict _wled._tcp to IPv4 (AVAHI_PROTO_INET) even though
        // Apache is dual-stack. The WLED iOS UI renders the device's
        // address inline and a full IPv6 string wraps awkwardly; the
        // shorter v4 dotted-quad fits the layout cleanly.
        AvahiStringList* txt = avahi_string_list_new(macTxt.c_str(), NULL);
        int werror = avahi_entry_group_add_service_strlst(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET,
                                                         (AvahiPublishFlags)0, m_serviceName.c_str(),
                                                         "_wled._tcp", NULL, NULL, 80, txt);
        avahi_string_list_free(txt);
        if (werror < 0) {
            LogWarn(VB_SYNC, "Failed to add _wled._tcp service: %s\n", avahi_strerror(werror));
        } else {
            LogDebug(VB_SYNC, "MDNS _wled._tcp on port 80 (mac=%s)\n", macFlat.c_str());
        }
    }

    // Commit the entry group to make the service visible
    error = avahi_entry_group_commit(group);
    if (error < 0) {
        LogWarn(VB_SYNC, "Failed to commit entry group: %s\n", avahi_strerror(error));
        avahi_entry_group_free(group);
        return;
    }

    m_entryGroup = group;
    LogInfo(VB_SYNC, "MDNS service registration initiated for %s._fppd._udp on port 32320\n", m_serviceName.c_str());
}

// ── Backend entry points ────────────────────────────────────────────────

void MDNSManager::StartBackend() {
    // Build a custom AvahiPoll that delegates to EPollManager
    auto* poll = new AvahiPoll{};
    poll->watch_new        = fpp_watch_new;
    poll->watch_update     = fpp_watch_update;
    poll->watch_get_events = fpp_watch_get_events;
    poll->watch_free       = fpp_watch_free;
    poll->timeout_new      = fpp_timeout_new;
    poll->timeout_update   = fpp_timeout_update;
    poll->timeout_free     = fpp_timeout_free;
    poll->userdata         = this;
    m_pollAdapter = poll;
    m_running = true;

    // Failing here is not fatal and not permanent: fppd starts before avahi-daemon
    // on a cold boot often enough that giving up would leave mDNS off for the whole
    // session.  StartAvahiClient() arms the retry.
    StartAvahiClient();
}

void MDNSManager::StopBackend() {
    // Stop the reconnect retries first: it must not fire (and rebuild the client we
    // are about to free) part way through this.
    if (m_reconnectTimerFd >= 0) {
        EPollManager::INSTANCE.removeFileDescriptor(m_reconnectTimerFd);
        close(m_reconnectTimerFd);
        m_reconnectTimerFd = -1;
    }
    m_clientDownSinceMS = 0;

    // Free Avahi objects in dependency order.
    // Freeing the client triggers watch_free / timeout_free for its internal
    // watches and timeouts, which removes them from EPollManager automatically.
    if (m_entryGroup) {
        // Reset the group first, which forces the daemon to send mDNS
        // withdraw packets *before* the group is destroyed. Without this,
        // peers can keep our advertised services cached until the record
        // TTL expires (often >1 hour) — and on a force-killed fppd they'd
        // miss the goodbye entirely.
        avahi_entry_group_reset(static_cast<AvahiEntryGroup*>(m_entryGroup));
        avahi_entry_group_free(static_cast<AvahiEntryGroup*>(m_entryGroup));
        m_entryGroup = nullptr;
    }
    if (m_serviceBrowser) {
        avahi_service_browser_free(static_cast<AvahiServiceBrowser*>(m_serviceBrowser));
        m_serviceBrowser = nullptr;
    }
    if (m_wledServiceBrowser) {
        avahi_service_browser_free(static_cast<AvahiServiceBrowser*>(m_wledServiceBrowser));
        m_wledServiceBrowser = nullptr;
    }
    if (m_client) {
        avahi_client_free(static_cast<AvahiClient*>(m_client));
        m_client = nullptr;
    }
    if (m_pollAdapter) {
        delete static_cast<AvahiPoll*>(m_pollAdapter);
        m_pollAdapter = nullptr;
    }
    m_running = false;
}

#endif // HAVE_AVAHI
