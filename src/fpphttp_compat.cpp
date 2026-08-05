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
// Safe plugin unregistration
// --------------------------
// Drogon has no route removal API. To allow plugins to be unloaded (and
// replaced) without leaving dangling function pointers in the router, the
// drogon route for a path is registered once, ever, and belongs to FPP. What it
// dispatches to is a slot owned by this file:
//
//   shared_ptr<RouteSlot> slot  <- shared by the drogon lambda and the registry
//
// The lambda re-reads the slot on every invocation. While a plugin is live the
// slot names its handler and the call is dispatched. Once disarmed the path
// answers 410 Gone instead of crashing, and arming the slot again -- from a
// reinstalled or newly built copy of the plugin -- puts the same route back to
// work. Nothing about that depends on the plugin's .so still being the one that
// registered it.
//
// The handler is the plugin's own std::function: its code and captured state
// live in the plugin's .so, so disarming must *destroy* it, not merely forget
// it. See disarmSlots() for the ordering that makes that safe.
//
// The registry maps path string -> slot, so register and unregister calls, from
// any generation of a plugin, find the same slot. Slots are never erased: the
// drogon route that captured one outlives every plugin.

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

// ---------------------------------------------------------------------------
// Route registry
// ---------------------------------------------------------------------------

namespace {

// What one path dispatches to. Owned by this file and never destroyed, because
// the drogon route that captured it cannot be unregistered.
struct RouteSlot {
    // Drogon-native API (FPPPlugins::registerPluginApi). Guarded by
    // s_registryMutex; held by shared_ptr so a dispatch can keep it alive for
    // the length of the call without blocking the registry.
    std::shared_ptr<FPPPlugins::PluginApiHandler> handler;
    // Whether the one-and-only drogon route for this path has been created.
    bool routeRegistered = false;
};
using SlotPtr = std::shared_ptr<RouteSlot>;

std::mutex s_registryMutex;
std::map<std::string, SlotPtr> s_registry;

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

// The regex key that family=true registrations use for subpaths of 'path'.
std::string familyKey(const std::string& path) {
    std::string base = path;
    if (!base.empty() && base.back() == '/')
        base.pop_back();
    return base + "/.*";
}

SlotPtr getOrCreateSlot(const std::string& path, bool* needsRoute = nullptr) {
    std::lock_guard<std::mutex> lock(s_registryMutex);
    auto& slot = s_registry[path];
    if (!slot) {
        slot = std::make_shared<RouteSlot>();
    }
    // Report (and latch) route creation under the same lock that publishes the
    // slot, so two plugins racing to register one path can't both create it.
    if (needsRoute) {
        *needsRoute = !slot->routeRegistered;
        slot->routeRegistered = true;
    }
    return slot;
}

// The one handler ever registered with drogon for a path. Re-reads the slot on
// every request, so it serves whichever plugin currently owns the path -- or
// answers 410 when nobody does.
using Dispatcher = std::function<void(const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&&)>;

Dispatcher makeDispatcher(const SlotPtr& slot) {
    return [slot](const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
        // Declared first so it is released LAST: the handler copy below must be
        // gone before a concurrent disarm is allowed to proceed, otherwise the
        // last reference to the plugin's callable could be dropped -- running
        // its destructor -- after the .so was unmapped.
        std::shared_lock<std::shared_mutex> dispatchLock(s_dispatchMutex);

        std::shared_ptr<FPPPlugins::PluginApiHandler> fn;
        {
            std::lock_guard<std::mutex> lock(s_registryMutex);
            fn = slot->handler;
        }
        if (fn) {
            (*fn)(req, std::move(callback));
            return;
        }
        callback(makeStringResponse("Plugin not loaded", 410, "text/plain"));
    };
}

// Creates the drogon route for a path if this is the first registration of it.
void ensureRoute(const std::string& path, const SlotPtr& slot, bool needsRoute,
                 const std::vector<drogon::HttpMethod>& methods, bool asRegex) {
    if (!needsRoute) {
        return; // already routed; the slot is what changed
    }
    // registerHandler takes constraints, not methods; HttpConstraint converts
    // implicitly from HttpMethod so the vector converts element-wise.
    std::vector<drogon::internal::HttpConstraint> constraints(methods.begin(), methods.end());
    auto& app = drogon::app();
    if (asRegex) {
        app.registerHandlerViaRegex(path, makeDispatcher(slot), constraints);
    } else {
        app.registerHandler(path, makeDispatcher(slot), constraints);
    }
}

// Clears both kinds of handler for the listed paths, then blocks until no
// request is inside plugin code, then destroys the plugin-owned callables.
// Destroy-then-return is the whole contract: the caller is entitled to dlclose()
// the moment this returns.
void disarmSlots(const std::vector<std::string>& paths) {
    std::vector<std::shared_ptr<FPPPlugins::PluginApiHandler>> extracted;
    {
        std::lock_guard<std::mutex> lock(s_registryMutex);
        for (const auto& p : paths) {
            auto it = s_registry.find(p);
            if (it == s_registry.end()) {
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

void registerPluginApi(const std::string& path, PluginApiHandler handler,
                       const std::vector<drogon::HttpMethod>& methods, bool family) {
    auto fn = std::make_shared<PluginApiHandler>(std::move(handler));

    bool needsRoute = false;
    SlotPtr slot = getOrCreateSlot(path, &needsRoute);
    {
        std::lock_guard<std::mutex> lock(s_registryMutex);
        slot->handler = fn;
    }
    ensureRoute(path, slot, needsRoute, methods, false);

    if (family) {
        std::string key = familyKey(path);
        bool subNeedsRoute = false;
        SlotPtr subSlot = getOrCreateSlot(key, &subNeedsRoute);
        {
            std::lock_guard<std::mutex> lock(s_registryMutex);
            subSlot->handler = fn;
        }
        ensureRoute(key, subSlot, subNeedsRoute, methods, true);
    }
}

void unregisterPluginApi(const std::string& path) {
    // Always disarm the family key too, whether or not it was registered:
    // forgetting it there would leave a second live reference to the plugin's
    // handler, which is exactly the dangling pointer this API exists to prevent.
    disarmSlots({ path, familyKey(path) });
}

} // namespace FPPPlugins
