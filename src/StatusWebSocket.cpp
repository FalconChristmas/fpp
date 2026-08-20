/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

// WebSocket push of the fppd status.
//
// Historically every open web page polled api/system/status on a timer
// (default 5s).  That PHP endpoint fetches /fppd/status from fppd and then
// augments it with slow-moving host data (wifi, interfaces, advancedView,
// plugin indicators, ...).  On a BeagleBone that augmentation is ~180ms of
// PHP per request; multiplied by every open tab every few seconds it is a
// meaningful, continuous load on the exact devices that can least afford it.
//
// This endpoint splits the fast-moving half off: fppd pushes its own
// /fppd/status payload over a WebSocket the instant it changes (and at most
// once per second while a playlist's clock ticks), so pages no longer poll
// for it at all.  The slow host augmentation stays in PHP but can be polled
// far less often.  fppd already has this data in hand, so producing it costs
// ~0.7ms and nothing when nobody is connected.
//
// Wire format (server -> client), mirroring a versioned-snapshot scheme:
//     {"type":"snapshot","v":{"status":<n>},"data":{"status":{...}}}
// `v` is a per-key monotonic version map; `data` carries only the keys that
// changed this round.  A client compares versions to detect a key it missed
// across a reconnect.  Today the only key is "status"; the envelope leaves
// room to add more (e.g. a slow "system" key) later without a protocol change.
//
// Client -> server: only "pong" is expected (drogon answers pings itself);
// anything else is ignored.  The endpoint is read-only — it never accepts
// commands — so proxying it to the LAN carries no more authority than the
// existing status poll did.

#include <drogon/HttpAppFramework.h>
#include <drogon/WebSocketController.h>
// trantor (drogon's net layer) defines LOG_* macros that collide with FPP's
// LogLevel enum; drop them before pulling in the FPP headers, same as httpAPI.cpp.
#undef LOG_WARN
#undef LOG_INFO
#undef LOG_DEBUG

#include "fpp-pch.h"

#include "StatusWebSocket.h"
#include "Warnings.h"
#include "common.h" // GetCurrentFPPDStatus, SaveJsonToString

#include <mutex>
#include <set>
#include <string>
#include <vector>

using namespace drogon;

namespace {

std::mutex g_mutex;
std::set<WebSocketConnectionPtr> g_conns;
uint64_t g_statusVersion = 0;
std::string g_lastStatusJson; // last pushed status payload, for change detection
// Set by StatusWebSocketShutdown() under g_mutex. Once set, no producer builds
// or sends a payload and no new connection is tracked, so a connection that
// arrives while fppd is tearing down can't re-populate g_conns after the clear.
bool g_shutdown = false;

// Build the current /fppd/status payload as a compact JSON string.  This is
// the identical data the GET /fppd/status handler returns.
std::string buildStatusJson() {
    Json::Value status;
    GetCurrentFPPDStatus(status);
    return SaveJsonToString(status, ""); // "" indentation == compact
}

// Wrap an already-serialized data payload for one key in the snapshot
// envelope.  dataJson is spliced in verbatim so we don't parse-then-reserialize.
std::string makeSnapshot(const char* key, const std::string& dataJson, uint64_t version) {
    std::string out;
    out.reserve(dataJson.size() + 64);
    out += "{\"type\":\"snapshot\",\"v\":{\"";
    out += key;
    out += "\":";
    out += std::to_string(version);
    out += "},\"data\":{\"";
    out += key;
    out += "\":";
    out += dataJson;
    out += "}}";
    return out;
}

// Produce the current status, and if it differs from what was last sent (or a
// new client needs it), push it to all connected clients.  Cheap and a no-op
// when nobody is connected, so the 1s timer costs nothing on an idle system.
void broadcastStatusIfChanged() {
    // Check for connections before doing any work: with nobody listening the
    // 1s timer must cost nothing, so an idle system pays no price for this
    // endpoint existing.  (A client can appear between this check and the
    // build below; it gets its own snapshot in handleNewConnection, so a
    // missed broadcast here is harmless.)
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_shutdown || g_conns.empty())
            return;
    }
    std::string js = buildStatusJson();
    std::string msg;
    std::vector<WebSocketConnectionPtr> targets;
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_shutdown || g_conns.empty())
            return;
        if (js == g_lastStatusJson)
            return;
        g_lastStatusJson = js;
        ++g_statusVersion;
        msg = makeSnapshot("status", js, g_statusVersion);
        targets.assign(g_conns.begin(), g_conns.end());
    }
    for (auto& c : targets) {
        if (c->connected())
            c->send(msg);
    }
}

// Warnings are part of the /fppd/status payload, so a new or cleared warning
// changes that payload.  Rather than wait up to a second for the timer, push
// immediately when WarningHolder notifies us.
class StatusWarningListener : public WarningListener {
public:
    void handleWarnings(const std::list<FPPWarning>& /*warnings*/) override {
        broadcastStatusIfChanged();
    }
};
StatusWarningListener* g_warningListener = nullptr;

} // namespace

// WebSocket controller bound to /fppdws.  Instances are thin: all shared state
// lives in the file-static structures above so the timer and warning callbacks
// (which have no controller instance) can reach it too.
class StatusWebSocket : public drogon::WebSocketController<StatusWebSocket> {
public:
    void handleNewConnection(const HttpRequestPtr& /*req*/,
                             const WebSocketConnectionPtr& conn) override {
        {
            // A connection can still land after shutdown has begun (drogon's
            // quit() only queues teardown). Building a payload then would read
            // producers that are being destroyed, so answer nothing at all.
            std::lock_guard<std::mutex> lk(g_mutex);
            if (g_shutdown)
                return;
        }
        // Send this client a full snapshot immediately so it doesn't have to
        // wait for the next change.  Force a build (bypass the change check).
        std::string js = buildStatusJson();
        std::string msg;
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (g_shutdown)
                return;
            g_conns.insert(conn);
            // Advance the shared cache/version so the timer doesn't
            // immediately re-broadcast identical data to everyone.
            if (js != g_lastStatusJson) {
                g_lastStatusJson = js;
                ++g_statusVersion;
            }
            msg = makeSnapshot("status", js, g_statusVersion);
        }
        conn->send(msg);
    }

    void handleNewMessage(const WebSocketConnectionPtr& /*conn*/,
                          std::string&& /*message*/,
                          const WebSocketMessageType& /*type*/) override {
        // Read-only endpoint: ignore anything the client sends.  drogon
        // handles ping/pong framing internally.
    }

    void handleConnectionClosed(const WebSocketConnectionPtr& conn) override {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_conns.erase(conn);
    }

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/fppdws");
    WS_PATH_LIST_END
};

void StatusWebSocketInit() {
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    if (!g_warningListener) {
        g_warningListener = new StatusWarningListener();
        WarningHolder::AddWarningListener(g_warningListener);
    }

    // A once-per-second tick on drogon's own event loop -- independent of the
    // fppd main loop, which is epoll-gated and doesn't tick reliably during
    // playback.  broadcastStatusIfChanged() is a no-op with no clients, so
    // this is effectively free until a page connects.
    drogon::app().getLoop()->runEvery(1.0, []() { broadcastStatusIfChanged(); });
}

void StatusWebSocketShutdown() {
    static bool shutdownDone = false;
    if (shutdownDone)
        return;
    shutdownDone = true;

    // Order matters here. Removing the listener first is what makes the rest
    // safe: WarningHolder holds its listener lock across the whole notify pass,
    // so once RemoveWarningListener() returns, no handleWarnings() call on our
    // listener is running or can start, and the warning thread can no longer
    // drive a status build. The listener object itself is deliberately leaked --
    // the notify thread is the only user and it is done with it by now, and a
    // delete buys nothing on a process that _exit()s.
    if (g_warningListener) {
        WarningHolder::RemoveWarningListener(g_warningListener);
    }

    // The remaining producer is the drogon timer, which is still ticking (this
    // runs before drogon is stopped). g_shutdown stops it building, and clearing
    // g_conns drops the connection references drogon's quit() is not guaranteed
    // to release through the close callbacks.
    std::lock_guard<std::mutex> lk(g_mutex);
    g_shutdown = true;
    g_conns.clear();
}
