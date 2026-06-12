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
#include <set>
#include <string>
#include <mutex>

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

    // Helper used by Avahi callbacks (public so static C callbacks can call)
    // isWled is true when the host was discovered via the _wled._tcp browser,
    // in which case it is probed over HTTP rather than the FPP ping protocol.
    void HandleResolveIP(const std::string& ip, bool isWled = false);
    void SetServiceBrowser(void* sb);
    void SetWLEDServiceBrowser(void* sb);
    void RegisterService(void* client);  // register local _fppd._udp service
    void PickAlternativeServiceName();    // pick new name on collision

private:
    bool m_running = false;

    std::set<std::string> m_knownHosts;

    std::recursive_mutex m_callbackLock;
    std::map<int, std::function<void(const std::string&)>> m_callbacks;
    int m_nextCallbackId = 1;

    // Service name (may be modified on collision)
    std::string m_serviceName;

    // Avahi objects (opaque pointers) - only used in implementation
    void* m_avahiPoll = nullptr;     // custom AvahiPoll adapter
    void* m_avahiClient = nullptr;
    void* m_serviceBrowser = nullptr;       // browser for _fppd._udp
    void* m_wledServiceBrowser = nullptr;   // browser for _wled._tcp
    void* m_entryGroup = nullptr;    // for registering local service
};
