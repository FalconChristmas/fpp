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

#include "fpp-pch.h"
#include "fpp-json.h"

#include <curl/curl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <cctype>

#ifndef PLATFORM_OSX
#include <netinet/ip_icmp.h>
#endif

#include "common.h"
#include "log.h"
#include "settings.h"
#include "Timers.h"
#include "NetworkMonitor.h"

#include "InternetConnectivityMonitor.h"

InternetConnectivityMonitor InternetConnectivityMonitor::INSTANCE;

namespace {
// Literal addresses only: a probe must not depend on DNS working, and each of
// these serves a certificate valid for the address itself so TLS verification
// can decide the HTTPS stage.  Shorter than the old PHP list on purpose: this
// runs in one fppd loop for all tabs, with backoff when there is no WAN, so
// five parallel connections per interface would add traffic for no extra
// confidence.
const char* kProbeTargets[] = { "1.1.1.1", "1.0.0.1", "8.8.8.8", "9.9.9.9" };

constexpr long long kNormalIntervalMS = 20000;
constexpr long long kMaxBackoffMS = 120000;
constexpr long long kWakeMinimumMS = 2000;
constexpr int kMaxCandidates = 4;

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) {
        return "";
    }
    size_t e = s.find_last_not_of(" \t\r\n");
    std::string v = s.substr(b, e - b + 1);
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') {
        v = v.substr(1, v.size() - 2);
    }
    return v;
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

size_t discardBody(char*, size_t size, size_t nmemb, void*) {
    return size * nmemb;
}
} // namespace

void InternetConnectivityMonitor::Init() {
    if (running.exchange(true)) {
        return;
    }
    netlinkCallback = [this](NetworkMonitor::NetEventType, int, const std::string&) {
        wake();
    };
    netlinkCallbackId = NetworkMonitor::INSTANCE.registerCallback(netlinkCallback);
    worker = std::thread([this]() { run(); });
    LogInfo(VB_GENERAL, "Internet connectivity monitor started\n");
}

void InternetConnectivityMonitor::Shutdown() {
    if (!running.exchange(false)) {
        return;
    }
    wakeCV.notify_all();
    if (worker.joinable()) {
        worker.join();
    }
    if (netlinkCallbackId >= 0) {
        NetworkMonitor::INSTANCE.removeCallback(netlinkCallbackId);
        netlinkCallbackId = -1;
    }
    std::lock_guard<std::mutex> lk(stateLock);
    state.clear();
}

void InternetConnectivityMonitor::wake() {
    std::lock_guard<std::mutex> lk(wakeLock);
    wakePending = true;
    wakeCV.notify_one();
}

std::map<std::string, bool> InternetConnectivityMonitor::getState() const {
    std::lock_guard<std::mutex> lk(stateLock);
    return state;
}

bool InternetConnectivityMonitor::globallyEnabled() const {
    // Absent or empty means ON: the documented default is to show the badge,
    // so a hand-edited settings line with no value must not silently disable it.
    std::string v = trim(getSetting("NetworkInternetProbe", "1"));
    if (v.empty()) {
        return true;
    }
    v = lower(v);
    return !(v == "0" || v == "false" || v == "no" || v == "off");
}

bool InternetConnectivityMonitor::isHiddenName(const std::string& name) {
    if (name.empty()) {
        return true;
    }
    static const char* excluded[] = { "lo", "lo0", "usb0", "usb1", "SoftAp0" };
    for (const char* e : excluded) {
        if (name == e) {
            return true;
        }
    }
    // RefreshHeaderBar() in fpp.js never renders these names; probing them
    // would spend a timeout on a dot no page can show.  Tunnels (wg0/tun0/
    // ppp0) are deliberately not excluded: they can be the only internet path.
    static const char* prefixes[] = { "eth0:0", "usb", "veth", "br", "docker", "can." };
    for (const char* p : prefixes) {
        if (name.compare(0, strlen(p), p) == 0) {
            return true;
        }
    }
    return false;
}

bool InternetConnectivityMonitor::displayEnabled(const std::string& name) const {
    // config/interface.<name> is written by the Network Configuration page.
    // INTERNETSTATUS is opt-out: absent or empty means ON.  "0" (or the usual
    // textual false spellings) turns the badge -- and the probe -- off.
    if (name.empty() || name.find('/') != std::string::npos) {
        return false;
    }
    std::ifstream in(FPP_DIR_CONFIG("/interface." + name));
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        if (trim(line.substr(0, eq)) != "INTERNETSTATUS") {
            continue;
        }
        std::string v = lower(trim(line.substr(eq + 1)));
        if (v.empty()) {
            return true;
        }
        return !(v == "0" || v == "false" || v == "no" || v == "off");
    }
    return true;
}

std::vector<InternetConnectivityMonitor::Candidate> InternetConnectivityMonitor::enumerateCandidates() const {
    std::vector<Candidate> out;
    struct ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0 || !ifa) {
        return out;
    }
    for (struct ifaddrs* p = ifa; p != nullptr; p = p->ifa_next) {
        if (!p->ifa_name || !p->ifa_addr || p->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        std::string name = p->ifa_name;
        if (isHiddenName(name) || !displayEnabled(name)) {
            continue;
        }
        if (!(p->ifa_flags & IFF_UP) || !(p->ifa_flags & IFF_RUNNING)) {
            continue;
        }
        char buf[INET_ADDRSTRLEN] = { 0 };
        if (!inet_ntop(AF_INET, &((struct sockaddr_in*)p->ifa_addr)->sin_addr, buf, sizeof(buf))) {
            continue;
        }
        std::string addr = buf;
        if (addr == "0.0.0.0" || addr.compare(0, 4, "127.") == 0 || addr.compare(0, 8, "169.254.") == 0) {
            continue;
        }
        out.push_back({ name, addr });
    }
    freeifaddrs(ifa);

#ifndef PLATFORM_OSX
    // Probe the interfaces that carry a default route first, so the cap below
    // never drops the primary uplink in favour of a virtual adapter.
    std::map<std::string, bool> defaultRoute;
    std::ifstream routes("/proc/net/route");
    std::string line;
    while (std::getline(routes, line)) {
        std::istringstream ss(line);
        std::string iface, dest, mask;
        if (ss >> iface >> dest >> mask && dest == "00000000" && mask == "00000000") {
            defaultRoute[iface] = true;
        }
    }
    std::stable_sort(out.begin(), out.end(), [&defaultRoute](const Candidate& a, const Candidate& b) {
        return defaultRoute.count(a.name) > defaultRoute.count(b.name);
    });
#endif
    if ((int)out.size() > kMaxCandidates) {
        out.resize(kMaxCandidates);
    }
    return out;
}

bool InternetConnectivityMonitor::icmpProbe(const Candidate& c) const {
#ifdef PLATFORM_OSX
    // Unprivileged raw sockets are not available to the GUI-session fppd on
    // macOS; the HTTPS stage is the reachability proof there.
    return false;
#else
    // SOCK_DGRAM ICMP lets the kernel do the checksum, and SO_BINDTODEVICE
    // makes the echo leave through this interface rather than the kernel's
    // preferred default route.  No fork, no shell utility.
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (fd < 0) {
        return false;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, c.name.c_str(), (socklen_t)c.name.size()) < 0) {
        close(fd);
        return false;
    }

    struct icmp request;
    memset(&request, 0, sizeof(request));
    request.icmp_type = ICMP_ECHO;
    request.icmp_id = (uint16_t)(getpid() & 0xffff);
    request.icmp_seq = 1;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    inet_pton(AF_INET, kProbeTargets[0], &dest.sin_addr);
    if (sendto(fd, &request, sizeof(request), 0, (struct sockaddr*)&dest, sizeof(dest)) != (ssize_t)sizeof(request)) {
        close(fd);
        return false;
    }

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    bool online = false;
    if (poll(&pfd, 1, 1000) > 0) {
        char buf[512];
        struct sockaddr_in from;
        socklen_t fromLen = sizeof(from);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
        if (n >= (ssize_t)sizeof(struct icmp)) {
            struct icmp* reply = (struct icmp*)buf;
            online = reply->icmp_type == ICMP_ECHOREPLY && reply->icmp_id == request.icmp_id;
        }
    }
    close(fd);
    return online;
#endif
}

bool InternetConnectivityMonitor::httpsProbe(const Candidate& c) const {
    CURLM* multi = curl_multi_init();
    if (!multi) {
        return false;
    }
    std::vector<CURL*> handles;
    for (const char* ip : kProbeTargets) {
        CURL* ch = curl_easy_init();
        if (!ch) {
            continue;
        }
        std::string url = std::string("https://") + ip + "/";
        curl_easy_setopt(ch, CURLOPT_URL, url.c_str());
        // Linux binds to the device name so the packet cannot ride another
        // interface's default route.  macOS libcurl does not reliably support
        // device-name binds, so use this interface's own source address there.
#ifdef PLATFORM_OSX
        curl_easy_setopt(ch, CURLOPT_INTERFACE, c.address.c_str());
#else
        curl_easy_setopt(ch, CURLOPT_INTERFACE, c.name.c_str());
#endif
        curl_easy_setopt(ch, CURLOPT_TIMEOUT_MS, 2000L);
        curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT_MS, 1000L);
        curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 0L);
        curl_easy_setopt(ch, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(ch, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(ch, CURLOPT_NOPROXY, "*");
        curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(ch, CURLOPT_USERAGENT, "FPP");
        curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, discardBody);
        if (curl_multi_add_handle(multi, ch) != CURLM_OK) {
            curl_easy_cleanup(ch);
            continue;
        }
        handles.push_back(ch);
    }
    if (handles.empty()) {
        curl_multi_cleanup(multi);
        return false;
    }

    bool online = false;
    int stillRunning = 0;
    do {
        CURLMcode mc = curl_multi_perform(multi, &stillRunning);
        if (mc != CURLM_OK) {
            break;
        }
        if (stillRunning) {
            int fds = 0;
            curl_multi_wait(multi, nullptr, 0, 100, &fds);
        }
        int queued = 0;
        CURLMsg* msg = nullptr;
        while ((msg = curl_multi_info_read(multi, &queued)) != nullptr) {
            if (msg->msg == CURLMSG_DONE && msg->data.result == CURLE_OK) {
                long verify = 0;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_SSL_VERIFYRESULT, &verify);
                if (verify == 0) {
                    online = true;
                }
            }
        }
    } while (!online && stillRunning > 0);

    for (CURL* ch : handles) {
        curl_multi_remove_handle(multi, ch);
        curl_easy_cleanup(ch);
    }
    curl_multi_cleanup(multi);
    return online;
}

bool InternetConnectivityMonitor::probeCandidate(const Candidate& c) const {
    // Inconclusive means no dot: ICMP is cheap and works before NTP has set a
    // correct clock; a filtered ICMP is decided by verified HTTPS instead.
    return icmpProbe(c) || httpsProbe(c);
}

bool InternetConnectivityMonitor::doCycle() {
    if (!globallyEnabled()) {
        std::lock_guard<std::mutex> lk(stateLock);
        state.clear();
        return false;
    }

    std::vector<Candidate> candidates = enumerateCandidates();
    std::map<std::string, bool> next;
    bool anyOnline = false;
    for (const Candidate& c : candidates) {
        bool online = probeCandidate(c);
        if (online) {
            next[c.name] = true;
            anyOnline = true;
        }
        LogDebug(VB_GENERAL, "Internet probe %s (%s): %s\n",
                 c.name.c_str(), c.address.c_str(), online ? "online" : "offline");
    }
    {
        std::lock_guard<std::mutex> lk(stateLock);
        state.swap(next);
    }
    return anyOnline;
}

void InternetConnectivityMonitor::run() {
    long long interval = kNormalIntervalMS;
    long long lastProbe = 0;
    while (running.load()) {
        {
            std::unique_lock<std::mutex> lk(wakeLock);
            long long delay = interval;
            if (wakePending) {
                long long since = GetMonotonicTimeMS() - lastProbe;
                delay = since >= kWakeMinimumMS ? 0 : kWakeMinimumMS - since;
            }
            if (delay > 0) {
                wakeCV.wait_for(lk, std::chrono::milliseconds(delay), [this]() {
                    return wakePending || !running.load();
                });
            }
            if (!running.load()) {
                break;
            }
            wakePending = false;
        }

        lastProbe = GetMonotonicTimeMS();
        bool anyOnline = doCycle();
        // Back off when nothing is reachable -- a show LAN with no WAN is the
        // common case, and there is no point paying a timeout every 20 s.
        interval = anyOnline ? kNormalIntervalMS : std::min(interval * 2, kMaxBackoffMS);
    }
}
