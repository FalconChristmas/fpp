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

// FPP-internal, deliberately NOT part of the plugin API.
//
// This lives outside fpphttp.h because it is the only helper of its kind that
// needs HttpRequest's *members* (getHeader/getPeerAddr) rather than just the
// HttpRequestPtr alias.  Plugins vendor fpphttp.h (along with Plugin.h,
// fpphttp_types.h and fpp-json-fwd.h) and compile it against their own stand-in
// for drogon; a non-template inline function is emitted in every translation
// unit that includes its header, so putting this in fpphttp.h would force every
// such stub to grow both methods.  Nothing outside FPP's own sources includes
// this file, so the requirement stays where it belongs.

#include "fpphttp.h"

#include <string>

// Best-effort real client IP for a request that may have come through FPP's
// local Apache reverse proxy (see etc/apache2.site - it proxies /api/* and
// friends to this process on 127.0.0.1:FPP_HTTP_PORT and, by Apache's
// mod_proxy default (ProxyAddHeaders On), appends X-Forwarded-For; the same
// default applies to the cross-host /proxy/<ip>/api/... rules used to reach
// another FPP's API through this one's Apache, so a multi-hop chain of FPP
// devices relaying a request ends up as multiple comma-separated entries).
// Logging the raw TCP peer for those requests always shows Apache's own
// loopback connection, indistinguishable from a local script/plugin calling
// this process directly.
//
// This is intentionally simple and NOT a trust/security check: if
// X-Forwarded-For is present at all, use its LEFTMOST entry (the original
// client, as far back through any proxy chain as the header reaches);
// otherwise fall back to the raw TCP peer. The header is trivially spoofable
// by anything that talks to this port directly - Apache only ever appends to
// whatever value it received, never validates or replaces what's already
// there - so treat this as best-effort attribution for logs, never as an
// access-control decision.
inline std::string getEffectiveClientIP(const HttpRequestPtr& req) {
    std::string xff = req->getHeader("x-forwarded-for");
    if (!xff.empty()) {
        std::string first = xff.substr(0, xff.find(','));
        size_t b = first.find_first_not_of(" \t");
        size_t e = first.find_last_not_of(" \t");
        if (b != std::string::npos) {
            return first.substr(b, e - b + 1);
        }
    }
    return req->getPeerAddr().toIp();
}
