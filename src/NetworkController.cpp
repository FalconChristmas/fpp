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

NetworkController::NetworkController(const std::string &ipStr)
  :
    ip(ipStr),
    hostname(ipStr),
    vendor("Unknown"),
    vendorURL("Unknown"),
    typeId(kSysTypeUnknown),
    typeStr("Unknown"),
    ranges("0-0"),
    version("Unknown"),
    majorVersion(0),
    minorVersion(0),
    systemMode(PLAYER_MODE)
{
}

NetworkController *NetworkController::DetectControllerViaHTML(const std::string &ip,
    const std::string &html)
{
    NetworkController *nc = new NetworkController(ip);

    if (nc->DetectFPP(ip, html)) {
        //for now, short circuit if we know it's a FPP instance
        //as we should be able to potentially detect these via
        //multisync
        delete nc;
        return nullptr;
    }
    
    if (nc->DetectFalconController(ip, html))
        return nc;
    if (nc->DetectSanDevicesController(ip, html))
        return nc;
    if (nc->DetectESPixelStickController(ip, html))
        return nc;

    delete nc;

    return nullptr;
}
bool NetworkController::DetectFPP(const std::string &ip, const std::string &html) {
    return html.find("Falcon Player - FPP") != std::string::npos;
}

bool NetworkController::DetectFalconController(const std::string &ip,
    const std::string &html)
{
    LogExcess(VB_SYNC, "Checking if %s is a Falcon controller\n", ip.c_str());

    std::regex re("\"css/falcon.css\"|\"/f16v2.js\"");
    std::cmatch m;

    if (!std::regex_search(html.c_str(), m, re))
        return false;

    LogExcess(VB_SYNC, "%s is potentially a Falcon controller, checking further\n", ip.c_str());

    vendor = "Falcon";
    vendorURL = "https://pixelcontroller.com";

    std::string url = ip + "/status.xml";
    std::string resp;

    if (urlGet(url, resp)) {
        std::size_t fStart = resp.find("<p>");
        if (fStart != std::string::npos) {
            std::size_t fEnd;
            typeId = (systemType)(atoi(resp.substr(fStart + 3).c_str()) + 0x80);
            typeStr = MultiSync::GetTypeString(typeId);

            version = getSimpleXMLTag(resp, "fv");
            systemMode = BRIDGE_MODE;

            if (typeId == kSysTypeFalconF16v2) {
                hostname = getSimpleHTMLTTag(html, "Name:</td>", "\">", "</td>");
                version = getSimpleHTMLTTag(html, "SW Version:</td>", "\">", "</td>");
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

bool NetworkController::DetectSanDevicesController(const std::string &ip,
    const std::string &html)
{
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

bool NetworkController::DetectESPixelStickController(const std::string &ip,
    const std::string &html)
{
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

void NetworkController::DumpControllerInfo(void)
{
    LogDebug(VB_SYNC, "Network Controller Info:\n"
        "IP              : %s\n"
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
        ip.c_str(), hostname.c_str(), vendor.c_str(), vendorURL.c_str(),
        (int)typeId, typeStr.c_str(), ranges.c_str(), version.c_str(),
        majorVersion, minorVersion, getFPPmodeStr(systemMode).c_str());
}

