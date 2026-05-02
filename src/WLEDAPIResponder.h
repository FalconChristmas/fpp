#pragma once
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

#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Minimal HTTP + WebSocket responder that mimics the surface a WLED node
// exposes, so the WLED iOS / Android apps treat an FPP device as a WLED
// node. Apache proxies /ws (and optionally /json/*) to this port via
// mod_proxy_wstunnel.
//
// This is intentionally a small, dependency-free server: it uses
// OpenSSL's SHA1 (already linked via libssl/libcrypto) and FPP's
// existing base64 helpers, and speaks just enough of RFC 6455 for the
// WLED apps' single text-frame request/response pattern.
class WLEDAPIResponder {
public:
    static WLEDAPIResponder INSTANCE;

    WLEDAPIResponder();
    ~WLEDAPIResponder();

    // Called from fppd startup (after PixelOverlayManager is up).
    // Opens the listening socket and spawns the accept thread.
    void Initialize();

    // Called from fppd shutdown. Closes the socket and joins threads.
    void Cleanup();

    // Notify connected WebSocket clients that overlay state changed.
    // Safe to call from any thread; messages are queued and sent from
    // the responder's own thread.
    void NotifyStateChanged();

    // Internal entry points for the worker threads (public so the
    // std::thread thunk can call them).
    void AcceptLoop();
    void ClientLoop(int fd);

private:
    bool HandleHTTPRequest(int fd, const std::string& reqLine,
                           const std::string& headers, const std::string& body);
    bool DoWebSocketHandshake(int fd, const std::string& key);
    void RunWebSocket(int fd);

    // WS framing helpers.
    bool SendTextFrame(int fd, const std::string& payload);
    bool SendCloseFrame(int fd);
    // Returns: 1 = text frame in `out`, 0 = control frame handled,
    //         -1 = client closed or error.
    int  ReadFrame(int fd, std::string& out);

    // WLED-compatible JSON builders.
    std::string BuildInfoJson();
    std::string BuildStateJson();
    std::string BuildCombinedJson();    // {state, info}
    std::string BuildEffectsJson();
    void        ApplyStatePost(const std::string& jsonBody);

    // Translate a WLED segment JSON into the args vector PixelOverlayEffect::apply expects.
    std::vector<std::string> BuildEffectArgs(class PixelOverlayEffect* eff,
                                             const class Json::Value& seg);

    // Port the WLED responder listens on. Apache proxies /ws to this.
    int m_port = 32327;
    int m_socket = -1;
    volatile bool m_running = false;

    std::thread* m_acceptThread = nullptr;

    std::mutex m_clientsLock;
    std::vector<int> m_wsClients;   // upgraded WebSocket fds (subset of m_clientFds)
    std::vector<int> m_clientFds;   // every accepted connection fd, so Cleanup() can wake each one
    std::vector<std::thread*> m_clientThreads;
};
