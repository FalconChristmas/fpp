#pragma once
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

#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "NetworkMonitor.h"

/*
 * Verifies which network interfaces can currently reach the internet and
 * publishes that as the top-level `internetState` field of fppd's status
 * payload (and therefore over /fppdws).  The probe runs on its own worker
 * thread: no web request, fppd main loop, or netlink callback ever blocks on
 * a network timeout, and one loop serves every open browser tab.
 *
 * Results are only ever read by GetCurrentFPPDStatus(), which takes a snapshot
 * under a short mutex -- no I/O from that (or any other status) thread.
 */
class InternetConnectivityMonitor {
public:
    struct Candidate {
        std::string name;
        std::string address;
    };

    static InternetConnectivityMonitor INSTANCE;

    void Init();
    void Shutdown();

    // Ask for an immediate re-probe.  A minimum interval in run() coalesces
    // bursts (several netlink events plus several tabs) into one probe.
    void wake();

    // Snapshot of interfaces with verified internet connectivity.  Interfaces
    // that are offline, disabled, or never probed are absent.
    std::map<std::string, bool> getState() const;

private:
    InternetConnectivityMonitor() = default;
    ~InternetConnectivityMonitor() = default;

    void run();
    bool doCycle();
    bool probeCandidate(const Candidate& c) const;
    bool icmpProbe(const Candidate& c) const;
    bool httpsProbe(const Candidate& c) const;

    std::vector<Candidate> enumerateCandidates() const;
    bool displayEnabled(const std::string& name) const;
    bool globallyEnabled() const;
    static bool isHiddenName(const std::string& name);

    std::atomic<bool> running{ false };
    std::thread worker;

    mutable std::mutex stateLock;
    std::map<std::string, bool> state;

    std::mutex wakeLock;
    std::condition_variable wakeCV;
    bool wakePending = false;

    // Kept alive for as long as the callback is registered: NetworkMonitor
    // dispatches through a std::function reference it does not own.
    std::function<void(NetworkMonitor::NetEventType, int, const std::string&)> netlinkCallback;
    int netlinkCallbackId = -1;
};
