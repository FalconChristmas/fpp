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

// Implementation of the httpserver::webserver compatibility shim.
// Kept in its own translation unit so that fpphttp.h avoids pulling in
// the heavy <drogon/HttpAppFramework.h> header everywhere.
//
// Safe plugin unregistration
// --------------------------
// Drogon has no route removal API. To allow plugins to be safely unloaded
// without leaving dangling function pointers in the router, every handler
// registered through this shim captures a shared_ptr to an atomic raw
// pointer rather than the raw pointer itself:
//
//   shared_ptr<atomic<http_resource*>> slot  ← shared by lambda + registry
//
// The drogon lambda checks the slot on every invocation. While the plugin
// is live, slot->load() returns the resource and the call is dispatched
// normally. When the plugin calls unregister_resource(), the slot is
// zeroed (slot->store(nullptr)). Any subsequent request to that path gets
// a 410 Gone response instead of a crash.
//
// The registry maps path string → slot so that the separate register and
// unregister calls (each with their own stack-allocated webserver object)
// can share the same slot.

// HttpAppFramework.h must come before fpphttp.h: fpphttp.h undefines LOG_DEBUG
// (to avoid a conflict with FPP's log.h), but drogon's orm/Field.h uses LOG_DEBUG
// during its own compilation. Including the full framework header first lets all
// drogon headers compile with LOG_DEBUG intact; fpphttp.h then cleans it up.
#include <drogon/HttpAppFramework.h>
#include "fpphttp.h"
#include "Warnings.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace httpserver {

// ---------------------------------------------------------------------------
// Route registry
// ---------------------------------------------------------------------------

using ResourceSlot = std::shared_ptr<std::atomic<http_resource*>>;

static std::mutex             s_registryMutex;
static std::map<std::string, ResourceSlot> s_registry;

// Held shared for the duration of a handler's render_*() call, and exclusively
// by clearSlot() once the slot has been nulled.  Nulling the slot alone is not
// enough: a handler that already loaded the pointer is about to make a virtual
// call into the plugin, and PluginManager::Cleanup() dlclose()s the .so right
// after unregisterApis() returns -- unmapping the vtable out from under the
// in-flight request.  Taking this exclusively makes unregister_resource() wait
// until no handler is inside the plugin before it returns, so the subsequent
// dlclose() is safe.  (A plugin that unregistered its own route from inside
// its own render_*() would self-deadlock, but nothing does that.)
static std::shared_mutex      s_dispatchMutex;

static ResourceSlot getOrCreateSlot(const std::string& path) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    auto it = s_registry.find(path);
    if (it != s_registry.end())
        return it->second;
    auto slot = std::make_shared<std::atomic<http_resource*>>(nullptr);
    s_registry[path] = slot;
    return slot;
}

static void clearSlot(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(s_registryMutex);
        auto it = s_registry.find(path);
        if (it != s_registry.end())
            it->second->store(nullptr);
    }
    // Null first (so newly arriving requests bail at the null check rather than
    // queueing behind us), then drain: this blocks until every handler already
    // inside a render_*() call has returned.  See s_dispatchMutex.
    std::unique_lock<std::shared_mutex> drain(s_dispatchMutex);
}

// ---------------------------------------------------------------------------
// webserver::register_resource
// ---------------------------------------------------------------------------

void webserver::register_resource(const std::string& path, http_resource* resource,
                                  bool family) {
    // Obtain (or create) the indirection slot for this path and arm it.
    ResourceSlot slot = getOrCreateSlot(path);
    slot->store(resource);

    std::string who = pluginName.empty() ? "Unknown plugin" : "Plugin '" + pluginName + "'";
    WarningHolder::AddWarning(who + " registered HTTP route '" + path +
                              "' using the deprecated libhttpserver API. "
                              "Recompile the plugin against the new fpphttp.h drogon-based API.");

    // The lambda captures the slot by value (shared_ptr copy). The raw
    // resource pointer is read atomically on every invocation so that a
    // concurrent unregister_resource() call is safe.
    auto handler = [slot](const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback) {
        // Keeps the plugin mapped for the duration of the call - see s_dispatchMutex.
        std::shared_lock<std::shared_mutex> dispatchLock(s_dispatchMutex);
        http_resource* res = slot->load();
        if (!res) {
            callback(makeStringResponse("Plugin not loaded", 410, "text/plain"));
            return;
        }
        http_request wrapped(req);
        std::shared_ptr<http_response> resp;
        switch (req->method()) {
        case drogon::Get:    resp = res->render_GET(wrapped);    break;
        case drogon::Post:   resp = res->render_POST(wrapped);   break;
        case drogon::Put:    resp = res->render_PUT(wrapped);    break;
        case drogon::Delete: resp = res->render_DELETE(wrapped); break;
        case drogon::Head:   resp = res->render_HEAD(wrapped);   break;
        default:
            resp = std::make_shared<string_response>("Method Not Allowed", 405);
        }
        callback(resp->toDrogon());
    };

    auto& app = drogon::app();
    app.registerHandler(path, handler,
                        { drogon::Get, drogon::Post, drogon::Put,
                          drogon::Delete, drogon::Head });
    if (family) {
        std::string regexPath = path;
        if (!regexPath.empty() && regexPath.back() == '/')
            regexPath.pop_back();
        // Also grab/arm a slot for the subpath regex key.
        std::string regexKey = regexPath + "/.*";
        ResourceSlot subSlot = getOrCreateSlot(regexKey);
        subSlot->store(resource);

        auto subHandler = [subSlot](const HttpRequestPtr& req,
                                    std::function<void(const HttpResponsePtr&)>&& callback) {
            // Keeps the plugin mapped for the duration of the call - see s_dispatchMutex.
            std::shared_lock<std::shared_mutex> dispatchLock(s_dispatchMutex);
            http_resource* res = subSlot->load();
            if (!res) {
                callback(makeStringResponse("Plugin not loaded", 410, "text/plain"));
                return;
            }
            http_request wrapped(req);
            std::shared_ptr<http_response> resp;
            switch (req->method()) {
            case drogon::Get:    resp = res->render_GET(wrapped);    break;
            case drogon::Post:   resp = res->render_POST(wrapped);   break;
            case drogon::Put:    resp = res->render_PUT(wrapped);    break;
            case drogon::Delete: resp = res->render_DELETE(wrapped); break;
            case drogon::Head:   resp = res->render_HEAD(wrapped);   break;
            default:
                resp = std::make_shared<string_response>("Method Not Allowed", 405);
            }
            callback(resp->toDrogon());
        };
        app.registerHandlerViaRegex(regexKey, subHandler,
                                    { drogon::Get, drogon::Post, drogon::Put,
                                      drogon::Delete, drogon::Head });
    }
}

// ---------------------------------------------------------------------------
// webserver::unregister_resource
// ---------------------------------------------------------------------------

void webserver::unregister_resource(const std::string& path, http_resource*) {
    clearSlot(path);
}

void webserver::unregister_resource(const std::string& path) {
    clearSlot(path);
}

} // namespace httpserver
