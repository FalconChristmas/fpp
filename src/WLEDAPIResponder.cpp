/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2026 by the Falcon Player Developers.
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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <sstream>

#include "WLEDAPIResponder.h"

#include "common.h"
#include "fppversion.h"
#include "log.h"
#include "settings.h"

#include "commands/Commands.h"
#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayEffects.h"
#include "overlays/PixelOverlayModel.h"
#include "overlays/WLEDEffects.h"

WLEDAPIResponder WLEDAPIResponder::INSTANCE;

namespace {

// RFC 6455 magic GUID, appended to the client's Sec-WebSocket-Key before
// SHA1 + base64 to produce the Sec-WebSocket-Accept response.
constexpr const char* WS_MAGIC = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string headerValue(const std::string& headers, const std::string& name) {
    // Case-insensitive header lookup.
    std::string lname = name;
    std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
    std::istringstream is(headers);
    std::string line;
    while (std::getline(is, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string h = line.substr(0, colon);
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
        if (h != lname) continue;
        std::string v = line.substr(colon + 1);
        // Trim leading spaces and trailing CR/whitespace.
        size_t s = v.find_first_not_of(" \t");
        size_t e = v.find_last_not_of(" \t\r\n");
        if (s == std::string::npos) return "";
        return v.substr(s, e - s + 1);
    }
    return "";
}

// Read until "\r\n\r\n", which separates HTTP headers from body.
// Returns the full headers-and-line string, or empty on error.
bool readUntilHeadersEnd(int fd, std::string& out, size_t maxBytes = 16384) {
    char buf[1024];
    while (out.size() < maxBytes) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) return false;
        out.append(buf, n);
        if (out.find("\r\n\r\n") != std::string::npos) return true;
    }
    return false;
}

bool readExact(int fd, void* dst, size_t n) {
    auto* p = static_cast<uint8_t*>(dst);
    while (n) {
        ssize_t r = ::recv(fd, p, n, 0);
        if (r <= 0) return false;
        p += r;
        n -= r;
    }
    return true;
}

bool writeAll(int fd, const void* src, size_t n) {
    auto* p = static_cast<const uint8_t*>(src);
    while (n) {
        ssize_t w = ::send(fd, p, n, MSG_NOSIGNAL);
        if (w <= 0) return false;
        p += w;
        n -= w;
    }
    return true;
}

std::string sha1Base64(const std::string& in) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(in.data()), in.size(), hash);
    return base64Encode(hash, SHA_DIGEST_LENGTH);
}

void closeFd(int& fd) {
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
    }
}

// Pick the first non-loopback interface and return its MAC (lowercase
// hex, no colons — the form WLED uses) along with its primary IPv4.
void primaryNetIdentity(std::string& macFlat, std::string& ipv4) {
    macFlat.clear();
    ipv4.clear();
    std::string ifname;

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
                    for (char c : s) if (c != ':') macFlat.push_back(std::tolower(c));
                    ifname = e->d_name;
                    fclose(f);
                    break;
                }
            }
            fclose(f);
        }
        closedir(d);
    }

    if (!ifname.empty()) {
        ifaddrs* ifa = nullptr;
        if (getifaddrs(&ifa) == 0) {
            for (auto* p = ifa; p; p = p->ifa_next) {
                if (!p->ifa_addr || p->ifa_addr->sa_family != AF_INET) continue;
                if (ifname != p->ifa_name) continue;
                char buf[INET_ADDRSTRLEN];
                auto* in = reinterpret_cast<sockaddr_in*>(p->ifa_addr);
                if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf))) {
                    ipv4 = buf;
                    break;
                }
            }
            freeifaddrs(ifa);
        }
    }
}

} // namespace

WLEDAPIResponder::WLEDAPIResponder() = default;

WLEDAPIResponder::~WLEDAPIResponder() {
    Cleanup();
}

void WLEDAPIResponder::Initialize() {
    if (m_running) return;

    // Allow disabling via /media/settings.
    std::string en = getSetting("WLEDAPIResponder");
    if (en == "0" || en == "false" || en == "off") {
        LogInfo(VB_GENERAL, "WLEDAPIResponder disabled by setting\n");
        return;
    }

    int p = getSettingInt("WLEDAPIPort", 0);
    if (p > 0 && p < 65536) m_port = p;

    m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) {
        LogErr(VB_GENERAL, "WLEDAPIResponder: socket() failed: %s\n", strerror(errno));
        return;
    }

    int opt = 1;
    ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // proxied through Apache
    addr.sin_port = htons(m_port);

    if (::bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LogErr(VB_GENERAL, "WLEDAPIResponder: bind(%d) failed: %s\n", m_port, strerror(errno));
        closeFd(m_socket);
        return;
    }
    if (::listen(m_socket, 8) < 0) {
        LogErr(VB_GENERAL, "WLEDAPIResponder: listen() failed: %s\n", strerror(errno));
        closeFd(m_socket);
        return;
    }

    m_running = true;
    m_acceptThread = new std::thread([this]() {
        SetThreadName("FPP-WLEDAPI");
        AcceptLoop();
    });

    LogInfo(VB_GENERAL, "WLEDAPIResponder: listening on 127.0.0.1:%d\n", m_port);
}

void WLEDAPIResponder::Cleanup() {
    if (!m_running && m_socket < 0) return;
    m_running = false;

    // shutdown() before close() — on Linux, plain close() on a socket
    // that another thread is blocked inside accept()/recv() does not
    // reliably wake that thread. shutdown(SHUT_RDWR) does.
    if (m_socket >= 0) {
        ::shutdown(m_socket, SHUT_RDWR);
        closeFd(m_socket);
    }

    // Shut down every per-connection socket too — accept-thread might
    // have handed off a connection that's now blocked in recv().
    {
        std::unique_lock<std::mutex> lock(m_clientsLock);
        for (int fd : m_clientFds) ::shutdown(fd, SHUT_RDWR);
        m_wsClients.clear();
    }

    if (m_acceptThread) {
        m_acceptThread->join();
        delete m_acceptThread;
        m_acceptThread = nullptr;
    }
    for (auto* t : m_clientThreads) {
        if (t->joinable()) t->join();
        delete t;
    }
    m_clientThreads.clear();
    {
        std::unique_lock<std::mutex> lock(m_clientsLock);
        m_clientFds.clear();
    }
}

void WLEDAPIResponder::AcceptLoop() {
    while (m_running) {
        int client = ::accept(m_socket, nullptr, nullptr);
        if (client < 0) {
            if (m_running) LogWarn(VB_GENERAL, "WLEDAPIResponder: accept failed: %s\n", strerror(errno));
            continue;
        }
        // Hand off to a per-connection thread. WebSocket streams are
        // long-lived, but there will only ever be a handful of clients
        // (one mobile app at a time, maybe a couple), so a dedicated
        // thread per connection is simpler than juggling select/epoll.
        {
            // Track every connection fd so Cleanup() can shutdown
            // them all. ClientLoop is responsible for removing its
            // fd when the connection ends naturally.
            std::unique_lock<std::mutex> lock(m_clientsLock);
            m_clientFds.push_back(client);
        }
        auto* t = new std::thread([this, client]() {
            SetThreadName("FPP-WLEDAPI-cli");
            ClientLoop(client);
            std::unique_lock<std::mutex> lock(m_clientsLock);
            m_clientFds.erase(
                std::remove(m_clientFds.begin(), m_clientFds.end(), client),
                m_clientFds.end());
        });
        std::unique_lock<std::mutex> lock(m_clientsLock);
        m_clientThreads.push_back(t);
    }
}

void WLEDAPIResponder::ClientLoop(int fd) {
    std::string headerBlock;
    if (!readUntilHeadersEnd(fd, headerBlock)) {
        closeFd(fd);
        return;
    }

    auto headerEnd = headerBlock.find("\r\n\r\n");
    std::string requestLine;
    std::string headers;
    std::string body = headerBlock.substr(headerEnd + 4);

    auto firstLineEnd = headerBlock.find("\r\n");
    if (firstLineEnd != std::string::npos) {
        requestLine = headerBlock.substr(0, firstLineEnd);
        headers = headerBlock.substr(firstLineEnd + 2, headerEnd - firstLineEnd - 2);
    }

    // If the request is a WebSocket upgrade, do the handshake and then
    // keep the connection open in WS mode.
    std::string upgrade = headerValue(headers, "Upgrade");
    std::transform(upgrade.begin(), upgrade.end(), upgrade.begin(), ::tolower);
    std::string wsKey = headerValue(headers, "Sec-WebSocket-Key");

    if (upgrade == "websocket" && !wsKey.empty()) {
        if (!DoWebSocketHandshake(fd, wsKey)) {
            closeFd(fd);
            return;
        }
        // Send initial state snapshot.
        SendTextFrame(fd, BuildCombinedJson());
        {
            std::unique_lock<std::mutex> lock(m_clientsLock);
            m_wsClients.push_back(fd);
        }
        RunWebSocket(fd);
        {
            std::unique_lock<std::mutex> lock(m_clientsLock);
            m_wsClients.erase(std::remove(m_wsClients.begin(), m_wsClients.end(), fd),
                              m_wsClients.end());
        }
        closeFd(fd);
        return;
    }

    // Otherwise treat as plain HTTP. We need the body if Content-Length
    // says so; recv any remainder.
    int contentLen = std::atoi(headerValue(headers, "Content-Length").c_str());
    while (static_cast<int>(body.size()) < contentLen) {
        char buf[1024];
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        body.append(buf, n);
    }

    HandleHTTPRequest(fd, requestLine, headers, body);
    closeFd(fd);
}

bool WLEDAPIResponder::DoWebSocketHandshake(int fd, const std::string& key) {
    std::string accept = sha1Base64(key + WS_MAGIC);
    std::ostringstream resp;
    resp << "HTTP/1.1 101 Switching Protocols\r\n"
         << "Upgrade: websocket\r\n"
         << "Connection: Upgrade\r\n"
         << "Sec-WebSocket-Accept: " << accept << "\r\n"
         << "Server: fppd/" << getFPPVersion() << "\r\n"
         << "\r\n";
    std::string s = resp.str();
    return writeAll(fd, s.data(), s.size());
}

bool WLEDAPIResponder::SendTextFrame(int fd, const std::string& payload) {
    std::vector<uint8_t> frame;
    frame.reserve(payload.size() + 10);
    frame.push_back(0x81); // FIN + text opcode
    size_t len = payload.size();
    if (len < 126) {
        frame.push_back(static_cast<uint8_t>(len));
    } else if (len <= 0xffff) {
        frame.push_back(126);
        frame.push_back((len >> 8) & 0xff);
        frame.push_back(len & 0xff);
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i) frame.push_back((len >> (i * 8)) & 0xff);
    }
    frame.insert(frame.end(), payload.begin(), payload.end());
    return writeAll(fd, frame.data(), frame.size());
}

bool WLEDAPIResponder::SendCloseFrame(int fd) {
    uint8_t close[2] = { 0x88, 0x00 };
    return writeAll(fd, close, sizeof(close));
}

int WLEDAPIResponder::ReadFrame(int fd, std::string& out) {
    uint8_t hdr[2];
    if (!readExact(fd, hdr, 2)) return -1;

    uint8_t opcode = hdr[0] & 0x0f;
    bool masked   = (hdr[1] & 0x80) != 0;
    uint64_t len  = hdr[1] & 0x7f;

    if (len == 126) {
        uint8_t e[2];
        if (!readExact(fd, e, 2)) return -1;
        len = (uint64_t(e[0]) << 8) | e[1];
    } else if (len == 127) {
        uint8_t e[8];
        if (!readExact(fd, e, 8)) return -1;
        len = 0;
        for (int i = 0; i < 8; ++i) len = (len << 8) | e[i];
    }

    uint8_t mask[4] = {};
    if (masked && !readExact(fd, mask, 4)) return -1;

    // Sanity cap — the WLED iOS app sends very small messages.
    if (len > 65536) return -1;

    std::string payload;
    payload.resize(len);
    if (len && !readExact(fd, payload.data(), len)) return -1;
    if (masked) {
        for (uint64_t i = 0; i < len; ++i) payload[i] ^= mask[i & 3];
    }

    switch (opcode) {
    case 0x1: // text
        out = std::move(payload);
        return 1;
    case 0x8: // close
        SendCloseFrame(fd);
        return -1;
    case 0x9: { // ping → pong with same payload
        std::vector<uint8_t> pong;
        pong.push_back(0x8a);
        pong.push_back(static_cast<uint8_t>(payload.size()));
        pong.insert(pong.end(), payload.begin(), payload.end());
        writeAll(fd, pong.data(), pong.size());
        return 0;
    }
    case 0xa: // pong
        return 0;
    default:
        // Binary, continuation, or anything else — ignore for now.
        return 0;
    }
}

void WLEDAPIResponder::RunWebSocket(int fd) {
    std::string msg;
    while (m_running) {
        int r = ReadFrame(fd, msg);
        if (r < 0) break;
        if (r == 0) continue;

        // Apply incoming WledState JSON, then echo back the new combined
        // {state, info} just like the real WLED node does.
        ApplyStatePost(msg);
        if (!SendTextFrame(fd, BuildCombinedJson())) break;
    }
}

void WLEDAPIResponder::NotifyStateChanged() {
    std::string snapshot = BuildCombinedJson();
    std::unique_lock<std::mutex> lock(m_clientsLock);
    for (int fd : m_wsClients) {
        // Best-effort; failed sockets get reaped by their ClientLoop.
        SendTextFrame(fd, snapshot);
    }
}

bool WLEDAPIResponder::HandleHTTPRequest(int fd, const std::string& reqLine,
                                         const std::string& /*headers*/,
                                         const std::string& body) {
    std::string method, path;
    {
        std::istringstream is(reqLine);
        is >> method >> path;
    }

    auto respond = [&](int code, const std::string& ctype, const std::string& payload) {
        std::ostringstream r;
        r << "HTTP/1.1 " << code << " OK\r\n"
          << "Content-Type: " << ctype << "\r\n"
          << "Access-Control-Allow-Origin: *\r\n"
          << "Cache-Control: no-store\r\n"
          << "Content-Length: " << payload.size() << "\r\n"
          << "Connection: close\r\n\r\n"
          << payload;
        std::string s = r.str();
        writeAll(fd, s.data(), s.size());
    };

    if (path == "/json/info" || path == "/json/info/") {
        respond(200, "application/json", BuildInfoJson());
    } else if (path == "/json/state" || path == "/json/state/") {
        if (method == "POST" || method == "PUT") ApplyStatePost(body);
        respond(200, "application/json", BuildStateJson());
    } else if (path == "/json/eff" || path == "/json/eff/") {
        respond(200, "application/json", BuildEffectsJson());
    } else if (path == "/json" || path == "/json/") {
        respond(200, "application/json", BuildCombinedJson());
    } else {
        respond(404, "text/plain", "not found\n");
    }
    return true;
}

// ----- JSON builders ------------------------------------------------------

std::string WLEDAPIResponder::BuildInfoJson() {
    Json::Value info;
    info["ver"] = getFPPVersion();
    info["vid"] = 0;
    info["cn"] = "FPP";
    info["release"] = getFPPVersion();
    // Prefer the actual hostname for the display name. HostDescription
    // is an optional free-text label; if it's empty or still set to the
    // default placeholder "FPP", fall back to HostName so the WLED
    // client shows the same identifier the user sees on the FPP UI and
    // on the network (matching the mDNS service name).
    std::string hn = getSetting("HostName");
    std::string hd = getSetting("HostDescription");
    std::string nm;
    if (!hd.empty() && hd != "FPP") {
        nm = hd;
    } else if (!hn.empty()) {
        nm = hn;
    } else {
        nm = "FPP";
    }
    info["name"] = nm;
    info["brand"] = "FPP";
    info["product"] = "FalconPlayer";
    info["arch"] = getSetting("Platform");

    Json::Value leds(Json::objectValue);
    int totalPixels = 0;
    int segCount = 0;
    for (const auto& name : PixelOverlayManager::INSTANCE.getModelNames()) {
        auto* m = PixelOverlayManager::INSTANCE.getModel(name);
        if (!m) continue;
        int w = m->getWidth();
        int h = m->getHeight();
        totalPixels += std::max(1, w) * std::max(1, h);
        ++segCount;
    }
    leds["count"]  = totalPixels ? totalPixels : 1;
    leds["pwr"]    = 0;
    leds["fps"]    = 40;
    leds["maxpwr"] = 0;
    leds["maxseg"] = segCount ? segCount : 1;
    leds["bootps"] = 0;
    leds["rgbw"]   = false;
    leds["wv"]     = 0;
    leds["cct"]    = false;
    leds["lc"]     = 1;
    info["leds"] = leds;

    info["fxcount"]  = static_cast<int>(PixelOverlayEffect::GetPixelOverlayEffects().size());
    info["palcount"] = 1;
    {
        std::string mac, ip;
        primaryNetIdentity(mac, ip);
        info["mac"] = mac;
        info["ip"]  = ip;
    }
    info["udpport"]  = 21324;
    info["live"]     = false;
    info["ws"]       = static_cast<int>(m_wsClients.size());
    info["str"]      = false;

    // The WLED Codable expects a `wifi` object even on wired devices.
    // Reporting a fake-good signal keeps clients from rendering us as
    // a degraded node; for an Ethernet-only FPP this is conventional.
    Json::Value wifi(Json::objectValue);
    wifi["bssid"]   = "00:00:00:00:00:00";
    wifi["rssi"]    = -50;
    wifi["signal"]  = 100;
    wifi["channel"] = 0;
    info["wifi"] = wifi;

    return SaveJsonToString(info, "");
}

std::string WLEDAPIResponder::BuildStateJson() {
    Json::Value state;
    Json::Value segs(Json::arrayValue);

    bool anyOn = false;
    int idx = 0;
    const auto& effects = PixelOverlayEffect::GetPixelOverlayEffects();
    for (const auto& name : PixelOverlayManager::INSTANCE.getModelNames()) {
        auto* m = PixelOverlayManager::INSTANCE.getModel(name);
        if (!m) continue;

        int w = m->getWidth();
        int h = m->getHeight();
        int len = std::max(1, w) * std::max(1, h);

        bool on = false;
        std::string fxName;
        {
            std::unique_lock<std::recursive_mutex> lock(m->getRunningEffectMutex());
            if (m->getRunningEffect()) {
                on = true;
                fxName = m->getRunningEffect()->name();
            }
        }
        if ((int)m->getState().getState() > 0) on = true;
        if (on) anyOn = true;

        int fxIdx = 0;
        if (!fxName.empty()) {
            int i = 0;
            for (const auto& en : effects) {
                if (en == fxName) { fxIdx = i; break; }
                ++i;
            }
        }

        Json::Value s;
        s["id"] = idx;
        s["start"] = 0;
        s["stop"] = len;
        s["len"] = len;
        s["grp"] = 1;
        s["spc"] = 0;
        s["of"] = 0;
        s["on"] = on;
        s["frz"] = false;
        s["bri"] = 255;
        s["cct"] = 127;
        s["set"] = 0;
        s["n"] = name;
        Json::Value cols(Json::arrayValue);
        for (int c = 0; c < 3; ++c) {
            Json::Value rgbw(Json::arrayValue);
            for (int b = 0; b < 4; ++b) rgbw.append(c == 0 && b < 3 ? 255 : 0);
            cols.append(rgbw);
        }
        s["col"] = cols;
        s["fx"] = fxIdx;
        s["sx"] = 128;
        s["ix"] = 128;
        s["pal"] = 0;
        s["sel"] = idx == 0;
        s["rev"] = false;
        s["mi"] = false;
        segs.append(s);
        ++idx;
    }
    if (segs.empty()) {
        Json::Value s;
        s["id"] = 0; s["start"] = 0; s["stop"] = 1; s["len"] = 1;
        s["on"] = false; s["bri"] = 255; s["fx"] = 0; s["sx"] = 128;
        s["ix"] = 128; s["pal"] = 0; s["sel"] = true;
        s["n"] = "Placeholder";
        segs.append(s);
    }

    state["on"] = anyOn;
    state["bri"] = 255;
    state["transition"] = 7;
    state["ps"] = -1;
    state["pl"] = -1;
    state["mainseg"] = 0;
    state["seg"] = segs;
    return SaveJsonToString(state, "");
}

std::string WLEDAPIResponder::BuildCombinedJson() {
    Json::Value root;
    Json::Reader r;
    Json::Value s, i;
    r.parse(BuildStateJson(), s);
    r.parse(BuildInfoJson(),  i);
    root["state"] = s;
    root["info"]  = i;
    return SaveJsonToString(root, "");
}

std::string WLEDAPIResponder::BuildEffectsJson() {
    Json::Value arr(Json::arrayValue);
    for (auto& n : PixelOverlayEffect::GetPixelOverlayEffects()) arr.append(n);
    return SaveJsonToString(arr, "");
}

void WLEDAPIResponder::ApplyStatePost(const std::string& jsonBody) {
    if (jsonBody.empty()) return;
    Json::Value root;
    Json::Reader rdr;
    if (!rdr.parse(jsonBody, root)) {
        LogDebug(VB_GENERAL, "WLEDAPIResponder: ignoring malformed JSON body\n");
        return;
    }

    // Snapshot the current model name list as a vector so we can index it.
    const auto& nameList = PixelOverlayManager::INSTANCE.getModelNames();
    std::vector<std::string> modelNames(nameList.begin(), nameList.end());

    auto applyOnOff = [&](const std::string& modelName, bool on) {
        auto* m = PixelOverlayManager::INSTANCE.getModel(modelName);
        if (!m) return;
        m->setState(PixelOverlayState(on ? PixelOverlayState::Enabled
                                          : PixelOverlayState::Disabled));
    };

    if (root.isMember("on") && !root.isMember("seg")) {
        bool on = root["on"].asBool();
        for (auto& n : modelNames) applyOnOff(n, on);
    }

    if (root.isMember("seg") && root["seg"].isArray()) {
        const auto& effList = PixelOverlayEffect::GetPixelOverlayEffects();
        std::vector<std::string> effects(effList.begin(), effList.end());

        for (auto& seg : root["seg"]) {
            int id = seg.get("id", 0).asInt();
            if (id < 0 || id >= (int)modelNames.size()) continue;
            const std::string& modelName = modelNames[id];

            if (seg.isMember("on")) applyOnOff(modelName, seg["on"].asBool());

            // Effect change via WLED `fx` index → look up name. We also
            // rebuild the args from the rest of the segment fields any
            // time a fx index, sx/ix/bri, or col[] is provided so live
            // slider drags update the running effect.
            bool hasParamUpdate = seg.isMember("fx") || seg.isMember("sx") ||
                                  seg.isMember("ix") || seg.isMember("bri") ||
                                  seg.isMember("col") || seg.isMember("pal");
            if (hasParamUpdate) {
                int fxIdx = seg.get("fx", 0).asInt();
                if (fxIdx < 0 || fxIdx >= (int)effects.size()) continue;
                auto* m = PixelOverlayManager::INSTANCE.getModel(modelName);
                auto* eff = PixelOverlayEffect::GetPixelOverlayEffect(effects[fxIdx]);
                if (!m || !eff) continue;

                std::vector<std::string> args = BuildEffectArgs(eff, seg);
                eff->apply(m, std::string("Enabled"), args);
            }
        }
    }

    NotifyStateChanged();
}

// Translate a WLED segment object into the std::vector<std::string>
// argument list that PixelOverlayEffect::apply() takes. We walk the
// effect's declared CommandArg list and fill each slot by matching the
// arg's name to the relevant WLED segment field. Unrecognized args fall
// back to the CommandArg's default value, which keeps WLED-ported and
// FPP-native effects equally happy.
std::vector<std::string> WLEDAPIResponder::BuildEffectArgs(PixelOverlayEffect* eff,
                                                            const Json::Value& seg) {
    std::vector<std::string> out;
    auto colHex = [&](int idx) -> std::string {
        if (!seg.isMember("col") || !seg["col"].isArray() || (int)seg["col"].size() <= idx) {
            return "";
        }
        const auto& c = seg["col"][idx];
        if (!c.isArray() || c.size() < 3) return "";
        char buf[16];
        snprintf(buf, sizeof(buf), "#%02X%02X%02X",
                 c[0].asInt() & 0xFF, c[1].asInt() & 0xFF, c[2].asInt() & 0xFF);
        return buf;
    };

    auto rangeScaled = [&](int wledVal, int min, int max) -> std::string {
        // WLED slider values are 0-255 (or 0-31 for c3). FPP effect args
        // declare their own min/max; if the FPP arg is also 0-255 we
        // pass through, otherwise we linearly rescale.
        if (max <= 0) return std::to_string(wledVal);
        if (max == 255) return std::to_string(wledVal);
        if (max == 31)  return std::to_string(wledVal & 0x1F);
        // e.g. Blink declares Speed range 1-5000.
        double f = (double)wledVal / 255.0;
        int v = min + (int)(f * (max - min));
        return std::to_string(v);
    };

    // RawWLEDEffect (every WLED-ported FX) declares an explicit role
    // per arg via getArgRoles(). Use that when available; fall back to
    // matching by arg name for the hand-written effects (Blink, Bars,
    // ColorFade, Text, and the FPP-native ones) whose arg names match
    // the WLED slider naming convention.
    auto roles = static_cast<WLEDEffect*>(eff)->getArgRoles();

    int idx = 0;
    for (const auto& a : eff->args) {
        WLEDArgRole role = WLEDArgRole::Unknown;
        if (idx < (int)roles.size()) role = roles[idx];

        if (role == WLEDArgRole::Unknown) {
            std::string lname = a.name;
            std::transform(lname.begin(), lname.end(), lname.begin(), ::tolower);
            if      (lname == "brightness")           role = WLEDArgRole::Brightness;
            else if (lname == "speed" || lname == "sx")     role = WLEDArgRole::Speed;
            else if (lname == "intensity" || lname == "ix") role = WLEDArgRole::Intensity;
            else if (lname == "color1")               role = WLEDArgRole::Color1;
            else if (lname == "color2")               role = WLEDArgRole::Color2;
            else if (lname == "color3")               role = WLEDArgRole::Color3;
            else if (lname == "palette")              role = WLEDArgRole::Palette;
        }

        std::string v = a.defaultValue;
        switch (role) {
        case WLEDArgRole::Brightness:
            v = rangeScaled(seg.get("bri", 255).asInt(), a.min, a.max);
            break;
        case WLEDArgRole::Speed:
            v = rangeScaled(seg.get("sx", 128).asInt(), a.min, a.max);
            break;
        case WLEDArgRole::Intensity:
            v = rangeScaled(seg.get("ix", 128).asInt(), a.min, a.max);
            break;
        case WLEDArgRole::Custom1:
            if (seg.isMember("c1")) v = rangeScaled(seg["c1"].asInt(), a.min, a.max);
            break;
        case WLEDArgRole::Custom2:
            if (seg.isMember("c2")) v = rangeScaled(seg["c2"].asInt(), a.min, a.max);
            break;
        case WLEDArgRole::Custom3:
            if (seg.isMember("c3")) v = rangeScaled(seg["c3"].asInt(), a.min, a.max);
            break;
        case WLEDArgRole::Check1:
            if (seg.isMember("o1")) v = seg["o1"].asBool() ? "1" : "0";
            break;
        case WLEDArgRole::Check2:
            if (seg.isMember("o2")) v = seg["o2"].asBool() ? "1" : "0";
            break;
        case WLEDArgRole::Check3:
            if (seg.isMember("o3")) v = seg["o3"].asBool() ? "1" : "0";
            break;
        case WLEDArgRole::Color1: { auto c = colHex(0); if (!c.empty()) v = c; break; }
        case WLEDArgRole::Color2: { auto c = colHex(1); if (!c.empty()) v = c; break; }
        case WLEDArgRole::Color3: { auto c = colHex(2); if (!c.empty()) v = c; break; }
        case WLEDArgRole::Palette:
            // WLED sends a numeric palette index; the WLED-ported effects
            // here want the palette name. Without a full palette catalog
            // round-trip we leave the default in place.
            break;
        default:
            break;
        }
        out.push_back(v);
        ++idx;
    }
    return out;
}
