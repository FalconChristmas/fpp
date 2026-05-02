#include "fpp-pch.h"

#include "MDNSManager.h"

#include "common.h"
#include "EPollManager.h"
#include "log.h"
#include "ping.h"
#include "MultiSync.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <vector>
#include <unistd.h>
#include <sys/time.h>

#if __has_include(<avahi-client/client.h>)
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/strlst.h>
#include <avahi-common/alternative.h>
#include <sys/timerfd.h>
#define HAVE_AVAHI 1
#else
#define HAVE_AVAHI 0
#endif

MDNSManager MDNSManager::INSTANCE;

MDNSManager::MDNSManager() = default;

MDNSManager::~MDNSManager() {
    Cleanup();
}

// ── Custom AvahiPoll adapter ────────────────────────────────────────────
// Avahi forward-declares AvahiWatch and AvahiTimeout as opaque types.
// The poll adapter provides the concrete definitions and wires them into
// the main FPP event loop via EPollManager.
// ─────────────────────────────────────────────────────────────────────────

#if HAVE_AVAHI

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

#endif // HAVE_AVAHI

// ── Avahi service callbacks ─────────────────────────────────────────────

#if HAVE_AVAHI
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
        // delegate to instance method to avoid accessing private members from C callback
        mgr->HandleResolveIP(ip);
    }

    avahi_service_resolver_free(r);
}
#endif

#if HAVE_AVAHI
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
#endif

#if HAVE_AVAHI
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
#endif

#if HAVE_AVAHI
static void client_callback(AvahiClient* c, AvahiClientState state, void* userdata) {
    MDNSManager* mgr = static_cast<MDNSManager*>(userdata);
    if (!mgr)
        return;

    if (state == AVAHI_CLIENT_S_RUNNING) {
        // Register local service
        mgr->RegisterService(c);

        // Create a service browser for _fppd._udp
        void* sb = avahi_service_browser_new(c, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_fppd._udp", NULL, (AvahiLookupFlags)0, browse_callback, mgr);
        if (!sb) {
            LogErr(VB_SYNC, "Failed to create Avahi service browser for _fppd._udp: %s\n",
                   avahi_strerror(avahi_client_errno(c)));
        } else {
            mgr->SetServiceBrowser(sb);
        }
    } else if (state == AVAHI_CLIENT_FAILURE) {
        LogErr(VB_SYNC, "Avahi client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
    }
}
#endif

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

#if HAVE_AVAHI
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
    m_avahiPoll = poll;

    int error;
    AvahiClient* client = avahi_client_new(poll, (AvahiClientFlags)0, client_callback, this, &error);
    if (!client) {
        LogWarn(VB_SYNC, "Failed to create Avahi client: %s\n", avahi_strerror(error));
        delete poll;
        m_avahiPoll = nullptr;
    } else {
        m_avahiClient = client;
        m_running = true;
    }
#else
    LogWarn(VB_SYNC, "Avahi development headers not available; MDNS disabled\n");
#endif
}

void MDNSManager::Cleanup() {
#if HAVE_AVAHI
    // Free Avahi objects in dependency order.
    // Freeing the client triggers watch_free / timeout_free for its internal
    // watches and timeouts, which removes them from EPollManager automatically.
    if (m_entryGroup) {
        avahi_entry_group_free(static_cast<AvahiEntryGroup*>(m_entryGroup));
        m_entryGroup = nullptr;
    }
    if (m_serviceBrowser) {
        avahi_service_browser_free(static_cast<AvahiServiceBrowser*>(m_serviceBrowser));
        m_serviceBrowser = nullptr;
    }
    if (m_avahiClient) {
        avahi_client_free(static_cast<AvahiClient*>(m_avahiClient));
        m_avahiClient = nullptr;
    }
    if (m_avahiPoll) {
        delete static_cast<AvahiPoll*>(m_avahiPoll);
        m_avahiPoll = nullptr;
    }
    m_running = false;
#endif
}

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

void MDNSManager::HandleResolveIP(const std::string& ip) {
    bool isNew = false;
    {
        std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
        if (m_knownHosts.insert(ip).second) {
            isNew = true;
        }
    }

    if (!isNew)
        return;

    LogDebug(VB_SYNC, "MDNS discovered new host %s\n", ip.c_str());

    // ping and notify MultiSync
    PingManager::INSTANCE.ping(ip, 1000, [ip](int result) {
        LogDebug(VB_SYNC, "MDNS ping %s result %d\n", ip.c_str(), result);
    });
    MultiSync::INSTANCE.PingSingleRemote(ip.c_str(), 1);

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

void MDNSManager::SetServiceBrowser(void* sb) {
    std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
    if (m_serviceBrowser) {
#if HAVE_AVAHI
        avahi_service_browser_free(static_cast<AvahiServiceBrowser*>(m_serviceBrowser));
#endif
        m_serviceBrowser = nullptr;
    }
    m_serviceBrowser = sb;
}

void MDNSManager::PickAlternativeServiceName() {
#if HAVE_AVAHI
    char* alt = avahi_alternative_service_name(m_serviceName.c_str());
    if (alt) {
        m_serviceName = alt;
        avahi_free(alt);
    }
#endif
}

void MDNSManager::RegisterService(void* client) {
#if HAVE_AVAHI
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
        // Pick the first non-loopback interface with a real MAC. Modern
        // Debian uses predictable names like "enxXX..." and "wlxXX..."
        // so we enumerate /sys/class/net rather than guessing.
        std::string mac;
        if (DIR* d = opendir("/sys/class/net")) {
            struct dirent* e;
            while ((e = readdir(d)) != nullptr) {
                if (e->d_name[0] == '.' || std::string(e->d_name) == "lo") continue;
                char path[256];
                snprintf(path, sizeof(path), "/sys/class/net/%s/address", e->d_name);
                FILE* f = fopen(path, "r");
                if (!f) continue;
                char buf[32] = {};
                if (fgets(buf, sizeof(buf), f)) {
                    std::string s(buf);
                    s.erase(s.find_last_not_of(" \t\r\n") + 1);
                    if (!s.empty() && s != "00:00:00:00:00:00") {
                        mac = s;
                        break;
                    }
                }
                fclose(f);
            }
            closedir(d);
        }
        // WLED uses lowercase hex with no colons in the mac TXT field.
        std::string macFlat;
        for (char c : mac) if (c != ':') macFlat.push_back(std::tolower(c));
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
#endif
}
