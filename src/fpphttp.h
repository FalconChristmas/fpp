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

// Compatibility layer for HTTP handling using Drogon framework.
// This replaces the previous libhttpserver-based implementation.
// Only include the minimal drogon headers to avoid DrObject auto-registration
// in translation units that don't need the full framework.

// HttpRequestPtr/HttpResponsePtr/HttpCallback aliases and the (drogon-free)
// httpserver::webserver shim live here so declaration-only headers can use them
// without the heavy drogon include below. See fpphttp_types.h.
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

// ---- Source-level compatibility shims for plugins written against libhttpserver ----
// These allow plugin source code that used libhttpserver types to be recompiled
// against the new drogon-based FPP API with minimal or no code changes.
// Define FPP_NO_HTTP_COMPAT_SHIMS before including this header to opt out.

#ifndef FPP_NO_HTTP_COMPAT_SHIMS

namespace httpserver {

// Wraps a drogon HttpRequestPtr and exposes the old libhttpserver http_request API.
class http_request {
public:
    explicit http_request(const HttpRequestPtr& r) :
        m_req(r), m_pieces(getPathPieces(r->path())) {}

    std::string get_path() const { return m_req->path(); }
    const std::vector<std::string>& get_path_pieces() const { return m_pieces; }
    std::string get_arg(const std::string& key) const { return m_req->getParameter(key); }
    std::string get_content() const { return std::string(m_req->body()); }
    std::string get_querystring() const { return m_req->query(); }
    std::string get_method() const {
        switch (m_req->method()) {
        case drogon::Get:    return "GET";
        case drogon::Post:   return "POST";
        case drogon::Put:    return "PUT";
        case drogon::Delete: return "DELETE";
        case drogon::Head:   return "HEAD";
        default:             return "UNKNOWN";
        }
    }
    const HttpRequestPtr& drogonRequest() const { return m_req; }

private:
    HttpRequestPtr m_req;
    std::vector<std::string> m_pieces;
};

// Abstract base for libhttpserver-style responses.
class http_response {
public:
    virtual ~http_response() = default;
    virtual HttpResponsePtr toDrogon() const = 0;
};

// Drop-in replacement for httpserver::string_response.
class string_response : public http_response {
public:
    string_response(const std::string& body, int code = 200,
                    const std::string& contentType = "text/plain") :
        m_body(body), m_code(code), m_ct(contentType) {}
    HttpResponsePtr toDrogon() const override {
        return makeStringResponse(m_body, m_code, m_ct);
    }

private:
    std::string m_body;
    int m_code;
    std::string m_ct;
};

// Base class for libhttpserver-style resource objects.
// Plugins can continue to inherit from this and override render_* methods.
class http_resource {
public:
    virtual ~http_resource() = default;
    virtual std::shared_ptr<http_response> render_GET(const http_request&) {
        return std::make_shared<string_response>("Method Not Allowed", 405);
    }
    virtual std::shared_ptr<http_response> render_POST(const http_request&) {
        return std::make_shared<string_response>("Method Not Allowed", 405);
    }
    virtual std::shared_ptr<http_response> render_PUT(const http_request&) {
        return std::make_shared<string_response>("Method Not Allowed", 405);
    }
    virtual std::shared_ptr<http_response> render_DELETE(const http_request&) {
        return std::make_shared<string_response>("Method Not Allowed", 405);
    }
    virtual std::shared_ptr<http_response> render_HEAD(const http_request& req) {
        return render_GET(req);
    }
};

// Note: the httpserver::webserver shim is defined in fpphttp_types.h (it does
// not depend on drogon), and is available here via the include above.

} // namespace httpserver

#endif // FPP_NO_HTTP_COMPAT_SHIMS
