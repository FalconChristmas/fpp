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

// The plugin HTTP route registry behind FPPPlugins::registerPluginApi(). Kept in
// its own translation unit so that fpphttp.h avoids pulling in the heavy
// <drogon/HttpAppFramework.h> header everywhere.
//
// Plugin routes are not drogon routes
// -----------------------------------
// Drogon's route table is frozen once app().run() starts: registering a handler
// after that asserts (and, in a build without asserts, mutates the router's maps
// unlocked under the I/O threads and never compiles regex routes). Plugins are
// loaded at runtime -- an install or reinstall does unload + load without
// restarting fppd -- so a plugin path that was not registered before run() can
// never become a drogon route.
//
// So no plugin path is ever handed to drogon. installPluginApiRouter() installs
// one drogon default handler before run(); drogon calls it for any request that
// none of FPP's own routes matched, and it looks the path up in the registry
// below on every request. Registering, re-arming and disarming a plugin path are
// then just registry edits, safe at any time.
//
// Safe plugin unregistration
// --------------------------
// A registered path owns a slot. While a plugin is live the slot names its
// handler and the call is dispatched. Once disarmed the path answers 410 Gone
// instead of crashing, and arming the slot again -- from a reinstalled or newly
// built copy of the plugin -- puts the same path back to work. Nothing about
// that depends on the plugin's .so still being the one that registered it.
//
// The handler is the plugin's own std::function: its code and captured state
// live in the plugin's .so, so disarming must *destroy* it, not merely forget
// it. See disarmSlots() for the ordering that makes that safe.
//
// The registry maps path string -> slot, so register and unregister calls, from
// any generation of a plugin, find the same slot. Slots are never erased, so a
// path that was ever registered keeps answering 410 rather than 404.

// HttpAppFramework.h must come before fpphttp.h: fpphttp.h undefines LOG_DEBUG
// (to avoid a conflict with FPP's log.h), but drogon's orm/Field.h uses LOG_DEBUG
// during its own compilation. Including the full framework header first lets all
// drogon headers compile with LOG_DEBUG intact; fpphttp.h then cleans it up.
#include <drogon/HttpAppFramework.h>
#include "fpphttp.h"
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Route registry
// ---------------------------------------------------------------------------

namespace {

// What one path dispatches to.
struct RouteSlot {
    // Guarded by s_registryMutex; held by shared_ptr so a dispatch can keep it
    // alive for the length of the call without blocking the registry.
    std::shared_ptr<FPPPlugins::PluginApiHandler> handler;
    std::vector<drogon::HttpMethod> methods;
    std::string path; // as registered, for listing; lookups use the normalized key
};
using SlotPtr = std::shared_ptr<RouteSlot>;

std::mutex s_registryMutex;
// Keyed by normalizedPath(). s_exact holds every registered path; s_family holds
// the family=true ones again, matching any subpath "<path>/...".
std::map<std::string, SlotPtr> s_exact;
std::map<std::string, SlotPtr> s_family;

// Held shared for the duration of a dispatch into plugin code, and exclusively
// by the disarm paths once the slot has been cleared.  Clearing the slot alone
// is not enough: a handler that already read it is about to call into the
// plugin, and an unload dlclose()s the .so right after unregistration returns --
// unmapping the code and vtables out from under the in-flight request.  Taking
// this exclusively makes disarming wait until no request is inside the plugin
// before it returns, so a subsequent dlclose() is safe.  (A handler that
// disarmed its own path from inside itself would self-deadlock, but nothing
// does that.)
std::shared_mutex s_dispatchMutex;

// Drogon matched routes case-insensitively and ignoring a trailing '/', and
// plugin paths were drogon routes before, so lookups keep those rules.
std::string normalizedPath(const std::string& path) {
    std::string p = path;
    for (auto& c : p) {
        c = (char)tolower((unsigned char)c);
    }
    while (p.size() > 1 && p.back() == '/') {
        p.pop_back();
    }
    return p;
}

void armSlot(std::map<std::string, SlotPtr>& table, const std::string& key, const std::string& path,
             const std::shared_ptr<FPPPlugins::PluginApiHandler>& fn,
             const std::vector<drogon::HttpMethod>& methods) {
    auto& slot = table[key];
    if (!slot) {
        slot = std::make_shared<RouteSlot>();
    }
    slot->handler = fn;
    slot->methods = methods;
    slot->path = path;
}

// The slot for a request path: an exact registration first, else the longest
// family registration it lies under. Caller holds s_registryMutex.
SlotPtr findSlot(const std::string& reqPath) {
    std::string p = normalizedPath(reqPath);
    auto it = s_exact.find(p);
    if (it != s_exact.end()) {
        return it->second;
    }
    for (;;) {
        auto slash = p.rfind('/');
        if (slash == std::string::npos || slash == 0) {
            return nullptr;
        }
        p.resize(slash);
        auto f = s_family.find(p);
        if (f != s_family.end()) {
            return f->second;
        }
    }
}

bool methodAllowed(const std::vector<drogon::HttpMethod>& methods, drogon::HttpMethod m) {
    for (auto a : methods) {
        // Drogon answered HEAD from a route's GET handler; keep doing that.
        if (a == m || (m == drogon::Head && a == drogon::Get)) {
            return true;
        }
    }
    return false;
}

// The drogon default handler: every request no FPP route matched lands here.
void dispatchPluginApi(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    // Declared first so it is released LAST: the handler copy below must be
    // gone before a concurrent disarm is allowed to proceed, otherwise the
    // last reference to the plugin's callable could be dropped -- running
    // its destructor -- after the .so was unmapped.
    std::shared_lock<std::shared_mutex> dispatchLock(s_dispatchMutex);

    std::shared_ptr<FPPPlugins::PluginApiHandler> fn;
    bool allowed = false;
    {
        std::lock_guard<std::mutex> lock(s_registryMutex);
        SlotPtr slot = findSlot(req->path());
        if (!slot) {
            dispatchLock.unlock();
            callback(drogon::HttpResponse::newNotFoundResponse());
            return;
        }
        fn = slot->handler;
        allowed = methodAllowed(slot->methods, req->method());
    }
    if (!fn) {
        callback(makeStringResponse("Plugin not loaded", 410, "text/plain"));
    } else if (!allowed) {
        callback(makeStringResponse("Method Not Allowed", 405, "text/plain"));
    } else {
        (*fn)(req, std::move(callback));
    }
}

// Clears both kinds of handler for the listed paths, then blocks until no
// request is inside plugin code, then destroys the plugin-owned callables.
// Destroy-then-return is the whole contract: the caller is entitled to dlclose()
// the moment this returns.
void disarmSlots(const std::string& path) {
    std::vector<std::shared_ptr<FPPPlugins::PluginApiHandler>> extracted;
    {
        std::lock_guard<std::mutex> lock(s_registryMutex);
        std::string key = normalizedPath(path);
        for (auto* table : { &s_exact, &s_family }) {
            auto it = table->find(key);
            if (it == table->end()) {
                continue;
            }
            // Clear first, so requests arriving now bail at the empty slot
            // rather than queueing behind the drain below.
            extracted.push_back(std::move(it->second->handler));
            it->second->handler.reset();
        }
    }
    std::unique_lock<std::shared_mutex> drain(s_dispatchMutex);
    // Nothing can be dispatching now, and these are the last references.
    extracted.clear();
}

} // namespace

namespace FPPPlugins {

void installPluginApiRouter() {
    drogon::app().setDefaultHandler(dispatchPluginApi);
}

std::vector<std::pair<std::string, drogon::HttpMethod>> listPluginApiRoutes() {
    std::vector<std::pair<std::string, drogon::HttpMethod>> routes;
    std::lock_guard<std::mutex> lock(s_registryMutex);
    for (auto* table : { &s_exact, &s_family }) {
        for (auto& [key, slot] : *table) {
            if (!slot->handler) {
                continue;
            }
            std::string path = slot->path;
            if (table == &s_family) {
                while (path.size() > 1 && path.back() == '/') {
                    path.pop_back();
                }
                path += "/.*";
            }
            for (auto m : slot->methods) {
                routes.emplace_back(path, m);
            }
        }
    }
    return routes;
}

void registerPluginApi(const std::string& path, PluginApiHandler handler,
                       const std::vector<drogon::HttpMethod>& methods, bool family) {
    auto fn = std::make_shared<PluginApiHandler>(std::move(handler));
    std::string key = normalizedPath(path);

    std::lock_guard<std::mutex> lock(s_registryMutex);
    armSlot(s_exact, key, path, fn, methods);
    if (family) {
        armSlot(s_family, key, path, fn, methods);
    }
}

void unregisterPluginApi(const std::string& path) {
    // Always disarm the family slot too, whether or not it was registered:
    // forgetting it there would leave a second live reference to the plugin's
    // handler, which is exactly the dangling pointer this API exists to prevent.
    disarmSlots(path);
}

} // namespace FPPPlugins
