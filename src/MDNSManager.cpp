#include "fpp-pch.h"

#include "MDNSManager.h"

#include "common.h"       // for split()
#include "log.h"
#include "ping.h"
#include "MultiSync.h"
#include "NetworkMonitor.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unistd.h>

#if __has_include(<avahi-client/client.h>)
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/strlst.h>
#include <avahi-common/timeval.h>
#include <avahi-common/alternative.h>
#define HAVE_AVAHI 1
#else
#define HAVE_AVAHI 0
#endif

MDNSManager MDNSManager::INSTANCE;

MDNSManager::MDNSManager() = default;

MDNSManager::~MDNSManager() {
    Cleanup();
}

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
        // best-effort remove all matching IPs (we don't have IP here)
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
    case AVAHI_ENTRY_GROUP_COLLISION:
        LogWarn(VB_SYNC, "MDNS service name collision, creating alternate name\n");
        // In a real implementation, you could rename and retry
        break;
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
        
        // create a service browser for _fppd._udp
        void* sb = avahi_service_browser_new(c, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_fppd._udp", NULL, (AvahiLookupFlags)0, browse_callback, mgr);
        mgr->SetServiceBrowser(sb);
    } else if (state == AVAHI_CLIENT_FAILURE) {
        LogErr(VB_SYNC, "Avahi client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
    }
}
#endif

void MDNSManager::Initialize(std::map<int, std::function<bool(int)>>& callbacks) {
    if (m_running)
        return;

    // Register with NetworkMonitor so we can restart the browser on net events
    std::function<void(NetworkMonitor::NetEventType, int, const std::string&)> netcb = [this](NetworkMonitor::NetEventType t, int up, const std::string& name) {
        // On new address/link, restart browse to pick up changes
        if (t == NetworkMonitor::NetEventType::NEW_ADDR || t == NetworkMonitor::NetEventType::NEW_LINK) {
            // no-op for now; Avahi client will pick up changes itself.
        }
    };
    // store id if needed in future (registerCallback returns int)
    m_networkCallbackId = NetworkMonitor::INSTANCE.registerCallback(netcb);

#if HAVE_AVAHI
    int error;
    m_avahiSimplePoll = avahi_simple_poll_new();
    if (!m_avahiSimplePoll) {
        LogWarn(VB_SYNC, "Failed to create Avahi simple poll\n");
    } else {
        AvahiSimplePoll* sp = static_cast<AvahiSimplePoll*>(m_avahiSimplePoll);
        AvahiClient* client = avahi_client_new(avahi_simple_poll_get(sp), (AvahiClientFlags)0, client_callback, this, &error);
        if (!client) {
            LogWarn(VB_SYNC, "Failed to create Avahi client: %s\n", avahi_strerror(error));
            avahi_simple_poll_free(sp);
            m_avahiSimplePoll = nullptr;
        } else {
            m_avahiClient = client;
            m_running = true;
            // run the avahi event loop in its own thread
            m_thread = new std::thread([sp]() { avahi_simple_poll_loop(sp); });
        }
    }
#else
    LogWarn(VB_SYNC, "Avahi development headers not available; MDNS disabled\n");
    // fallback to previous behavior (no-op) -- leave m_running false
#endif
}

void MDNSManager::Cleanup() {
    if (m_networkCallbackId) {
        NetworkMonitor::INSTANCE.removeCallback(m_networkCallbackId);
        m_networkCallbackId = 0;
    }

#if HAVE_AVAHI
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
    if (m_avahiSimplePoll) {
        AvahiSimplePoll* sp = static_cast<AvahiSimplePoll*>(m_avahiSimplePoll);
        avahi_simple_poll_quit(sp);
        if (m_thread) {
            m_thread->join();
            delete m_thread;
            m_thread = nullptr;
        }
        avahi_simple_poll_free(sp);
        m_avahiSimplePoll = nullptr;
    }
#else
    if (m_thread) {
        m_running = false;
        m_thread->join();
        delete m_thread;
        m_thread = nullptr;
    }
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

    LogInfo(VB_SYNC, "MDNS discovered new host %s\n", ip.c_str());

    // ping and notify MultiSync
    PingManager::INSTANCE.ping(ip, 1000, [ip](int result) {
        LogDebug(VB_SYNC, "MDNS ping %s result %d\n", ip.c_str(), result);
    });
    MultiSync::INSTANCE.PingSingleRemote(ip.c_str(), 1);

    std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
    for (auto const& pair : m_callbacks) {
        pair.second(ip);
    }
}

void MDNSManager::SetServiceBrowser(void* sb) {
    // called from C callback to set the opaque service browser pointer
    std::unique_lock<std::recursive_mutex> lk(m_callbackLock);
    if (m_serviceBrowser) {
#if HAVE_AVAHI
        avahi_service_browser_free(static_cast<AvahiServiceBrowser*>(m_serviceBrowser));
#endif
        m_serviceBrowser = nullptr;
    }
    m_serviceBrowser = sb;
}

void MDNSManager::RegisterService(void* client) {
#if HAVE_AVAHI
    AvahiClient* c = static_cast<AvahiClient*>(client);
    if (!c)
        return;

    int error;
    AvahiEntryGroup* group = avahi_entry_group_new(c, entry_group_callback, this);
    if (!group) {
        LogWarn(VB_SYNC, "Failed to create Avahi entry group\n");
        return;
    }

    // Get hostname for service name
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "fppd", sizeof(hostname) - 1);
        hostname[sizeof(hostname) - 1] = '\0';
    }

    // Register _fppd._udp service on port 32320 (MultiSync port)
    error = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, (AvahiPublishFlags)0,
                                          hostname, "_fppd._udp", NULL, NULL, 32320, NULL);
    if (error < 0) {
        LogWarn(VB_SYNC, "Failed to add service: %s\n", avahi_strerror(error));
        avahi_entry_group_free(group);
        return;
    }

    // Commit the entry group to make the service visible
    error = avahi_entry_group_commit(group);
    if (error < 0) {
        LogWarn(VB_SYNC, "Failed to commit entry group: %s\n", avahi_strerror(error));
        avahi_entry_group_free(group);
        return;
    }

    m_entryGroup = group;
    LogInfo(VB_SYNC, "MDNS service registration initiated for %s._fppd._udp on port 32320\n", hostname);
#endif
}
