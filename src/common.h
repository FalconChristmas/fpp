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

// The MAC of an on-link IPv4 neighbour, as 12 uppercase hex digits with no
// separators, or "" if it isn't known.  Read from the kernel's ARP table, so it
// only answers for hosts on a directly attached subnet -- a routed host simply
// has no entry, which is what we want (we must never hand back the gateway's
// MAC for a device behind it).  Used to give controllers that report no UUID of
// their own a stable identity; see MultiSyncSystem::update().
std::string GetMacForAddress(const std::string& address);
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

