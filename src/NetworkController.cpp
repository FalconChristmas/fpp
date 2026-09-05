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
#include "fpp-pch.h"

#include "fpp-json.h"

#include <regex>
#include <stdlib.h>
#include <string>
#include <vector>

#include "CurlManager.h"
#include "NetworkController.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"
#include "util/RegExCache.h"

NetworkController::NetworkController(const std::string& ipStr) :
    ip(ipStr),
    hostname(ipStr),
    vendor("Unknown"),
    vendorURL("Unknown"),
    typeId(kSysTypeUnknown),
    typeStr("Unknown"),
    uuid("Unknown"),
    ranges("0-0"),
    version("Unknown"),
    majorVersion(0),
    minorVersion(0),
    systemMode(PLAYER_MODE) {
}

// One in-progress detection.  The detectors below are tried in order, and any
// of them may have to ask the device a question before it can say yes or no --
// so the walk cannot be a loop.  Each step either claims the device
// (matched()), passes (noMatch(), which advances to the next detector), or
// issues a fetch() whose completion does one of those two later.  The object
// lives until one of those outcomes is reached and deletes it.
class NetworkController::Detection {
public:
    // `blocking` drives the whole walk inline on the calling thread, using the
    // synchronous curl helpers, so the callback has run by the time run()
    // returns.  That is only for the deprecated blocking overload below; FPP's
    // own callers leave it false.
    Detection(const std::string& ipStr, const std::string& htmlStr,
              std::function<void(NetworkController*)>&& cb, bool blockingCalls = false) :
        ip(ipStr),
        html(htmlStr),
        nc(new NetworkController(ipStr)),
        callback(std::move(cb)),
        blocking(blockingCalls) {
    }

    const std::string ip;
    const std::string html;

    // Deliberately shared by every detector in turn, exactly as the old
    // sequential version shared one object: a detector that fills in vendor and
    // typeId and only then fails its probe leaves that behind for whichever
    // detector claims the device to overwrite.
    NetworkController* const nc;

    std::function<void(NetworkController*)> callback;
    const bool blocking = false;
    size_t step = 0;

    // Run the detector at `step`, or finish empty once the list is exhausted.
    void run() {
        typedef void (NetworkController::*DetectStep)(Detection*);
        // Order is significant and is the order the blocking version used.
        static const DetectStep STEPS[] = {
            &NetworkController::DetectFPP,
            &NetworkController::DetectFalconController,
            &NetworkController::DetectSanDevicesController,
            &NetworkController::DetectESPixelStickController,
            &NetworkController::DetectBaldrickController,
            &NetworkController::DetectAlphaPixController,
            &NetworkController::DetectHinksPixController,
            &NetworkController::DetectDIYLEDExpressController,
            &NetworkController::DetectExperienceController,
            &NetworkController::DetectWLEDController,
        };
        if (step >= (sizeof(STEPS) / sizeof(STEPS[0]))) {
            finish(nullptr);
            return;
        }
        (nc->*STEPS[step])(this);
    }

    void matched() {
        finish(nc);
    }

    // Only the detectors that settle it from the HTML alone reach this
    // synchronously; the rest come back through it from a curl completion.  The
    // recursion is bounded by the length of STEPS.
    void noMatch() {
        ++step;
        run();
    }

    // Ask the device something.  `ok` is false only when the transfer itself
    // failed -- which is exactly what the blocking urlGet() this replaced
    // reported, so a 404 still reaches the handler with the error page as its
    // body and the detectors, which only look for their own fields in the
    // response, behave as they did before.
    void fetch(const std::string& url, std::function<void(bool ok, const std::string& resp)>&& handler) {
        if (blocking) {
            std::string resp;
            bool ok = urlGet(url, resp);
            handler(ok, resp);
            return;
        }
        CurlManager::INSTANCE.addGet(url, [this, handler](int rc, const std::string& resp) {
            handler(rc != 0, resp);
        });
    }

    // Same contract as fetch(), for the one vendor whose status is only
    // reachable by POST.  Kept beside it so a detector never has to touch
    // CurlManager directly and risk a path that reaches none of matched() /
    // noMatch() / fetch().
    void post(const std::string& url, const std::string& body, const std::string& contentType,
              std::function<void(bool ok, const std::string& resp)>&& handler) {
        if (blocking) {
            std::string resp;
            bool ok = urlHelper("POST", url, body, resp, { "Content-Type: " + contentType });
            handler(ok, resp);
            return;
        }
        CurlManager::INSTANCE.addPost(url, body, contentType, [this, handler](int rc, const std::string& resp) {
            handler(rc != 0, resp);
        });
    }

private:
    void finish(NetworkController* result) {
        // Move the callback out and destroy ourselves before running it: it may
        // well start the next piece of work, and it must not do that while a
        // half-finished Detection is still alive.
        std::function<void(NetworkController*)> cb = std::move(callback);
        if (!result) {
            delete nc;
        }
        delete this;
        cb(result);
    }
};

void NetworkController::DetectControllerViaHTML(const std::string& ip, const std::string& html,
                                                std::function<void(NetworkController*)>&& callback) {
    if (html.empty()) {
        callback(nullptr);
        return;
    }
    (new Detection(ip, html, std::move(callback)))->run();
}

NetworkController* NetworkController::DetectControllerViaHTML(const std::string& ip, const std::string& html) {
    if (html.empty()) {
        return nullptr;
    }
    // The same detector walk, driven with the blocking curl helpers so it is
    // finished before this returns.  Kept for out-of-tree plugins compiled
    // against the pre-async signature; nothing in FPP calls it.
    NetworkController* result = nullptr;
    (new Detection(ip, html, [&result](NetworkController* nc) { result = nc; }, true))->run();
    return result;
}

void NetworkController::DetectFPP(Detection* st) {
    if (st->html.find("Falcon Player - FPP") == std::string::npos) {
        st->noMatch();
        return;
    }
    st->fetch(buildHttpURL(st->ip, "/api/system/info?simple=1"), [this, st](bool ok, const std::string& resp) {
        if (!ok) {
            st->noMatch();
            return;
        }
        Json::Value v;
        LoadJsonFromString(resp, v, JsonRoot::Object);
        hostname = v["HostName"].asString();
        vendor = "FPP";
        vendorURL = "https://falconchristmas.com/forum/";
        if (JsonHas(v, "channelRanges")) {
            ranges = v["channelRanges"].asString();
        }
        if (JsonHas(v, "uuid")) {
            uuid = v["uuid"].asString();
        }
        version = v["Version"].asString();
        typeStr = v["Variant"].asString();
        typeId = MultiSync::ModelStringToType(typeStr);
        if (typeId == kSysTypeFPP) {
            // Pi's tend to have a just the model in the Variant, we'll try mapping those
            typeId = MultiSync::ModelStringToType("Raspberry " + typeStr);
            if (typeId != kSysTypeFPP) {
                typeStr = "Raspberry " + typeStr;
            }
        }

        std::string md = v["Mode"].asString();
        if (md == "bridge") {
            systemMode = BRIDGE_MODE;
        } else if (md == "player") {
            systemMode = PLAYER_MODE;
        } else if (md == "remote") {
            systemMode = REMOTE_MODE;
        } else if (md == "master") {
            systemMode = PLAYER_MODE;
            sendingMultiSync = true;
        }
        if (v.isMember("multisync")) {
            sendingMultiSync = v["multisync"].asBool();
        }
        majorVersion = v["majorVersion"].asInt();
        minorVersion = v["minorVersion"].asInt();
        st->matched();
    });
}

void NetworkController::DetectFalconController(Detection* st) {
    LogExcess(VB_SYNC, "Checking if %s is a Falcon controller\n", st->ip.c_str());

    RegExCache re("\"css/falcon.css\"|\"/f16v2.js\"|\"js/cntrlr_(\\d+).js\"");
    std::smatch m;

    if (!std::regex_search(st->html, m, *re.regex)) {
        st->noMatch();
        return;
    }

    LogExcess(VB_SYNC, "%s is potentially a Falcon controller, checking further\n", st->ip.c_str());

    vendor = "Falcon";
    vendorURL = "https://pixelcontroller.com";

    st->fetch(buildHttpURL(st->ip, "/status.xml"), [this, st](bool ok, const std::string& resp) {
        std::size_t fStart = ok ? resp.find("<p>") : std::string::npos;
        if (fStart == std::string::npos) {
            st->noMatch();
            return;
        }
        typeId = (systemType)(atoi(getSimpleXMLTag(resp, "p").c_str()));

        if (typeId >= 0x80) { // v4 is just 0x80
            if (typeId == 0x82) {
                typeId = kSysTypeFalconF16v5;
            } else if (getSimpleXMLTag(resp, "np") == "16") {
                typeId = kSysTypeFalconF16v4;
            } else if (getSimpleXMLTag(resp, "np") == "48" || getSimpleXMLTag(resp, "np") == "32") {
                typeId = kSysTypeFalconF48v4;
            }
        } else { // v3 and below, 0x80 + p tag
            typeId = (systemType)(atoi(resp.substr(fStart + 3).c_str()) + 0x80);
        }

        typeStr = MultiSync::GetTypeString(typeId);

        version = getSimpleXMLTag(resp, "fv");
        systemMode = BRIDGE_MODE;

        if ((typeId == kSysTypeFalconF16v2) ||
            (typeId == kSysTypeFalconF4v2_64Mb) ||
            (typeId == kSysTypeFalconF16v2R) ||
            (typeId == kSysTypeFalconF4v2)) {
            hostname = getSimpleHTMLTTag(st->html, "Name:</td>", "\">", "</td>");
            version = getSimpleHTMLTTag(st->html, "SW Version:</td>", "\">", "</td>");
            if ((hostname != "") && (startsWith(version, hostname))) {
                std::string tmpStr(hostname);
                version.erase(0, hostname.length());
                TrimWhiteSpace(version);

                if (startsWith(version, "- "))
                    version.erase(0, 2);
            }
        } else if ((typeId == kSysTypeFalconF16v4) ||
                   (typeId == kSysTypeFalconF48v4)) {
            hostname = getSimpleXMLTag(resp, "n");
            std::size_t spacePos = version.find(" ");
            if (spacePos != std::string::npos) {
                majorVersion = atoi(version.substr(spacePos + 1).c_str());
            }
        } else {
            hostname = getSimpleXMLTag(resp, "n");

            if (version != "") {
                majorVersion = atoi(version.c_str());

                std::size_t verDot = version.find(".");
                if (verDot != std::string::npos) {
                    minorVersion = atoi(version.substr(verDot + 1).c_str());
                }
            }
        }

        // status.xml carries no identity on any generation.  The V4/V5 line has
        // a JSON API that reports one, reachable only by POST; everything older
        // answers 404 there and keeps the derived identity MultiSync gives it.
        //
        // Deliberately not allowed to affect detection: the XML above already
        // settled that this is a Falcon, so the request only ever adds a uuid,
        // and both outcomes reach matched().  An F48 predating the API is a
        // real example of the 404 path -- it is still a Falcon.
        if ((typeId == kSysTypeFalconF16v4) ||
            (typeId == kSysTypeFalconF48v4) ||
            (typeId == kSysTypeFalconF16v5)) {
            st->post(buildHttpURL(st->ip, "/api"),
                     "{\"T\":\"Q\",\"M\":\"ST\",\"B\":0,\"E\":0,\"I\":0,\"P\":{}}",
                     "application/json",
                     [this, st](bool aok, const std::string& aresp) {
                         Json::Value av;
                         // isObject() guards the get() below: jsoncpp rejects a
                         // keyed lookup on a scalar, and nothing here controls
                         // what a device puts in "P".
                         if (aok && LoadJsonFromString(aresp, av, JsonRoot::Object) &&
                             JsonHas(av, "P") && av["P"].isObject()) {
                             // Same spelling MultiSync uses for an ARP-derived
                             // address, so a device keeps one identity however
                             // it was discovered.
                             std::string mac = NormalizeMacAddress(av["P"].get("C", "").asString());
                             if (!mac.empty()) {
                                 uuid = MAC_UUID_PREFIX + mac;
                             }
                         }
                         DumpControllerInfo();
                         st->matched();
                     });
            return;
        }

        DumpControllerInfo();
        st->matched();
    });
}

void NetworkController::DetectSanDevicesController(Detection* st) {
    LogExcess(VB_SYNC, "Checking if %s is a SanDevices controller\n", st->ip.c_str());
    RegExCache re("Controller Model (E[0-9]+)");
    std::smatch m;

    if (!std::regex_search(st->html, m, *re.regex)) {
        st->noMatch();
        return;
    }

    LogExcess(VB_SYNC, "%s is potentially a SanDevices controller, checking further\n", st->ip.c_str());

    vendor = "SanDevices";
    vendorURL = "http://sandevices.com/";
    typeId = kSysTypeSanDevices;
    typeStr = m[1];
    systemMode = BRIDGE_MODE;

    // The status page states the board's MAC, so this costs no extra request --
    // and unlike the ARP fallback in MultiSync it works for a board on another
    // subnet.  Matched loosely across the intervening markup rather than against
    // one firmware's exact table layout, which the version regexes below already
    // have to be spelled twice to cope with.  Measured equal to the address ARP
    // reports for the same board, so it shares the one identity namespace (see
    // MultiSync::ApplyUUIDHint() for why that matters).
    RegExCache macre("MAC Address:[\\s\\S]{0,64}?((?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2})");
    std::smatch mm;
    if (std::regex_search(st->html, mm, *macre.regex)) {
        std::string mac = NormalizeMacAddress(mm[1].str());
        if (!mac.empty()) {
            // Same spelling MultiSync uses for an ARP-derived address, so a
            // device keeps one identity however it was discovered.
            uuid = MAC_UUID_PREFIX + mac;
        }
    }

    RegExCache v4re("Firmware Version:</th></td><td></td><td>([0-9]+.[0-9]+)</td>");
    RegExCache v5re("Firmware Version:</th></td><td>\\s?([0-9]+.[0-9]+)(-W\\d+)?</td>");

    if ((std::regex_search(st->html, m, *v4re.regex)) ||
        (std::regex_search(st->html, m, *v5re.regex))) {
        version = m[1];

        if (version != "") {
            majorVersion = atoi(version.c_str());

            std::size_t verDot = version.find(".");
            if (verDot != std::string::npos) {
                minorVersion = atoi(version.substr(verDot + 1).c_str());
            }
        }

        DumpControllerInfo();
        st->matched();
        return;
    }

    st->noMatch();
}

void NetworkController::DetectESPixelStickController(Detection* st) {
    LogExcess(VB_SYNC, "Checking if %s is running ESPixelStick firmware\n", st->ip.c_str());

    if (!contains(st->html, "\"esps.js\"")) {
        st->noMatch();
        return;
    }

    LogExcess(VB_SYNC, "%s is potentially an ESPixelStick, checking further\n", st->ip.c_str());

    vendor = "ESPixelStick";
    vendorURL = "https://forkineye.com";
    typeId = kSysTypeESPixelStick;
    typeStr = "ESPixelStick";
    systemMode = BRIDGE_MODE;

    // buildHttpURL() rather than the bare "<ip>/conf" this used to build: that
    // relied on curl guessing the scheme, and it could not work at all for an
    // IPv6 literal, which has to be bracketed.
    st->fetch(buildHttpURL(st->ip, "/conf"), [this, st](bool ok, const std::string& resp) {
        if (ok) {
            Json::Value config;
            LoadJsonFromString(resp, config, JsonRoot::Object);
            if (JsonHas(config, "network") && JsonHas(config["network"], "hostname")) {
                hostname = config["network"]["hostname"].asString();
            }

            if (hostname != "") {
                DumpControllerInfo();
                st->matched();
                return;
            }
        }
        st->noMatch();
    });
}

// Pulls "v3.5.4" apart into major/minor.  Tolerates the leading "v" being
// absent, and leaves the numbers alone if the string is not that shape.
void NetworkController::ParseBaldrickVersion(const std::string& fw) {
    version = fw;
    std::string verNum = (!version.empty() && version[0] == 'v') ? version.substr(1) : version;
    std::size_t verDot = verNum.find(".");
    if (verDot == std::string::npos) {
        return;
    }
    majorVersion = atoi(verNum.substr(0, verDot).c_str());
    std::size_t verDot2 = verNum.find(".", verDot + 1);
    if (verDot2 != std::string::npos) {
        minorVersion = atoi(verNum.substr(verDot + 1, verDot2 - (verDot + 1)).c_str());
    }
}

void NetworkController::DetectBaldrickController(Detection* st) {
    LogExcess(VB_SYNC, "Checking if %s is a Baldrick controller\n", st->ip.c_str());

    if (!contains(st->html, "Baldrick Board")) {
        st->noMatch();
        return;
    }

    LogExcess(VB_SYNC, "%s is potentially a Baldrick controller, checking further\n", st->ip.c_str());

    vendor = "ILightThat";
    vendorURL = "https://www.ilightthat.com";
    typeId = kSysTypeBaldrick;
    typeStr = "Baldrick";
    systemMode = BRIDGE_MODE;

    st->fetch(buildHttpURL(st->ip, "/system_state"), [this, st](bool ok, const std::string& resp) {
        if (!ok) {
            st->noMatch();
            return;
        }
        Json::Value state;
        LoadJsonFromString(resp, state, JsonRoot::Object);
        if (JsonHas(state, "board_model")) {
            typeStr = state["board_model"].asString();
        }

        // Newer firmware states the board's OWN id at the top level; older
        // firmware states only its neighbours'.  Note this is the ESP32 base
        // address, which is NOT what ARP sees for a board on Ethernet (that is
        // base+3) -- see MultiSync::ApplyUUIDHint() for why a vendor-reported
        // id must win over ARP rather than merely fill in behind it.  Measured
        // on a board reporting both: the id it gives for itself is byte-for-byte
        // the one its neighbour gives for it, so the two sources agree and
        // neither has to be preferred for correctness.
        if (JsonHas(state, "board_id")) {
            std::string bid = NormalizeMacAddress(state["board_id"].asString());
            if (!bid.empty()) {
                uuid = MAC_UUID_PREFIX + bid;
            }
        }

        // The neighbour list is still read, and still matters for two reasons:
        // it is the only identity a board on older firmware ever has, and it
        // names Baldricks of other kinds -- signal boards, DMX boards, input
        // boards -- that discovery may reach before or without reaching this
        // one.  Recorded against the peer address for the caller to apply;
        // nothing here invents a system that discovery has not otherwise found.
        if (JsonHas(state, "buddies") && state["buddies"].isArray()) {
            for (const auto& buddy : state["buddies"]) {
                if (!buddy.isObject()) {
                    continue;
                }
                std::string bip = buddy.get("ip", "").asString();
                std::string bid = NormalizeMacAddress(buddy.get("board_id", "").asString());
                if (!bip.empty() && !bid.empty()) {
                    peerUUIDs[bip] = MAC_UUID_PREFIX + bid;
                }
            }
        }
        if (JsonHas(state, "ota")) {
            const Json::Value& ota = state["ota"];
            // Two firmware generations, two shapes.  The older boards put the
            // version directly on "ota"; current ones report a list of
            // separately-updatable components under "ota.updatable" and carry
            // the version on each entry.  The first entry that has one is the
            // board itself.
            if (JsonHas(ota, "current_firmware_version")) {
                ParseBaldrickVersion(ota["current_firmware_version"].asString());
            } else if (JsonHas(ota, "updatable") && ota["updatable"].isArray()) {
                for (const auto& part : ota["updatable"]) {
                    if (part.isObject() && JsonHas(part, "current_firmware_version")) {
                        ParseBaldrickVersion(part["current_firmware_version"].asString());
                        break;
                    }
                }
            }
        }

        // /system_state has no hostname -- that lives in /settings, which is why
        // this used to be left showing the bare IP.  A failure here is not fatal:
        // the board is already identified, it just keeps the IP as its name.
        st->fetch(buildHttpURL(st->ip, "/settings"), [this, st](bool sok, const std::string& sresp) {
            Json::Value settings;
            if (sok && LoadJsonFromString(sresp, settings, JsonRoot::Object) &&
                JsonHas(settings, "hostname")) {
                std::string h = settings["hostname"].asString();
                if (!h.empty()) {
                    hostname = h;
                }
            }
            DumpControllerInfo();
            // Answering /system_state at all is what identifies the board; the
            // old `hostname != ""` test here could never fail, because the
            // constructor seeds hostname with the IP.
            st->matched();
        });
    });
}

void NetworkController::DetectAlphaPixController(Detection* st) {
    LogExcess(VB_SYNC, "Checking if %s is a AlphaPix controller\n", st->ip.c_str());

    RegExCache re("AlphaPix (\\d+|Flex|Evolution)");
    RegExCache re2("(\\d+) Port Ethernet to SPI Controller");
    std::smatch m, m2;

    // These boards serve one client at a time.  While another computer holds
    // the session every page, this one included, is replaced by a stub that
    // names no model and no version -- so without this the board matches
    // nothing, is dropped as an unrecognised address, and simply vanishes from
    // discovery with nothing said about why.  Claim it anyway: an AlphaPix with
    // no detail is far more use to somebody looking for a missing controller
    // than no row at all, and the log line names the actual cause.
    if (contains(st->html, "Existing user login")) {
        LogWarn(VB_SYNC, "%s looks like an AlphaPix whose web session is held by another client; "
                         "no model or version can be read until that session is released\n",
                st->ip.c_str());
        vendor = "HolidayCoro";
        vendorURL = "https://www.holidaycoro.com/";
        typeId = kSysTypeAlphaPix;
        typeStr = "AlphaPix";
        systemMode = BRIDGE_MODE;
        DumpControllerInfo();
        st->matched();
        return;
    }

    bool matchedModel = std::regex_search(st->html, m, *re.regex);
    if (!matchedModel && !std::regex_search(st->html, m2, *re2.regex)) {
        st->noMatch();
        return;
    }

    LogExcess(VB_SYNC, "%s is potentially a AlphaPix controller, checking further\n", st->ip.c_str());

    vendor = "HolidayCoro";
    vendorURL = "https://www.holidaycoro.com/";
    typeId = kSysTypeAlphaPix;
    // Read from whichever pattern actually matched.  This took m[1]
    // unconditionally, but m holds the result of the FAILED search whenever the
    // model is named only as "<n> Port Ethernet to SPI Controller" -- so every
    // board whose page uses that wording alone was typed as an empty string.
    typeStr = matchedModel ? m[1].str() : m2[1].str();
    systemMode = BRIDGE_MODE;

    // These boards state a "Device ID", and on the one that could be measured
    // it is the low 24 bits of the board's MAC written in decimal: 0404361 is
    // 0x062B89, and the address ARP reports is 00:00:00:06:2B:89.  The vendor
    // appears to use an all-zero OUI and hand out the id as the rest, which
    // makes this the only identity these boards expose that survives crossing a
    // subnet -- they publish no MAC on any page, and the xLights implementation
    // extracts none either.
    //
    // Spelled as the MAC it encodes rather than as an id of its own, so a board
    // identified from this page and one identified only from the ARP table land
    // on the same value instead of on two names for one device.
    RegExCache didre("Device ID:\\s*([0-9]+)");
    if (std::regex_search(st->html, m, *didre.regex)) {
        long id = strtol(m[1].str().c_str(), nullptr, 10); // base 10: the id is zero-padded
        if (id > 0 && id <= 0xFFFFFF) {
            char buf[16];
            snprintf(buf, sizeof(buf), "000000%06lX", id);
            // Same spelling MultiSync uses for an ARP-derived address, so a
            // device keeps one identity however it was discovered.
            uuid = MAC_UUID_PREFIX + std::string(buf);
        }
    }

    RegExCache vre("Currently Installed Firmware Version:  ([0-9]+.[0-9]+)");

    // The version is reported when it can be read, but is NOT what decides
    // whether this is an AlphaPix -- the model above already settled that.
    // Requiring it here meant a board whose page states a model but spells the
    // version differently (the 2.16+ web UI is a different layout) was rejected
    // outright rather than reported without a version.
    if (std::regex_search(st->html, m, *vre.regex)) {
        version = m[1];

        if (version != "") {
            majorVersion = atoi(version.c_str());

            std::size_t verDot = version.find(".");
            if (verDot != std::string::npos) {
                minorVersion = atoi(version.substr(verDot + 1).c_str());
            }
        }
    }

    DumpControllerInfo();
    st->matched();
}

void NetworkController::DetectHinksPixController(Detection* st) {
    LogExcess(VB_SYNC, "Checking if %s is a HinksPix controller\n", st->ip.c_str());
    if (!contains(st->html, "HinksPix Config")) {
        st->noMatch();
        return;
    }
    LogExcess(VB_SYNC, "%s is potentially a HinksPix controller, checking further\n", st->ip.c_str());

    vendor = "HolidayCoro";
    vendorURL = "https://www.holidaycoro.com/";
    typeId = kSysTypeHinksPix;
    typeStr = "HinksPix";
    systemMode = BRIDGE_MODE;

    DumpControllerInfo();
    st->matched();
}

void NetworkController::DetectDIYLEDExpressController(Detection* st) {
    LogExcess(VB_SYNC, "Checking if %s is a DIYLEDExpress controller\n", st->ip.c_str());

    RegExCache re("DIYLEDExpress E1.31 Bridge Configuration Page");
    std::smatch m;

    if (!std::regex_search(st->html, *re.regex)) {
        st->noMatch();
        return;
    }

    LogExcess(VB_SYNC, "%s is potentially a DIYLEDExpress controller, checking further\n", st->ip.c_str());

    vendor = "DIYLEDExpress";
    vendorURL = "http://www.diyledexpress.com/";
    typeId = kSysTypeDIYLEDExpress;
    typeStr = "E1.31 Bridge";
    systemMode = BRIDGE_MODE;

    // Firmware Rev: 4.02
    RegExCache vre("Firmware Rev: ([0-9]+.[0-9]+)");

    if (std::regex_search(st->html, m, *vre.regex)) {
        version = m[1];

        if (version != "") {
            majorVersion = atoi(version.c_str());

            std::size_t verDot = version.find(".");
            if (verDot != std::string::npos) {
                minorVersion = atoi(version.substr(verDot + 1).c_str());
            }
        }

        DumpControllerInfo();
        st->matched();
        return;
    }

    st->noMatch();
}

// The Experience controllers -- the Genius Pixel / PRO / Long Range / Pixel Link
// range, plus the rebadged LOR AURORA CORE and YPS VIVID units -- all serve the
// same petite-vue app.  Their <title> carries the retail brand, so it is not
// something we can match across the family; the app's own component markers are
// far more stable.  Either way the HTML only decides whether it is worth asking:
// nothing is accepted until /api/state answers with the fields this firmware
// defines, so a stray HTML match cannot misidentify some other device.
//
// These controllers do answer MultiSync pings, and that ping reports an exact
// model type.  This path exists for the two things the ping cannot do: it
// carries no UUID field at all, and it does not reach a controller on another
// subnet -- which is exactly the case co-universes.json seeding is there to
// cover.
// Pulls the hardware address out of one of these controllers' "network" object.
// Both firmware lines report every interface, so prefer the one actually
// answering on the address we are talking to: that is the MAC the ARP table
// would have given for the same device, and identity must not depend on which
// route found it.
static std::string ExperienceNetworkMac(const std::string& ip, const Json::Value& network) {
    if (!network.isObject()) {
        return "";
    }
    std::string fallback;
    for (const char* key : { "wired", "wifi" }) {
        if (!JsonHas(network, key) || !network[key].isObject()) {
            continue;
        }
        const Json::Value& iface = network[key];
        std::string mac = NormalizeMacAddress(iface.get("mac_address", "").asString());
        if (mac.empty()) {
            continue;
        }
        if (iface.get("ip_address", "").asString() == ip) {
            return mac;
        }
        if (fallback.empty()) {
            fallback = mac;
        }
    }
    return fallback;
}

void NetworkController::DetectExperienceController(Detection* st) {
    if (st->html.find("FriendlyNameBar()") == std::string::npos &&
        st->html.find("CurrentMonitoring()") == std::string::npos &&
        st->html.find("<title>Genius") == std::string::npos) {
        st->noMatch();
        return;
    }

    LogExcess(VB_SYNC, "%s is potentially an Experience controller, checking further\n", st->ip.c_str());

    st->fetch(buildHttpURL(st->ip, "/api/state"), [this, st](bool ok, const std::string& resp) {
        Json::Value v;
        if (!ok || !LoadJsonFromString(resp, v, JsonRoot::Object) || !JsonHas(v, "system")) {
            st->noMatch();
            return;
        }
        const Json::Value& sys = v["system"];
        if (!JsonHas(sys, "controller_model") || !JsonHas(sys, "controller_model_name")) {
            st->noMatch();
            return;
        }

        vendor = "Experience";

        // Deliberately the generic Experience type rather than a model-code lookup.
        // A table mapping every retail code to its own kSysTypeExperience* value
        // would be guesswork for the models we cannot test, and it would rot as the
        // range grows.  The generic id still lands in the range the UI treats as
        // this family, the exact model name below is the part a user reads, and
        // where a ping does reach the device it supplies the precise type anyway --
        // MultiSyncSystem::update() will not let this HTTP result overwrite it.
        typeId = kSysTypeExperienceGenius;
        typeStr = sys["controller_model_name"].asString();

        // The firmware string is not a stable shape across releases: the 1.x line
        // reported "Genius_PRO_Controller_16 v1.3.1-2", the 2.x line reports a bare
        // "2.2.0-0".  Accept the dotted version either at the start of the string or
        // after a "v", and require the dot so a model number ("...16 Port") cannot
        // be mistaken for one.  Anything unrecognised is passed through as-is rather
        // than dropped.
        std::string fw = sys.get("firmware_version", "").asString();
        std::smatch m;
        RegExCache re("(?:^|v)([0-9]+)\\.([0-9]+)(?:\\.([0-9]+))?");
        if (std::regex_search(fw, m, *re.regex)) {
            version = m[1].str() + "." + m[2].str();
            if (m[3].matched) {
                version += "." + m[3].str();
            }
            majorVersion = atoi(m[1].str().c_str());
            minorVersion = atoi(m[2].str().c_str());
        } else if (!fw.empty()) {
            version = fw;
        }

        // The 1.x line carries the interface addresses in the state document, so
        // keep them: if the identity endpoint below comes up empty they are the
        // MAC fallback, and `v` does not survive this callback.
        Json::Value network = v["network"];

        // The identity endpoint is the only place these report something stable and
        // their own.  Build the same string the statistics collector has always
        // built from it, so a controller is not identified two different ways
        // depending on which side of FPP is asking.
        st->fetch(buildHttpURL(st->ip, "/update/identity"), [this, st, network](bool idOk, const std::string& idResp) {
            Json::Value idv;
            if (idOk && LoadJsonFromString(idResp, idv, JsonRoot::Object) && JsonHas(idv, "id")) {
                std::string id = idv["id"].asString();
                std::string hw = idv.get("hardware", "").asString();
                if (!id.empty()) {
                    uuid = hw.empty() ? id : (hw + "-" + id);
                }
            }
            if (!uuid.empty() && uuid != "Unknown") {
                DumpControllerInfo();
                st->matched();
                return;
            }

            // No identity of its own.  The 2.x firmware dropped the endpoint above
            // entirely, so for that line this is the only identity there is -- and
            // unlike the ARP table it works for a controller on another subnet, which
            // is exactly where seeding discovery from the configured output addresses
            // reaches.  The 1.x line carries the addresses in the state document
            // already fetched; 2.x moved them to their own endpoint, so only ask for it
            // when the first place came up empty.
            std::string mac = ExperienceNetworkMac(st->ip, network);
            if (!mac.empty()) {
                // Same spelling MultiSync uses for an ARP-derived address, so a
                // device keeps one identity however it was discovered.
                uuid = MAC_UUID_PREFIX + mac;
                DumpControllerInfo();
                st->matched();
                return;
            }

            st->fetch(buildHttpURL(st->ip, "/api/current_state"), [this, st](bool curOk, const std::string& curResp) {
                Json::Value cur;
                if (curOk && LoadJsonFromString(curResp, cur, JsonRoot::Object)) {
                    std::string curMac = ExperienceNetworkMac(st->ip, cur["network"]);
                    if (!curMac.empty()) {
                        uuid = MAC_UUID_PREFIX + curMac;
                    }
                }
                DumpControllerInfo();
                st->matched();
            });
        });
    });
}

void NetworkController::DetectWLEDController(Detection* st) {
    if (st->html.find("WLED UI") == std::string::npos) {
        st->noMatch();
        return;
    }

    st->fetch(buildHttpURL(st->ip, "/json/info"), [this, st](bool ok, const std::string& resp) {
        if (!ok) {
            st->noMatch();
            return;
        }
        Json::Value v;
        LoadJsonFromString(resp, v, JsonRoot::Object);

        vendor = "WLED";
        vendorURL = "https://github.com/Aircoookie/WLED";

        // WLED reports its own MAC in the document already being read here, so
        // this costs no extra request.  Unlike the ARP fallback in MultiSync it
        // works for a controller on another subnet, which is where discovery
        // seeded from configured output addresses usually reaches.
        std::string mac = NormalizeMacAddress(v.get("mac", "").asString());
        if (!mac.empty()) {
            // Same spelling MultiSync uses for an ARP-derived address, so a
            // device keeps one identity however it was discovered.
            uuid = MAC_UUID_PREFIX + mac;
        }

        version = v["ver"].asString();
        hostname = v["name"].asString();
        typeStr = v["arch"].asString();
        typeId = kSysTypeWLED;
        systemMode = BRIDGE_MODE;

        if (version != "") {
            std::size_t verDot = version.find(".");
            if (verDot != std::string::npos) {
                majorVersion = atoi(version.substr(0, verDot).c_str());
                std::size_t verDot2 = version.find(".", verDot + 1);
                if (verDot2 != std::string::npos) {
                    minorVersion = atoi(version.substr(verDot + 1, verDot2 - (verDot + 1)).c_str());
                }
            }
        }
        st->matched();
    });
}

void NetworkController::DumpControllerInfo(void) {
    LogDebug(VB_SYNC, "Network Controller Info:\n"
                      "IP              : %s\n"
                      "UUID            : %s\n"
                      "Hostname        : %s\n"
                      "Vendor          : %s\n"
                      "Vendor URL      : %s\n"
                      "TypeID          : %d\n"
                      "TypeStr         : %s\n"
                      "Channel Ranges  : %s\n"
                      "Firmware Version: %s\n"
                      "Firmware MajorV : %u\n"
                      "Firmware MinorV : %u\n"
                      "System Mode     : %s\n",
             ip.c_str(), uuid.c_str(), hostname.c_str(), vendor.c_str(), vendorURL.c_str(),
             (int)typeId, typeStr.c_str(), ranges.c_str(), version.c_str(),
             majorVersion, minorVersion, getFPPmodeStr(systemMode).c_str());
}
