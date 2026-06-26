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

// Lightweight HTTP types for headers that only need to *name* a request or
// response (e.g. `HttpResponsePtr render_GET(const HttpRequestPtr&)` method
// declarations) without pulling in the full Drogon framework.
//
// Including <drogon/HttpRequest.h>+<drogon/HttpResponse.h> costs ~105k lines of
// preprocessing per translation unit. Because these aliases are just
// shared_ptrs to forward-declared classes, a header can declare such methods
// against this file instead and stay cheap. The .cpp that actually *implements*
// those methods (and therefore calls Drogon members or the fpphttp.h inline
// helpers) must include "fpphttp.h" directly.
//
// fpphttp.h includes this file, so anything that includes fpphttp.h still gets
// these names -- the public/plugin-facing surface is unchanged.

#include <functional>
#include <memory>
#include <string>

// Trantor (a Drogon dependency) #defines LOG_WARN / LOG_INFO / LOG_DEBUG as
// macros that shadow FPP's LogLevel enum values used by log.h's LogWarn/
// LogInfo/LogDebug. Plugins that include <drogon/...> directly and then an FPP
// header (Plugin.h / Commands.h include this file) need those macros cleared so
// their LogWarn() calls still compile. fpphttp.h does the same after its drogon
// include; this restores the cleanup for the lightweight path. Undef of a
// non-macro is a no-op, so this is harmless when trantor was not included.
#ifdef LOG_WARN
#undef LOG_WARN
#endif
#ifdef LOG_INFO
#undef LOG_INFO
#endif
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif

namespace drogon {
class HttpRequest;
class HttpResponse;
} // namespace drogon

// These match drogon's own `using HttpRequestPtr = std::shared_ptr<HttpRequest>`
// exactly, so they are the same type whether reached through this header or
// through the real Drogon headers.
using HttpRequestPtr = std::shared_ptr<drogon::HttpRequest>;
using HttpResponsePtr = std::shared_ptr<drogon::HttpResponse>;
using HttpCallback = std::function<void(const HttpResponsePtr&)>;

// ---- Drogon-free part of the libhttpserver compatibility shims ----
// The webserver shim's API never touches Drogon (register_resource takes an
// http_resource* and is implemented out-of-line in fpphttp_compat.cpp), so it
// lives here where a plugin-facing header can use it cheaply. The Drogon-using
// shims (http_request/http_response/string_response/http_resource) stay in
// fpphttp.h. Define FPP_NO_HTTP_COMPAT_SHIMS before including to opt out.
#ifndef FPP_NO_HTTP_COMPAT_SHIMS

namespace httpserver {

class http_resource; // defined in fpphttp.h

// Shim for httpserver::webserver. Passed to old-style registerApis(webserver*)
// overrides so they can call register_resource() and have routes land in drogon.
class webserver {
public:
    void setPluginName(const std::string& name) { pluginName = name; }

    // Registers an http_resource for the given path with drogon.
    // If 'family' is true, also registers a regex catch-all for all subpaths.
    // Implemented in fpphttp_compat.cpp to avoid pulling in HttpAppFramework.h here.
    void register_resource(const std::string& path, http_resource* resource,
                           bool family = false);

    // Zeros the atomic slot for this path so in-flight and future drogon
    // dispatches return 410 Gone instead of calling into freed plugin memory.
    // Implemented in fpphttp_compat.cpp alongside register_resource.
    void unregister_resource(const std::string& path, http_resource*);
    void unregister_resource(const std::string& path);

    std::string pluginName;
};

} // namespace httpserver

#endif // FPP_NO_HTTP_COMPAT_SHIMS
