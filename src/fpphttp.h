#pragma once
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

// HTTP helpers for FPP and for plugins, on top of Drogon.
// Only include the minimal drogon headers to avoid DrObject auto-registration
// in translation units that don't need the full framework.

// HttpRequestPtr/HttpResponsePtr/HttpCallback aliases live there so that
// declaration-only headers can use them without the heavy drogon include below.
// See fpphttp_types.h.
#include "fpphttp_types.h"

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

// Trantor (drogon dependency) defines these as macros which conflict
// with FPP's LogLevel enum values in log.h. Undefine them here.
#ifdef LOG_WARN
#undef LOG_WARN
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// HttpRequestPtr / HttpResponsePtr / HttpCallback are defined in fpphttp_types.h
// (included above), so they are also available to lightweight headers.

// Helper to split a URL path into pieces (equivalent to libhttpserver's get_path_pieces())
inline std::vector<std::string> getPathPieces(const std::string& path) {
    std::vector<std::string> pieces;
    std::string piece;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '/') {
            if (!piece.empty()) {
                pieces.push_back(piece);
                piece.clear();
            }
        } else {
            piece += path[i];
        }
    }
    if (!piece.empty()) {
        pieces.push_back(piece);
    }
    return pieces;
}

// Helper to create a string response (equivalent to httpserver::string_response)
inline HttpResponsePtr makeStringResponse(const std::string& body, int statusCode = 200,
                                           const std::string& contentType = "text/plain") {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(static_cast<drogon::HttpStatusCode>(statusCode));
    resp->setBody(body);
    if (contentType == "application/json") {
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    } else {
        resp->setContentTypeCodeAndCustomString(drogon::CT_CUSTOM, contentType);
    }
    return resp;
}

// Build a response that carries a content-derived ETag, answering 304 when the
// client already holds this version.
//
// For a large, rarely-changing API result this is the difference between
// sending the body on every page load and sending nothing at all: the overlay
// effect list alone is ~350KB, and a client that goes through FPPMon's MQTT
// proxy pays for those bytes over someone's internet connection. The tag is a
// plain FNV-1a over the bytes, which is a validator rather than a digest -- it
// only has to change when the content does.
inline HttpResponsePtr makeETagResponse(const HttpRequestPtr& req, const std::string& body,
                                        const std::string& contentType = "application/json") {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned char c : body) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%llx-%llx", (unsigned long long)body.size(), (unsigned long long)h);
    std::string bare(buf);
    std::string etag = "\"" + bare + "\"";

    // If-None-Match may carry a list, and a cache that compressed the response
    // on the way past can append a suffix to the tag it echoes back
    // (`"...-gzip"`), so match on the bare tag appearing anywhere rather than
    // on string equality.
    std::string inm = req->getHeader("if-none-match");
    if (!inm.empty() && inm.find(bare) != std::string::npos) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k304NotModified);
        resp->addHeader("ETag", etag);
        return resp;
    }
    auto resp = makeStringResponse(body, 200, contentType);
    resp->addHeader("ETag", etag);
    return resp;
}

// Helper to get a request parameter (query string or form parameter)
// Equivalent to libhttpserver's req.get_arg("key")
inline std::string getRequestArg(const HttpRequestPtr& req, const std::string& key) {
    return req->getParameter(key);
}

// Helper to get request body as string
// Equivalent to libhttpserver's req.get_content()
inline std::string getRequestContent(const HttpRequestPtr& req) {
    return std::string(req->body());
}

// Helper to get query string
// Equivalent to libhttpserver's req.get_querystring()
inline std::string getQueryString(const HttpRequestPtr& req) {
    return req->query();
}

// ---- Plugin HTTP API registration -----------------------------------------
//
// Plugins must register their HTTP routes through these rather than calling
// drogon::app().registerHandler() directly, because drogon has no route removal
// API. A handler registered straight with drogon is a function pointer into the
// plugin's .so that can never be withdrawn, which makes the plugin impossible to
// unload -- and impossible to upgrade in place, since a newly loaded build
// cannot take over a path the retired one still owns.
//
// What FPP does instead: the drogon route is registered once, ever, per path,
// and belongs to FPP. It dispatches through a slot that lives here in libfpp,
// not in any plugin. registerPluginApi() arms that slot; unregisterPluginApi()
// disarms it and requests to the path answer 410 Gone. A later
// registerPluginApi() for the same path re-arms the same slot, so a replacement
// build of the plugin picks the route straight back up.
//
// unregisterPluginApi() does not return until no request is executing inside the
// handler AND the handler object itself has been destroyed -- the plugin's own
// callable, whose code and captured state live in the .so. That is the ordering
// that makes a subsequent dlclose() safe: destroy while still mapped, then
// unmap. (A handler that unregistered its own path from inside itself would
// deadlock. Nothing should do that.)
//
// This covers inbound HTTP only. A handler that hands work to another thread is
// responsible for that thread itself, in FPPPlugins::Plugin::shutdown().
namespace FPPPlugins {

// Same shape drogon's registerHandler() takes.
using PluginApiHandler = std::function<void(const HttpRequestPtr&, HttpCallback&&)>;

// Registers (or re-arms) 'path'. With family=true, subpaths of 'path' route to
// the same handler as well.
void registerPluginApi(const std::string& path, PluginApiHandler handler,
                       const std::vector<drogon::HttpMethod>& methods = { drogon::Get, drogon::Post, drogon::Put,
                                                                          drogon::Delete, drogon::Head },
                       bool family = false);

// Disarms 'path' (and its family subpaths, if any were registered) and destroys
// the handler before returning. Safe to call for a path that was never
// registered, and safe to call twice.
void unregisterPluginApi(const std::string& path);

} // namespace FPPPlugins

