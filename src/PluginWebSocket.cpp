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

// The registry behind FPPPlugins::registerPluginWebSocket().  See
// PluginWebSocket.h for the contract; this follows the route registry in
// fpphttp_compat.cpp, with one addition: a WebSocket outlives the request that
// opened it, so a slot also tracks the connections opened through it, and
// disarming closes them.
//
// The endpoint is chosen by the "name" query parameter rather than by path
// because the drogon on FPP OS images (Debian's 1.9.0) matches WebSocket routes
// by exact path only.

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpFilter.h>
#include <drogon/WebSocketController.h>
// trantor (drogon's net layer) defines LOG_* macros that collide with FPP's
// LogLevel enum; drop them before pulling in the FPP headers, same as httpAPI.cpp.
#undef LOG_WARN
#undef LOG_INFO
#undef LOG_DEBUG

#include "PluginWebSocket.h"
#include "fpphttp.h"
#include "log.h"

#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

using namespace drogon;

namespace {

struct WsSlot {
    // Guarded by s_mutex.
    std::shared_ptr<FPPPlugins::PluginWebSocketHandlers> handlers;
    std::string name; // as registered, for logging
};
using WsSlotPtr = std::shared_ptr<WsSlot>;

std::mutex s_mutex;
// Keyed by normalizedName().  Slots are never erased, so a name that was ever
// registered answers 410 rather than 404 once disarmed.
std::map<std::string, WsSlotPtr> s_slots;
// Every open connection that a plugin was handed, and the slot it came through.
// Disarming removes a slot's connections from here before closing them, so the
// close and any message still in flight on them find no entry and never reach
// whichever plugin build arms the slot next.
std::map<WebSocketConnectionPtr, WsSlotPtr> s_conns;

// Same role as s_dispatchMutex in fpphttp_compat.cpp: held shared for every
// call into plugin code, exclusively by disarm before it destroys the handlers.
std::shared_mutex s_dispatchMutex;

// Matched case-insensitively and ignoring leading and trailing '/', like
// plugin HTTP routes.
std::string normalizedName(const std::string& name) {
    size_t b = name.find_first_not_of('/');
    if (b == std::string::npos) {
        return "";
    }
    size_t e = name.find_last_not_of('/');
    std::string n = name.substr(b, e - b + 1);
    for (auto& c : n) {
        c = (char)tolower((unsigned char)c);
    }
    return n;
}

std::string requestName(const HttpRequestPtr& req) {
    return normalizedName(req->getParameter("name"));
}

// Runs a plugin handler; a throw must not reach drogon's event loop.
template <typename Fn>
void callPlugin(const char* what, const std::string& name, Fn&& fn) {
    try {
        fn();
    } catch (const std::exception& ex) {
        LogErr(VB_PLUGIN, "Plugin WebSocket %s handler for %s threw: %s\n", what, name.c_str(), ex.what());
    } catch (...) {
        LogErr(VB_PLUGIN, "Plugin WebSocket %s handler for %s threw a non-standard exception\n", what, name.c_str());
    }
}

// The slot an open connection came through, or null if it was never handed to
// a plugin or its slot has been disarmed since.  Caller holds s_mutex.
WsSlotPtr slotFor(const WebSocketConnectionPtr& conn) {
    auto it = s_conns.find(conn);
    return it == s_conns.end() ? nullptr : it->second;
}

} // namespace

// Refuses the handshake for a name no plugin has armed, so a client can tell
// "no such endpoint" (404, or 410 for a plugin that has since unloaded) from a
// connection that failed.  Arming can still change between this and
// handleNewConnection(); that is handled there.
class FPPPluginWebSocketFilter : public drogon::HttpFilter<FPPPluginWebSocketFilter> {
public:
    void doFilter(const HttpRequestPtr& req, FilterCallback&& fcb, FilterChainCallback&& fccb) override {
        int code = 404;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            auto it = s_slots.find(requestName(req));
            if (it != s_slots.end()) {
                code = it->second->handlers ? 0 : 410;
            }
        }
        if (code == 0) {
            fccb();
        } else {
            fcb(makeStringResponse(code == 404 ? "Not Found" : "Plugin not loaded", code, "text/plain"));
        }
    }
};

// The one drogon route for every plugin WebSocket.
class FPPPluginWebSocket : public drogon::WebSocketController<FPPPluginWebSocket> {
public:
    void handleNewConnection(const HttpRequestPtr& req, const WebSocketConnectionPtr& conn) override {
        // Taken before the slot is read and released last, as in
        // dispatchPluginApi(): a disarm must not destroy the handlers while
        // this is about to call them.
        std::shared_lock<std::shared_mutex> dispatchLock(s_dispatchMutex);
        std::shared_ptr<FPPPlugins::PluginWebSocketHandlers> h;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            auto it = s_slots.find(requestName(req));
            if (it != s_slots.end() && it->second->handlers) {
                h = it->second->handlers;
                s_conns[conn] = it->second;
            }
        }
        if (!h) {
            // Disarmed after the filter let the handshake through.
            conn->shutdown(CloseCode::kEndpointGone, "Plugin not loaded");
            return;
        }
        if (h->onConnect) {
            callPlugin("connect", req->getParameter("name"), [&] { h->onConnect(req, conn); });
        }
    }

    void handleNewMessage(const WebSocketConnectionPtr& conn, std::string&& message,
                          const WebSocketMessageType& type) override {
        std::shared_lock<std::shared_mutex> dispatchLock(s_dispatchMutex);
        std::shared_ptr<FPPPlugins::PluginWebSocketHandlers> h;
        std::string name;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (WsSlotPtr slot = slotFor(conn)) {
                h = slot->handlers;
                name = slot->name;
            }
        }
        if (h && h->onMessage) {
            callPlugin("message", name, [&] { h->onMessage(conn, std::move(message), type); });
        }
    }

    void handleConnectionClosed(const WebSocketConnectionPtr& conn) override {
        std::shared_lock<std::shared_mutex> dispatchLock(s_dispatchMutex);
        std::shared_ptr<FPPPlugins::PluginWebSocketHandlers> h;
        std::string name;
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (WsSlotPtr slot = slotFor(conn)) {
                h = slot->handlers;
                name = slot->name;
            }
            s_conns.erase(conn);
        }
        if (h && h->onClose) {
            callPlugin("close", name, [&] { h->onClose(conn); });
        }
    }

    // drogon creates the filter by name and nothing here constructs one, so
    // the class template's static registrar is only instantiated -- and the
    // filter only found -- because classTypeName() uses it.  A string literal
    // name compiles fine and leaves the route unfiltered.
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/fppdws/plugin", FPPPluginWebSocketFilter::classTypeName());
    WS_PATH_LIST_END
};

namespace FPPPlugins {

bool registerPluginWebSocket(const std::string& name, PluginWebSocketHandlers handlers) {
    std::string key = normalizedName(name);
    if (key.empty()) {
        LogErr(VB_PLUGIN, "Plugin WebSocket registered with an empty name\n");
        return false;
    }
    auto h = std::make_shared<PluginWebSocketHandlers>(std::move(handlers));
    std::lock_guard<std::mutex> lock(s_mutex);
    auto& slot = s_slots[key];
    if (!slot) {
        slot = std::make_shared<WsSlot>();
    }
    slot->handlers = h;
    slot->name = name;
    return true;
}

void unregisterPluginWebSocket(const std::string& name) {
    std::shared_ptr<PluginWebSocketHandlers> extracted;
    std::vector<WebSocketConnectionPtr> conns;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_slots.find(normalizedName(name));
        if (it == s_slots.end()) {
            return;
        }
        // Clear and detach in one step: a connection arriving now finds the
        // slot empty, and none of this slot's connections can reach a handler.
        extracted = std::move(it->second->handlers);
        it->second->handlers.reset();
        for (auto c = s_conns.begin(); c != s_conns.end();) {
            if (c->second == it->second) {
                conns.push_back(c->first);
                c = s_conns.erase(c);
            } else {
                ++c;
            }
        }
    }
    {
        std::unique_lock<std::shared_mutex> drain(s_dispatchMutex);
        // Nothing can be dispatching now, and this is the last reference.
        extracted.reset();
    }
    for (auto& c : conns) {
        c->shutdown(CloseCode::kEndpointGone, "Plugin unloaded");
    }
}

} // namespace FPPPlugins
