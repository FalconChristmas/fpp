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
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <unistd.h>
#include <utility>
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

// ---- ETags -----------------------------------------------------------------
//
// FNV-1a, used to turn either a response body or a caller-supplied version
// string into a compact validator. This is a validator rather than a digest --
// it only has to change when the content does, and nothing depends on it being
// hard to forge.
inline uint64_t fppETagHash(const std::string& s, uint64_t h = 0xcbf29ce484222325ULL) {
    for (unsigned char c : s) {
        h ^= c;
        h *= 0x100000001b3ULL;
    }
    return h;
}

// True when the client's If-None-Match carries `bare`.
//
// If-None-Match may hold a list, and a cache that compressed the response on
// the way past can append a suffix to the tag it echoes back (`"...-gzip"`), so
// match on the bare tag appearing anywhere rather than on string equality.
inline bool fppETagPresent(const HttpRequestPtr& req, const std::string& bare) {
    std::string inm = req->getHeader("if-none-match");
    return !inm.empty() && inm.find(bare) != std::string::npos;
}

inline HttpResponsePtr makeNotModifiedResponse(const std::string& bare) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k304NotModified);
    resp->addHeader("ETag", "\"" + bare + "\"");
    return resp;
}

// Build a response that carries a content-derived ETag, answering 304 when the
// client already holds this version.
//
// For a large, rarely-changing API result this is the difference between
// sending the body on every page load and sending nothing at all: the overlay
// effect list alone is ~350KB, and a client that goes through FPPMon's MQTT
// proxy pays for those bytes over someone's internet connection.
//
// This form still builds the body before it can hash it, so it saves transfer
// and compression but not generation. When the handler can name what it is
// about to serialize more cheaply than serializing it, prefer the versioned
// form below.
inline HttpResponsePtr makeETagResponse(const HttpRequestPtr& req, const std::string& body,
                                        const std::string& contentType = "application/json") {
    char buf[64];
    snprintf(buf, sizeof(buf), "%llx-%llx", (unsigned long long)body.size(),
             (unsigned long long)fppETagHash(body));
    std::string bare(buf);

    if (fppETagPresent(req, bare)) {
        return makeNotModifiedResponse(bare);
    }
    auto resp = makeStringResponse(body, 200, contentType);
    resp->addHeader("ETag", "\"" + bare + "\"");
    return resp;
}

// ---- Versioned ETags -------------------------------------------------------
//
// The content hash above cannot answer a conditional request without first
// building the answer, and on the slow boards building is the expensive half:
// /api/overlays/effects?full=true takes 0.9-3.4s of a PocketBeagle2 to
// serialize but only ~10KB to send once gzipped. A handler that can name its
// own version -- a counter bumped when the underlying data changes, a file
// mtime -- can answer 304 without doing that work at all.
//
// `version` need only distinguish one state of the data from the next; it is
// never parsed by the client. Two things are folded in for you:
//
//  - the request path and query string, so a handler whose output varies on
//    parameters (?simple=true, ?full=true) cannot serve one variant's body
//    under another variant's tag by using a single counter for all of them;
//  - a per-process salt, because an in-memory counter restarts at zero. Without
//    it a client holding a tag from before an fppd restart would be told 304
//    against a *different* command or model set that happens to be at the same
//    count. The cost is that a restart invalidates these tags, which is the
//    right way round: a false 304 serves stale content, a false 200 only costs
//    bytes.
inline std::string makeETagToken(const HttpRequestPtr& req, const std::string& version) {
    static const uint64_t salt = fppETagHash(std::to_string((unsigned long long)::time(nullptr)) + "-" + std::to_string((long)::getpid()));

    uint64_t h = fppETagHash(version, salt);
    h = fppETagHash(req->path(), h);
    h = fppETagHash(req->query(), h);
    char buf[64];
    snprintf(buf, sizeof(buf), "v%llx", (unsigned long long)h);
    return std::string(buf);
}

// True when the client already holds this version of this route, and the
// handler can skip building the body entirely. Pair it with
// makeVersionedETagResponse() below so the 200 goes back carrying the same tag:
//
//     std::string ver = std::to_string(generation);
//     if (etagMatches(req, ver)) {
//         return makeNotModifiedResponse(makeETagToken(req, ver));
//     }
//     ... build body ...
//     return makeVersionedETagResponse(req, ver, body);
//
// The callable overload does both halves in one step and is harder to get
// wrong -- forgetting the tag on the 200 leaves a client that can never
// revalidate, which fails silently as "the ETag does nothing".
inline bool etagMatches(const HttpRequestPtr& req, const std::string& version) {
    return fppETagPresent(req, makeETagToken(req, version));
}

// Body already in hand.
inline HttpResponsePtr makeVersionedETagResponse(const HttpRequestPtr& req, const std::string& version,
                                                 const std::string& body,
                                                 const std::string& contentType = "application/json") {
    std::string bare = makeETagToken(req, version);
    if (fppETagPresent(req, bare)) {
        return makeNotModifiedResponse(bare);
    }
    auto resp = makeStringResponse(body, 200, contentType);
    resp->addHeader("ETag", "\"" + bare + "\"");
    return resp;
}

// Preferred form: `bodyFn` runs only on a miss, so a 304 costs a hash and
// nothing else. SFINAE on callability keeps this from competing with the
// std::string overload above.
template <typename BodyFn, typename = decltype(std::declval<BodyFn&>()())>
inline HttpResponsePtr makeVersionedETagResponse(const HttpRequestPtr& req, const std::string& version,
                                                 BodyFn&& bodyFn,
                                                 const std::string& contentType = "application/json") {
    std::string bare = makeETagToken(req, version);
    if (fppETagPresent(req, bare)) {
        return makeNotModifiedResponse(bare);
    }
    auto resp = makeStringResponse(bodyFn(), 200, contentType);
    resp->addHeader("ETag", "\"" + bare + "\"");
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

