#pragma once
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

// WebSocket endpoints for plugins, the push counterpart of registerPluginApi()
// in fpphttp.h.  New in FPP 10.4; see docs/Plugin_Development.md.
//
// A plugin endpoint registered as "<name>" is reached at
// ws://<host>/fppdws/plugin?name=<name>; any other query parameters are the
// plugin's to read.  Name it after the plugin, e.g. "FPPMon/status".  Apache
// already proxies /fppdws (and everything under it) to fppd as a WebSocket, so a
// plugin endpoint needs no web server configuration and works on any box whose
// FPP provides these functions.
//
// Drogon cannot add a route once it is running and has no way to remove one, so
// plugins must not register WebSocketControllers of their own.  FPP owns the one
// /fppdws/plugin route and dispatches each connection to the handlers registered
// here for its name.  The same slot rules as registerPluginApi() apply:
// unregistering disarms the name, waits for any call into the plugin's handlers
// to finish, closes every connection opened through it and destroys the
// handlers before it returns, so the plugin can be unloaded right afterwards.  A
// handshake for a name nobody registered is refused with 404, and one for a name
// whose plugin has unregistered with 410.
//
// The handlers run on drogon I/O threads.  The plugin owns the connections it is
// handed: it may keep them, send to them from any thread and close them, but it
// must stop doing so (and drop them) in its own shutdown().  onClose is called
// for connections the client or the plugin closed; connections closed by
// unregisterPluginWebSocket() are closed after the handlers are gone and are
// not reported.
//
// These declarations are weak, so a plugin built against this header still loads
// on an fppd that does not have them.  Check pluginWebSocketsSupported() before
// calling either function.

#include <drogon/WebSocketConnection.h>

// After drogon: clears trantor's LOG_* macros, and gives handlers the full
// HttpRequest.
#include "fpphttp.h"

#include <functional>
#include <string>

// Lets plugin source build against older FPP headers as well.
#define FPP_HAS_PLUGIN_WEBSOCKET 1

namespace FPPPlugins {

struct PluginWebSocketHandlers {
    // A client connected.  Any of the three may be left empty.
    std::function<void(const HttpRequestPtr&, const drogon::WebSocketConnectionPtr&)> onConnect;
    std::function<void(const drogon::WebSocketConnectionPtr&, std::string&&, const drogon::WebSocketMessageType&)> onMessage;
    std::function<void(const drogon::WebSocketConnectionPtr&)> onClose;
};

// Registers (or re-arms) 'name', matched case-insensitively and ignoring
// leading and trailing '/'.  Returns false, and registers nothing, for an empty
// name.
__attribute__((weak)) bool registerPluginWebSocket(const std::string& name, PluginWebSocketHandlers handlers);

// Disarms 'name' as described above.  Safe to call for a name that was never
// registered, and safe to call twice.
__attribute__((weak)) void unregisterPluginWebSocket(const std::string& name);

inline bool pluginWebSocketsSupported() {
    return &registerPluginWebSocket != nullptr && &unregisterPluginWebSocket != nullptr;
}

} // namespace FPPPlugins
