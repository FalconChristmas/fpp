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

#include "NetworkController.h"
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

NetworkController* NetworkController::DetectControllerViaHTML(const std::string& ip,
                                                              const std::string& html) {
    NetworkController* nc = new NetworkController(ip);

    if (nc->DetectFPP(ip, html)) {
        return nc;
    }

    if (nc->DetectFalconController(ip, html)) {
        return nc;
    }
    if (nc->DetectSanDevicesController(ip, html)) {
        return nc;
    }
    if (nc->DetectESPixelStickController(ip, html)) {
        return nc;
    }
    if (nc->DetectAlphaPixController(ip, html)) {
        return nc;
    }
    if (nc->DetectHinksPixController(ip, html)) {
        return nc;
    }
    if (nc->DetectDIYLEDExpressController(ip, html)) {
        return nc;
    }
    if (nc->DetectWLEDController(ip, html)) {
        return nc;
    }

    delete nc;

    return nullptr;
}
bool NetworkController::DetectFPP(const std::string& ip, const std::string& html) {
    if (html.find("Falcon Player - FPP") == std::string::npos) {
        return false;
    }
    std::string url = "http://" + ip + "/api/system/info&simple=1";
    std::string resp;

    if (urlGet(url, resp)) {
        Json::Value v = LoadJsonFromString(resp);
        hostname = v["HostName"].asString();
        vendor = "FPP";
        vendorURL = "https://falconchristmas.com/forum/";
        if (v.isMember("channelRanges")) {
            ranges = v["channelRanges"].asString();
        }
        if (v.isMember("uuid")) {
            uuid = v["uuid"].asString();
        }
        version = v["Version"].asString();
        typeStr = v["Variant"].asString();
        typeId = MultiSync::ModelStringToType(typeStr);
        if (typeId == kSysTypeFPP) {
            //Pi's tend to have a just the model in the Variant, we'll try mapping those
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
        return true;
    }
    return false;
}

bool NetworkController::DetectFalconController(const std::string& ip,
                                               const std::string& html) {
    LogExcess(VB_SYNC, "Checking if %s is a Falcon controller\n", ip.c_str());

    RegExCache re("\"css/falcon.css\"|\"/f16v2.js\"|\"js/cntrlr_(\\d+).js\"");
    std::cmatch m;

    if (!std::regex_search(html.c_str(), m, *re.regex))
        return false;

    LogExcess(VB_SYNC, "%s is potentially a Falcon controller, checking further\n", ip.c_str());

    vendor = "Falcon";
    vendorURL = "https://pixelcontroller.com";

    std::string url = "http://" + ip + "/status.xml";
    std::string resp;

    if (urlGet(url, resp)) {
        std::size_t fStart = resp.find("<p>");
        if (fStart != std::string::npos) {
            typeId = (systemType)(atoi(getSimpleXMLTag(resp, "p").c_str()));

            if (typeId == kSysTypeFalconController) { //v4 is just 0x80
                if (getSimpleXMLTag(resp, "np") == "16") {
                    typeId = kSysTypeFalconF16v4;
                } else if (getSimpleXMLTag(resp, "np") == "48") {
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
                hostname = getSimpleHTMLTTag(html, "Name:</td>", "\">", "</td>");
                version = getSimpleHTMLTTag(html, "SW Version:</td>", "\">", "</td>");
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

            DumpControllerInfo();

            return true;
        }
    }

    return false;
}

bool NetworkController::DetectSanDevicesController(const std::string& ip,
                                                   const std::string& html) {
    LogExcess(VB_SYNC, "Checking if %s is a SanDevices controller\n", ip.c_str());
    RegExCache re("Controller Model (E[0-9]+)");
    std::cmatch m;

    if (!std::regex_search(html.c_str(), m, *re.regex))
        return false;

    LogExcess(VB_SYNC, "%s is potentially a SanDevices controller, checking further\n", ip.c_str());

    vendor = "SanDevices";
    vendorURL = "http://sandevices.com/";
    typeId = kSysTypeSanDevices;
    typeStr = m[1];
    systemMode = BRIDGE_MODE;

    RegExCache v4re("Firmware Version:</th></td><td></td><td>([0-9]+.[0-9]+)</td>");
    RegExCache v5re("Firmware Version:</th></td><td>\\s?([0-9]+.[0-9]+)(-W\\d+)?</td>");

    if ((std::regex_search(html.c_str(), m, *v4re.regex)) ||
        (std::regex_search(html.c_str(), m, *v5re.regex))) {
        version = m[1];

        if (version != "") {
            majorVersion = atoi(version.c_str());

            std::size_t verDot = version.find(".");
            if (verDot != std::string::npos) {
                minorVersion = atoi(version.substr(verDot + 1).c_str());
            }
        }

        DumpControllerInfo();

        return true;
    }

    return false;
}

bool NetworkController::DetectESPixelStickController(const std::string& ip,
                                                     const std::string& html) {
    LogExcess(VB_SYNC, "Checking if %s is running ESPixelStick firmware\n", ip.c_str());

    if (!contains(html, "\"esps.js\""))
        return false;

    LogExcess(VB_SYNC, "%s is potentially an ESPixelStick, checking further\n", ip.c_str());

    vendor = "ESPixelStick";
    vendorURL = "https://forkineye.com";
    typeId = kSysTypeESPixelStick;
    typeStr = "ESPixelStick";
    systemMode = BRIDGE_MODE;

    std::string url = ip + "/conf";
    std::string resp;

    if (urlGet(url, resp)) {
        Json::Value config = LoadJsonFromString(resp);
        if (config.isMember("network")) {
            if (config["network"].isMember("hostname")) {
                hostname = config["network"]["hostname"].asString();
            }
        }

        if (hostname != "") {
            DumpControllerInfo();

            return true;
        }
    }

    return false;
}

bool NetworkController::DetectAlphaPixController(const std::string& ip, const std::string& html) {
    LogExcess(VB_SYNC, "Checking if %s is a AlphaPix controller\n", ip.c_str());

    RegExCache re("AlphaPix (\\d+|Flex|Evolution)");
    RegExCache re2("(\\d+) Port Ethernet to SPI Controller");
    std::cmatch m;

    if ((!std::regex_search(html.c_str(), m, *re.regex)) && (!std::regex_search(html.c_str(), m, *re2.regex)))
        return false;

    LogExcess(VB_SYNC, "%s is potentially a AlphaPix controller, checking further\n", ip.c_str());

    vendor = "HolidayCoro";
    vendorURL = "https://www.holidaycoro.com/";
    typeId = kSysTypeAlphaPix;
    typeStr = m[1];
    systemMode = BRIDGE_MODE;

    RegExCache vre("Currently Installed Firmware Version:  ([0-9]+.[0-9]+)");

    if (std::regex_search(html.c_str(), m, *vre.regex)) {
        version = m[1];

        if (version != "") {
            majorVersion = atoi(version.c_str());

            std::size_t verDot = version.find(".");
            if (verDot != std::string::npos) {
                minorVersion = atoi(version.substr(verDot + 1).c_str());
            }
        }

        DumpControllerInfo();

        return true;
    }

    return false;
}

bool NetworkController::DetectHinksPixController(const std::string& ip, const std::string& html) {
    LogExcess(VB_SYNC, "Checking if %s is a HinksPix controller\n", ip.c_str());
    if (!contains(html, "HinksPix Config")) {
        return false;
    }
    LogExcess(VB_SYNC, "%s is potentially a HinksPix controller, checking further\n", ip.c_str());

    vendor = "HolidayCoro";
    vendorURL = "https://www.holidaycoro.com/";
    typeId = kSysTypeHinksPix;
    typeStr = "HinksPix";
    systemMode = BRIDGE_MODE;

    DumpControllerInfo();
    return true;
}

bool NetworkController::DetectDIYLEDExpressController(const std::string& ip,
                                                      const std::string& html) {
    LogExcess(VB_SYNC, "Checking if %s is a DIYLEDExpress controller\n", ip.c_str());

    RegExCache re("DIYLEDExpress E1.31 Bridge Configuration Page");
    std::cmatch m;

    if (!std::regex_search(html.c_str(), m, *re.regex))
        return false;

    LogExcess(VB_SYNC, "%s is potentially a DIYLEDExpress controller, checking further\n", ip.c_str());

    vendor = "DIYLEDExpress";
    vendorURL = "http://www.diyledexpress.com/";
    typeId = kSysTypeDIYLEDExpress;
    typeStr = "E1.31 Bridge";
    systemMode = BRIDGE_MODE;

    //Firmware Rev: 4.02
    RegExCache vre("Firmware Rev: ([0-9]+.[0-9]+)");

    if (std::regex_search(html.c_str(), m, *vre.regex)) {
        version = m[1];

        if (version != "") {
            majorVersion = atoi(version.c_str());

            std::size_t verDot = version.find(".");
            if (verDot != std::string::npos) {
                minorVersion = atoi(version.substr(verDot + 1).c_str());
            }
        }

        DumpControllerInfo();

        return true;
    }

    return false;
}

bool NetworkController::DetectWLEDController(const std::string& ip, const std::string& html) {
    if (html.find("WLED UI") == std::string::npos) {
        return false;
    }
    std::string url = "http://" + ip + "/json/info";
    std::string resp;

    if (urlGet(url, resp)) {
        Json::Value v = LoadJsonFromString(resp);

        vendor = "WLED";
        vendorURL = "https://github.com/Aircoookie/WLED";

        version = v["ver"].asString();
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
        return true;
    }
    return false;
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
