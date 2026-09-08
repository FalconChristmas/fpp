/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <chrono>
#include <string>

#include "CurlManager.h"
#include "common.h"
#include "log.h"
#include "settings.h"

#include "CapeLicenseNotify.h"

// How long to wait before trying again when nothing has been delivered yet.  A
// box that is offline at boot and gains a network later must still report, so
// this cannot be "once at startup"; a day is often enough to catch that and
// rare enough not to be a beacon.
#define CAPE_LICENSE_RETRY_HOURS 24

// How long after a successful notification before sending another.  The vendor
// wants to know the situation is still current, not to hear about it daily.
// Same shape and same floor as the statistics publish interval.
#define CAPE_LICENSE_REPEAT_DAYS 7

std::string CapeLicenseStateFile() {
    return FPP_DIR_CONFIG("/cape_license_state.json");
}

static std::string urlEscape(const std::string& s) {
    static const char* HEX = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += HEX[c >> 4];
            out += HEX[c & 0xF];
        }
    }
    return out;
}

static void appendParam(std::string& url, const char* name, const std::string& value) {
    if (value.empty()) {
        return;
    }
    url += (url.find('?') == std::string::npos) ? '?' : '&';
    url += name;
    url += '=';
    url += urlEscape(value);
}

// Which vendor URL the notification goes to.
//
// `licenseCheckUrl` is the endpoint a vendor can declare in their EEPROM image
// to receive this and nothing else.  Where it is absent -- every EEPROM in the
// field today -- the logo URL is used, because that is the host the old
// mechanism reached by hijacking the <img> and is therefore known to be the
// vendor's own and known to be in the CSP.  Coverage never regresses just
// because an EEPROM predates the endpoint.
static std::string notifyURL(const Json::Value& capeInfo) {
    if (!capeInfo.isMember("vendor")) {
        return "";
    }
    const Json::Value& vendor = capeInfo["vendor"];
    std::string url;
    if (vendor.isMember("licenseCheckUrl")) {
        url = vendor["licenseCheckUrl"].asString();
    } else if (vendor.isMember("image")) {
        url = vendor["image"].asString();
    } else if (vendor.isMember("url")) {
        url = vendor["url"].asString();
    }
    if (url.empty() || !startsWith(url, "http")) {
        return "";
    }

    // What the vendor needs to match this against their order book, and no more.
    // It is the same set the hijacked logo fetch already sent, so nothing new is
    // disclosed by naming the request for what it is -- and data minimisation is
    // part of what makes the Art 6(1)(f) balance come out this way (see the
    // header), so this list does not grow without that being reconsidered.
    appendParam(url, "lc", "1");
    if (capeInfo.isMember("id")) {
        appendParam(url, "id", capeInfo["id"].asString());
    }
    if (capeInfo.isMember("serialNumber")) {
        appendParam(url, "sn", capeInfo["serialNumber"].asString());
    }
    if (capeInfo.isMember("cs")) {
        appendParam(url, "cs", capeInfo["cs"].asString());
    }
    if (capeInfo.isMember("verifiedKeyId")) {
        appendParam(url, "key", capeInfo["verifiedKeyId"].asString());
    }
    return url;
}

// Identifies the cape this state refers to.  A different cape -- or the same
// cape re-signed -- is a new detection and reports immediately rather than
// inheriting the previous one's quiet period.
static std::string capeIdentity(const Json::Value& capeInfo) {
    std::string id;
    for (const char* k : { "id", "serialNumber", "cs", "verifiedKeyId" }) {
        if (capeInfo.isMember(k)) {
            id += capeInfo[k].asString();
        }
        id += "/";
    }
    return id;
}

static void recordAttempt(const std::string& identity, bool delivered) {
    Json::Value state;
    if (FileExists(CapeLicenseStateFile())) {
        LoadJsonFromFile(CapeLicenseStateFile(), state);
    }
    if (!state.isObject()) {
        state = Json::Value(Json::objectValue);
    }
    state["cape"] = identity;
    state["lastAttempt"] = (Json::Int64)time(nullptr);
    if (delivered) {
        state["lastSuccess"] = (Json::Int64)time(nullptr);
        state["source"] = "device";
    }
    SaveJsonToFile(state, CapeLicenseStateFile());
    SetFilePerms(CapeLicenseStateFile());
}

void CapeLicenseNotifyBackground() {
    // Nothing here is expensive, but the idle tick runs about once a second and
    // this needs to run about once a day, so hold the file reads behind a clock
    // check.  The counter is deliberately in-process: a restart may re-check
    // sooner than the interval, which is harmless -- the state file below is
    // what actually decides whether anything is sent.
    static auto nextCheck = std::chrono::steady_clock::now();
    static bool inFlight = false;
    if (inFlight || std::chrono::steady_clock::now() < nextCheck) {
        return;
    }
    nextCheck = std::chrono::steady_clock::now() + std::chrono::hours(1);

    Json::Value capeInfo;
    std::string capeFile = FPP_DIR_MEDIA("/tmp/cape-info.json");
    if (!FileExists(capeFile) || !LoadJsonFromFile(capeFile, capeInfo) || !capeInfo.isMember("licenseCheck")) {
        // The overwhelmingly common case: no cape, or a cape whose signature is
        // bound to real hardware.  Nothing to report, now or later.
        return;
    }

    // Deliberately NOT gated on hideExternalURLs.  That setting is kiosk mode:
    // it hides links so somebody at the kiosk cannot navigate off the box, and
    // it says nothing about whether FPP itself may make a request.  Reading it
    // as "the user asked for no outbound traffic" would be inventing a privacy
    // control out of a navigation one -- and it would leave the vendor hearing
    // nothing from precisely the installs least likely to be administered
    // through a browser.
    std::string identity = capeIdentity(capeInfo);
    Json::Value state;
    if (FileExists(CapeLicenseStateFile())) {
        LoadJsonFromFile(CapeLicenseStateFile(), state);
    }
    time_t now = time(nullptr);
    if (state.isObject() && state["cape"].asString() == identity) {
        time_t lastSuccess = state.isMember("lastSuccess") ? (time_t)state["lastSuccess"].asInt64() : 0;
        time_t lastAttempt = state.isMember("lastAttempt") ? (time_t)state["lastAttempt"].asInt64() : 0;
        if (lastSuccess && (now - lastSuccess) < (CAPE_LICENSE_REPEAT_DAYS * 24 * 60 * 60)) {
            return;
        }
        // Either transport counts as an attempt, so a browser that reaches the
        // vendor daily keeps this quiet even where the device never can.
        if (!lastSuccess && (now - lastAttempt) < (CAPE_LICENSE_RETRY_HOURS * 60 * 60)) {
            return;
        }
    }

    std::string url = notifyURL(capeInfo);
    if (url.empty()) {
        LogDebug(VB_GENERAL, "Cape signature is not bound to this hardware, but the cape declares no vendor URL to report it to\n");
        return;
    }

    // Logged either way.  The point of moving this out of the logo fetch is that
    // the device's own logs show it happened; a detection nobody can tell from a
    // coincidence is what the old mechanism left behind.
    LogInfo(VB_GENERAL, "Cape signature verifies but is not bound to this hardware; notifying the cape vendor\n");
    inFlight = true;
    std::string identityCopy = identity;
    CurlManager::INSTANCE.addGet(url, [identityCopy](int rc, const std::string& resp) {
        inFlight = false;
        bool delivered = (rc >= 200 && rc < 400);
        if (delivered) {
            LogInfo(VB_GENERAL, "Cape vendor notified of unbound cape signature (HTTP %d)\n", rc);
        } else {
            LogInfo(VB_GENERAL, "Could not notify the cape vendor of an unbound cape signature (HTTP %d); will retry\n", rc);
        }
        recordAttempt(identityCopy, delivered);
    });
}
