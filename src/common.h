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

#include <stdint.h>
#include "fpp-json-fwd.h"

#include <functional>
#include <list>
#include <string>
#include <vector>

#include "common_mini.h"
#include "log.h"

/////////////////////////////////////////////////////////////////////////////
// printf macros for printing bitmaps
//
// Macros from https://stackoverflow.com/a/25108449

#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i) \
    (((i)&0x80ll) ? '1' : '0'),       \
        (((i)&0x40ll) ? '1' : '0'),   \
        (((i)&0x20ll) ? '1' : '0'),   \
        (((i)&0x10ll) ? '1' : '0'),   \
        (((i)&0x08ll) ? '1' : '0'),   \
        (((i)&0x04ll) ? '1' : '0'),   \
        (((i)&0x02ll) ? '1' : '0'),   \
        (((i)&0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8 PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8), PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16 PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64 \
    PRINTF_BINARY_PATTERN_INT32 PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)

/////////////////////////////////////////////////////////////////////////////
void HexDump(const char* title, const void* data, int len, FPPLoggerInstance& facility, int perLine = 16);
int CheckForHostSpecificFile(const std::string& hostname, std::string& filename);
int CheckForHostSpecificFile(const char* hostname, char* filename);
std::string ReplaceKeywords(std::string str, std::map<std::string, std::string>& keywords);
char* FindInterfaceForIP(char* ip);

void MergeJsonValues(Json::Value& a, Json::Value& b);
Json::Value LoadJsonFromFile(const std::string& filename);
Json::Value LoadJsonFromString(const std::string& str);
bool LoadJsonFromString(const std::string& str, Json::Value& root);
bool LoadJsonFromFile(const std::string& filename, Json::Value& root);
bool LoadJsonFromFile(const char* filename, Json::Value& root);

/*
 * Load a config file and require its root to be the shape the caller expects.
 *
 * A file that parses cleanly but holds the wrong kind of value - an array where
 * an object is expected, or the reverse - is a corrupt config, not usable data.
 * Every jsoncpp accessor a caller reaches for next (operator[], isMember(),
 * getMemberNames(), get()) throws Json::LogicError on that mismatch, and nothing
 * in fppd catches it, so a single bad config file aborts the daemon. Checking
 * here, where untrusted file data enters, keeps that throw unreachable.
 *
 * On any failure root is set to an empty value of the expected shape, so callers
 * that ignore the return value iterate over nothing instead of aborting.
 *
 * These are separate overloads rather than a defaulted argument on the two
 * versions above on purpose: adding a parameter would change those mangled names
 * and break already-built plugins that link against them. The shape is an FPP
 * enum rather than Json::ValueType so this header can keep using the Json::Value
 * forward declaration instead of pulling jsoncpp into every includer.
 */
enum class JsonRoot {
    Object,
    Array
};
bool LoadJsonFromFile(const std::string& filename, Json::Value& root, JsonRoot expected);
Json::Value LoadJsonFromFile(const std::string& filename, JsonRoot expected);
bool LoadJsonFromString(const std::string& str, Json::Value& root, JsonRoot expected);
Json::Value LoadJsonFromString(const std::string& str, JsonRoot expected);
std::string SaveJsonToString(const Json::Value& root, const std::string& indentation = "");
bool SaveJsonToString(const Json::Value& root, std::string& str, const std::string& indentation);
bool SaveJsonToFile(const Json::Value& root, const std::string& filename, const std::string& indentation = "\t");
bool SaveJsonToFile(const Json::Value& root, const char* filename, const char* indentation = "\t");

std::string getSimpleHTMLTTag(const std::string& html, const std::string& searchStr, const std::string& skipStr, const std::string& endStr);
std::string getSimpleXMLTag(const std::string& xml, const std::string& tag);

// URL Helpers
// Build an "http://host[path]" URL from a raw address.  IPv6 literals are
// wrapped in brackets and any link-local zone id ("%iface") is percent-encoded
// so curl accepts them; IPv4 addresses and hostnames are passed through
// unchanged.  This is what makes discovery work on IPv6-only networks.
std::string buildHttpURL(const std::string& address, const std::string& path = "");

// True for an IPv4 127/8 or IPv6 ::1 address.  Discovery treats loopback as
// "this box finding itself" and never as a peer -- see MultiSync::UpdateSystem().
bool IsLoopbackAddress(const std::string& address);

// Resolve `host` to a single IPv4 address, stored in `addr` in network byte
// order.  A dotted-quad is parsed directly; anything else goes to getaddrinfo()
// and is remembered -- successes and failures alike -- in a small process-wide
// cache with a short TTL.
//
// The negative caching is the point.  An unresolvable name costs a flat ~4s
// through systemd-resolved, and a name only has to appear once in
// co-universes.json to be looked up from several places during startup; six
// such lookups for two dead output hostnames were 23s of a 26s fppd start.
// Callers must still hold onto the address they get -- this is a stampede
// guard, not a substitute for the resolver.
//
// TTLs are deliberately short in both directions: long enough to collapse a
// startup burst, short enough that a controller which comes up (or moves) is
// picked up on the next poll rather than at the next restart.
bool ResolveHostToIPv4(const std::string& host, uint32_t& addr);

// As above, returning the dotted-quad form, or "" if it could not be resolved.
std::string ResolveHostToIPv4(const std::string& host);

// Drop every cached entry.  Call this when the network underneath the resolver
// changes (an interface gaining or losing an address), so a name that failed
// while the link was down is retried immediately instead of after the TTL.
void FlushHostResolveCache();

// Reduces a MAC in any of the usual spellings to 12 uppercase hex digits with
// no separators, or "" if it isn't one.  Every path that derives an identity
// from a hardware address must go through this, so that the same device gets
// the same string whichever way it was found.
std::string NormalizeMacAddress(const std::string& mac);

// The MAC of an on-link IPv4 neighbour, as 12 uppercase hex digits with no
// separators, or "" if it isn't known.  Read from the kernel's ARP table, so it
// only answers for hosts on a directly attached subnet -- a routed host simply
// has no entry, which is what we want (we must never hand back the gateway's
// MAC for a device behind it).  Used to give controllers that report no UUID of
// their own a stable identity; see MultiSyncSystem::update().
std::string GetMacForAddress(const std::string& address);
// DEPRECATED.  These drive a private curl easy handle to completion inline, so
// the calling thread is stopped for as long as the request takes -- up to the
// connect timeout plus `timeout` per call, and a caller that makes several in a
// row pays that for each one.  New FPP code must use CurlManager instead: its
// add()/addGet()/addPost()/addPut() queue onto the shared multi handle and hand
// the answer to a callback run from the main loop, so nothing blocks.
//
// They are kept, and will stay kept, because external channel-output plugins
// link against them.  They are also still the only correct choice for the FPP
// code that has no main loop to complete against: fppmm and fppoled are their
// own binaries, and CurlManager's synchronous doGet()/doPut() are not usable
// off the main-loop thread -- they spin on processCurls(), which drains and
// invokes *every* subsystem's pending callbacks on whatever thread calls it.
// Until CurlManager grows a thread-safe blocking call, "deprecated" here means
// "do not reach for this in new fppd code", not "unusable".
bool urlHelper(const std::string method, const std::string& url, const std::string& data, std::string& resp, const std::list<std::string>& headers, const unsigned int timeout = 30);
bool urlHelper(const std::string method, const std::string& url, const std::string& data, std::string& resp, const unsigned int timeout = 30);
bool urlHelper(const std::string method, const std::string& url, std::string& resp, const unsigned int timeout = 30);
bool urlGet(const std::string url, std::string& resp);
bool urlPost(const std::string url, const std::string data, std::string& resp);
bool urlPut(const std::string url, const std::string data, std::string& resp);
bool urlDelete(const std::string url, const std::string data, std::string& resp);
bool urlDelete(const std::string url, std::string& resp);

std::string base64Encode(uint8_t const* bytes_to_encode, unsigned int in_len);
std::vector<uint8_t> base64Decode(std::string const& encodedString);

void ShutdownFPPD(bool restart = false);
// Restart fppd and resume the currently-running playlist even when it was
// started manually rather than by the scheduler.  For self-initiated recovery
// restarts (the wedged-decoder ladder in GStreamerOut) where coming back idle
// and leaving the show -- and every synced remote -- dead defeats the whole
// point of restarting (issue #2727).
void RestartFPPDResumingPlaylist();
bool RestartShouldResumePlaylist();
void RegisterShutdownHandler(const std::function<void(bool)> hook);

// Breadcrumb for the main-loop stall watchdog (issue #2727): the last call the
// fppd main loop entered.  Pass a string literal (or any string with static
// storage) -- the watchdog thread reads the pointer while the main loop is
// wedged, so it must outlive the call that set it.
void SetMainLoopPhase(const char* phase);
const char* GetMainLoopPhase();

void GetCurrentFPPDStatus(Json::Value& result);

std::string getPlatform();

inline std::string toStdStringAndFree(char* v) {
    std::string s = v;
    free(v);
    return s;
}

void SetThreadName(const std::string& name);
bool SetThreadRealtimePriority(int priority);

void TransposeBits32x32(uint32_t *dst, uint32_t *src);

