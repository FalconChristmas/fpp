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

#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>

class MDNSManager {
public:
    static MDNSManager INSTANCE;

    MDNSManager();
    ~MDNSManager();

    /**
     * @param callbacks  placeholder matching other managers, not currently used
     */
    void Initialize(std::map<int, std::function<bool(int)>>& callbacks);
    void Cleanup();

    /**
     * Register a callback that will be invoked when a new MDNS-based host
     * address is discovered.  The argument is the IPv4 address of the host.
     * Returns an ID that can be used to remove the callback later.
     */
    int registerCallback(std::function<void(const std::string&)>&& callback);
    void removeCallback(int id);

    // Helpers used by the active backend's C callbacks (public so static
    // callbacks can reach them).  isWled is true when the host was discovered
    // via the _wled._tcp browser, in which case it is probed over HTTP rather
    // than the FPP ping protocol.
    void HandleResolveIP(const std::string& ip, bool isWled = false);
    void SetServiceBrowser(void* sb);
    void SetWLEDServiceBrowser(void* sb);
    void RegisterService(void* client); // register local _fppd._udp service
    void PickAlternativeServiceName();  // pick new name on collision

    // Client liveness, driven from the backend's client state callback.  Only
    // Avahi has a client that can go away; the Bonjour backend leaves these
    // empty.
    void NoteClientRunning(); // connected: clear the retry state and the warning
    void NoteClientDown();    // disconnected: start/continue the reconnect retries

    const std::string& ServiceName() const { return m_serviceName; }
    void SetServiceName(const std::string& name) { m_serviceName = name; }

private:
    // Defined by exactly one of MDNSBackendAvahi.cpp / MDNSBackendBonjour.cpp,
    // or by the no-op pair in MDNSManager.cpp when the platform has neither.
    void StartBackend();
    void StopBackend();

    // Set once the backend has started.  It does NOT mean there is a live
    // connection to a daemon - for Avahi that is m_client, which comes and goes.
    bool m_running = false;

    // Avahi's client never reconnects: once avahi-daemon goes away the client is
    // dead for good and a new one has to be built.  These drive that retry, and hold
    // off the "is avahi-daemon running?" warning until the outage looks real rather
    // than like the daemon restarting or the machine shutting down.  Bonjour has
    // no equivalent failure -- mDNSResponder is part of the OS -- so its backend
    // leaves them alone.
    int m_reconnectTimerFd = -1;
    std::function<bool(int)> m_reconnectCallback;
    long long m_clientDownSinceMS = 0; // 0 == client is up
    int m_reconnectDelayMS = 0;
    bool StartAvahiClient();
    void ScheduleClientReconnect();
    void ReconnectAvahiClient();

    std::set<std::string> m_knownHosts;

    std::recursive_mutex m_callbackLock;
    std::map<int, std::function<void(const std::string&)>> m_callbacks;
    int m_nextCallbackId = 1;

    // Service name (may be modified on collision)
    std::string m_serviceName;

    // Backend-owned handles.  Only the active backend file touches these, and
    // what they point at is that backend's business: AvahiPoll*, AvahiClient*,
    // AvahiServiceBrowser* and AvahiEntryGroup* for Avahi; DNSServiceRef for
    // Bonjour.  m_backendState is for whatever else a backend needs to keep --
    // Bonjour hangs its list of in-flight resolves there.
    void* m_pollAdapter = nullptr;        // Avahi only: custom AvahiPoll adapter
    void* m_client = nullptr;             // connection to the mDNS daemon
    void* m_serviceBrowser = nullptr;     // browser for _fppd._udp
    void* m_wledServiceBrowser = nullptr; // browser for _wled._tcp
    void* m_entryGroup = nullptr;         // the locally advertised services
    void* m_backendState = nullptr;       // backend's own bookkeeping
};
