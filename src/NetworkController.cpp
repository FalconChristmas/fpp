/*
*   Falcon Player Daemon
*
*   Copyright (C) 2013-2019 the Falcon Player Developers
*
*   The Falcon Player (FPP) is free software; you can redistribute it
*   and/or modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2 of
*   the License, or (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <regex>

#include "fpp-pch.h"

#include "NetworkController.h"

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
    std::string url = "http://" + ip + "/fppjson.php?command=getSysInfo&simple";
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

    std::regex re("\"css/falcon.css\"|\"/f16v2.js\"");
    std::cmatch m;

    if (!std::regex_search(html.c_str(), m, re))
        return false;

    LogExcess(VB_SYNC, "%s is potentially a Falcon controller, checking further\n", ip.c_str());

    vendor = "Falcon";
    vendorURL = "https://pixelcontroller.com";

    std::string url = "http://" + ip + "/status.xml";
    std::string resp;

    if (urlGet(url, resp)) {
        std::size_t fStart = resp.find("<p>");
        if (fStart != std::string::npos) {
            std::size_t fEnd;
            typeId = (systemType)(atoi(resp.substr(fStart + 3).c_str()) + 0x80);
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
    std::regex re("Controller Model (E[0-9]+)");
    std::cmatch m;

    if (!std::regex_search(html.c_str(), m, re))
        return false;

    LogExcess(VB_SYNC, "%s is potentially a SanDevices controller, checking further\n", ip.c_str());

    vendor = "SanDevices";
    vendorURL = "http://sandevices.com/";
    typeId = kSysTypeSanDevices;
    typeStr = m[1];
    systemMode = BRIDGE_MODE;

    std::regex v4re("Firmware Version:</th></td><td></td><td>([0-9]+.[0-9]+)</td>");
    std::regex v5re("Firmware Version:</th></td><td>([0-9]+.[0-9]+)</td>");

    if ((std::regex_search(html.c_str(), m, v4re)) ||
        (std::regex_search(html.c_str(), m, v5re))) {
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

    std::regex re("\"esps.js\"");
    std::cmatch m;

    if (!std::regex_search(html.c_str(), m, re))
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
    std::regex re("AlphaPix (\\d+|Flex|Evolution)");
    std::regex re2("(\\d+) Port Ethernet to SPI Controller");
    std::cmatch m;

    if ((!std::regex_search(html.c_str(), m, re)) && (!std::regex_search(html.c_str(), m, re2)))
        return false;

    LogExcess(VB_SYNC, "%s is potentially a AlphaPix controller, checking further\n", ip.c_str());

    vendor = "HolidayCoro";
    vendorURL = "https://www.holidaycoro.com/";
    typeId = kSysTypeAlphaPix;
    typeStr = m[1];
    systemMode = BRIDGE_MODE;

    std::regex vre("Currently Installed Firmware Version:  ([0-9]+.[0-9]+)");

    if (std::regex_search(html.c_str(), m, vre)) {
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
    std::regex re("HinksPix Config");
    std::cmatch m;

    if (!std::regex_search(html.c_str(), m, re))
        return false;

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
    std::regex re("DIYLEDExpress E1.31 Bridge Configuration Page");
    std::cmatch m;

    if (!std::regex_search(html.c_str(), m, re))
        return false;

    LogExcess(VB_SYNC, "%s is potentially a DIYLEDExpress controller, checking further\n", ip.c_str());

    vendor = "DIYLEDExpress";
    vendorURL = "http://www.diyledexpress.com/";
    typeId = kSysTypeDIYLEDExpress;
    typeStr = "E1.31 Bridge";
    systemMode = BRIDGE_MODE;

    //Firmware Rev: 4.02
    std::regex vre("Firmware Rev: ([0-9]+.[0-9]+)");

    if (std::regex_search(html.c_str(), m, vre)) {
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
