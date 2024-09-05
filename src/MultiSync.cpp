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

#include <arpa/inet.h>
#include <curl/curl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <ctype.h>
#include <errno.h>
#include <functional>
#include <ifaddrs.h>
#include <list>
#include <map>
#include <math.h>
#include <memory>
#include <mutex>
#include <netdb.h>
#include <set>
#include <stdio.h>
#include <string.h>
#include <string>
#include <strings.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#include "NetworkController.h"
#include "NetworkMonitor.h"
#include "Player.h"
#include "Plugins.h"
#include "Sequence.h"
#include "command.h"
#include "common.h"
#include "falcon.h"
#include "fppversion.h"
#include "log.h"
#include "settings.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "channeloutput/channeloutputthread.h"
#include "commands/Commands.h"
#include "mediaoutput/MediaOutputBase.h"
#include "mediaoutput/mediaoutput.h"

#include "MultiSync.h"

MultiSync MultiSync::INSTANCE;
MultiSync* multiSync = &MultiSync::INSTANCE;

static const char* MULTISYNC_MULTICAST_ADDRESS = "239.70.80.80"; // 239.F.P.P
static uint32_t MULTISYNC_MULTICAST_ADD = inet_addr(MULTISYNC_MULTICAST_ADDRESS);

NetInterfaceInfo::NetInterfaceInfo() :
    address(0),
    broadcastAddress(0),
    multicastSocket(-1) {
}
NetInterfaceInfo::~NetInterfaceInfo() {
    if (multicastSocket != -1) {
        LogDebug(VB_SYNC, "Closing multicast socket for %s\n", interfaceName.c_str());
        close(multicastSocket);
    }
}

static bool GetIPForHost(std::string& target) {
    struct hostent* uhost = gethostbyname(target.c_str());
    if (uhost) {
        struct in_addr add = *((struct in_addr*)uhost->h_addr);
        target = inet_ntoa(add);
        return true;
    }
    return false;
}

void MultiSyncSystem::update(MultiSyncSystemType type,
                             unsigned int majorVersion, unsigned int minorVersion,
                             FPPMode fppMode,
                             const std::string& address,
                             const std::string& hostname,
                             const std::string& version,
                             const std::string& model,
                             const std::string& ranges,
                             const std::string& uuid,
                             const bool multiSync,
                             const bool sendingMultiSync) {
    // Always update uuid if not Unknown
    if (uuid != "Unknown" && uuid != "") {
        this->uuid = uuid;
    }

    // If this record is from info learned via the MultiSync protocol,
    // don't allow it to be overwritten by data discovered elsewhere
    // except for uuid.
    if (this->multiSync && !multiSync)
        return;

    this->fppMode = fppMode;
    this->sendingMultiSync = sendingMultiSync;
    if (fppMode == MASTER_MODE) {
        this->fppMode = PLAYER_MODE;
        this->sendingMultiSync = true;
    }

    this->type = type;
    this->majorVersion = majorVersion;
    this->minorVersion = minorVersion;
    this->address = address;
    this->hostname = hostname;
    this->version = version;
    this->model = model;
    this->ranges = ranges;

    if (!this->multiSync && multiSync)
        this->multiSync = true;

    std::vector<std::string> parts = split(address, '.');
    if (parts.size() != 4) {
        if (!GetIPForHost(this->address)) {
            LogErr(VB_SYNC, "Error looking up hostname: %s\n", address.c_str());
        } else {
            parts = split(this->address, '.');
        }
    }
    if (parts.size() >= 4) {
        this->ipa = atoi(parts[0].c_str());
        this->ipb = atoi(parts[1].c_str());
        this->ipc = atoi(parts[2].c_str());
        this->ipd = atoi(parts[3].c_str());
    } else {
        this->ipa = 0;
        this->ipb = 0;
        this->ipc = 0;
        this->ipd = 0;
    }
}

Json::Value MultiSyncSystem::toJSON(bool local, bool timestamps) {
    Json::Value system;

    system["type"] = MultiSync::GetTypeString(type, local);
    system["typeId"] = (int)type;
    if (timestamps) {
        system["lastSeen"] = (Json::UInt64)lastSeen;
        system["lastSeenStr"] = lastSeenStr;
    }
    system["majorVersion"] = majorVersion;
    system["minorVersion"] = minorVersion;
    system["uuid"] = uuid;
    system["fppMode"] = fppMode;
    system["multisync"] = sendingMultiSync;

    char* s = modeToString(fppMode);
    system["fppModeString"] = s;
    free(s);

    system["address"] = address;
    system["hostname"] = hostname;
    system["version"] = version;
    system["model"] = model;
    system["channelRanges"] = ranges;

    system["local"] = local ? 1 : 0;

    system["multiSyncCapable"] = multiSync ? 1 : 0;

    if (local) {
        system["HostDescription"] = getSetting("HostDescription");
    }

    return system;
}

/*
 *
 */
MultiSync::MultiSync() :
    m_broadcastSock(-1),
    m_controlSock(-1),
    m_receiveSock(-1),
    m_lastMediaHalfSecond(0),
    m_remoteOffset(0.0),
    m_lastPingTime(0),
    m_lastCheckTime(0),
    m_lastFrame(0),
    m_sendMulticast(false),
    m_sendBroadcast(false) {
    memset(rcvBuffers, 0, sizeof(rcvBuffers));
    memset(rcvCmbuf, 0, sizeof(rcvCmbuf));
}

/*
 *
 */
MultiSync::~MultiSync() {
    ShutdownSync();
    for (auto& a : m_syncStats) {
        MultiSyncStats* stats = (MultiSyncStats*)a.second;
        delete stats;
    }
    m_syncStats.clear();
}

/*
 *
 */
int MultiSync::Init(void) {
    if (getFPPmode() == REMOTE_MODE) {
        m_multiSyncEnabled = false;
    } else {
        m_multiSyncEnabled = getSettingInt("MultiSyncEnabled", 0);
    }

    FillInInterfaces();
    FillLocalSystemInfo();

    if (!OpenReceiveSocket())
        return 0;

    if (!OpenBroadcastSocket())
        return 0;

    if (m_multiSyncEnabled) {
        if (!OpenControlSockets())
            return 0;
    }

    std::function<void(NetworkMonitor::NetEventType i, int up, const std::string&)> f = [this](NetworkMonitor::NetEventType i, int up, const std::string& name) {
        LogDebug(VB_SYNC, "MultiSync::NetworkChanged - Interface: %s   Up: %d   Msg: %d\n", name.c_str(), up, i);
        if (i == NetworkMonitor::NetEventType::DEL_ADDR && !up) {
            RemoveInterface(name);
        } else if (i == NetworkMonitor::NetEventType::NEW_ADDR && up) {
            bool changed = FillInInterfaces();
            if (changed) {
                FillLocalSystemInfo();
                Ping(0, false);
            }
            setupMulticastReceive();
        }
    };
    NetworkMonitor::INSTANCE.registerCallback(f);

    return 1;
}

/*
 *
 */
void MultiSync::UpdateSystem(MultiSyncSystemType type,
                             unsigned int majorVersion, unsigned int minorVersion,
                             FPPMode fppMode,
                             const std::string& address,
                             const std::string& hostname,
                             const std::string& version,
                             const std::string& model,
                             const std::string& ranges,
                             const std::string& uuid,
                             const bool multiSync,
                             const bool sendingMultiSync) {
    LogDebug(VB_SYNC, "UpdateSystem(%d, %u, %u, %d, '%s', '%s', '%s', '%s', '%s', '%s', %s)\n",
             (int)type, majorVersion, minorVersion, (int)fppMode,
             address.c_str(), hostname.c_str(), version.c_str(), model.c_str(),
             ranges.c_str(), uuid.c_str(), multiSync ? "true" : "false");

    char timeStr[34];
    memset(timeStr, 0, sizeof(timeStr));
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    snprintf(timeStr, sizeof(timeStr), "%4d-%.2d-%.2d %.2d:%.2d:%.2d",
             1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    std::string ipForAddress = address;
    GetIPForHost(ipForAddress);

    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
    bool found = false;
    for (auto& sys : m_remoteSystems) {
        if ((address == sys.address || ipForAddress == sys.address) &&
            ((hostname == sys.hostname) ||
             (address == sys.hostname) ||
             (hostname == sys.address))) {
            found = true;
            sys.update(type, majorVersion, minorVersion, fppMode, address, hostname, version, model, ranges, uuid, multiSync, sendingMultiSync);
            sys.lastSeenStr = timeStr;
            sys.lastSeen = t;
        }
    }
    for (auto& sys : m_localSystems) {
        if ((address == sys.address) &&
            (hostname == sys.hostname)) {
            found = true;
            sys.update(type, majorVersion, minorVersion, fppMode, address, hostname, version, model, ranges, uuid, multiSync, sendingMultiSync);
            sys.lastSeen = t;
            sys.lastSeenStr = timeStr;
        }
    }
    if (!found) {
        MultiSyncSystem sys;
        sys.update(type, majorVersion, minorVersion, fppMode, address, hostname, version, model, ranges, uuid, multiSync, sendingMultiSync);
        sys.lastSeenStr = timeStr;
        sys.lastSeen = t;
        m_remoteSystems.push_back(sys);
    }
}

/*
 *
 */
MultiSyncSystemType MultiSync::ModelStringToType(std::string model) {
    if (startsWith(model, "Raspberry Pi Model A Rev") || (model == "Raspberry Pi Model A"))
        return kSysTypeFPPRaspberryPiA;
    if (startsWith(model, "Raspberry Pi Model B Rev") || (model == "Raspberry Pi Model B"))
        return kSysTypeFPPRaspberryPiB;
    if (startsWith(model, "Raspberry Pi Model A Plus") || (model == "Raspberry Pi Model A+"))
        return kSysTypeFPPRaspberryPiAPlus;
    if (startsWith(model, "Raspberry Pi Model B Plus") || (model == "Raspberry Pi Model B+"))
        return kSysTypeFPPRaspberryPiBPlus;
    if ((startsWith(model, "Raspberry Pi 2 Model B 1.1")) ||
        (startsWith(model, "Raspberry Pi 2 Model B 1.0")))
        return kSysTypeFPPRaspberryPi2B;
    if (startsWith(model, "Raspberry Pi 2 Model B"))
        return kSysTypeFPPRaspberryPi2BNew;
    if (startsWith(model, "Raspberry Pi 3 Model B Rev") || (model == "Raspberry Pi 3 Model B"))
        return kSysTypeFPPRaspberryPi3B;
    if (startsWith(model, "Raspberry Pi 3 Model B Plus") || (model == "Raspberry Pi 3 Model B+"))
        return kSysTypeFPPRaspberryPi3BPlus;
    if (startsWith(model, "Raspberry Pi Zero Rev"))
        return kSysTypeFPPRaspberryPiZero;
    if (startsWith(model, "Raspberry Pi Zero W"))
        return kSysTypeFPPRaspberryPiZeroW;
    if (startsWith(model, "Raspberry Pi Zero 2 W"))
        return kSysTypeFPPRaspberryPiZero2W;
    if (startsWith(model, "Raspberry Pi 3 Model A Plus") || (model == "Raspberry Pi 3 Model A+"))
        return kSysTypeFPPRaspberryPi3APlus;
    if (startsWith(model, "Raspberry Pi 4") || startsWith(model, "Raspberry Pi Compute Module 4"))
        return kSysTypeFPPRaspberryPi4;
    if (startsWith(model, "Raspberry Pi 5"))
        return kSysTypeFPPRaspberryPi5;
    if (startsWith(model, "SanCloud BeagleBone Enhanced"))
        return kSysTypeFPPSanCloudBeagleBoneEnhanced;
    if (contains(model, "BeagleBone Black")) {
        if (contains(model, "Wireless")) {
            return kSysTypeFPPBeagleBoneBlackWireless;
        }
        return kSysTypeFPPBeagleBoneBlack;
    }
    if (contains(model, "BeagleBone Green")) {
        if (contains(model, "Wireless")) {
            return kSysTypeFPPBeagleBoneGreenWireless;
        }
        return kSysTypeFPPBeagleBoneGreen;
    }
    if (contains(model, "PocketBeagle")) {
        return kSysTypeFPPPocketBeagle;
    }
    if (model == "MacOS") {
        return kSysTypeMacOS;
    }
    if (model == "Armbian") {
        return kSysTypeFPPArmbian;
    }
    // FIXME, fill in the rest of the types

    return kSysTypeFPP;
}

/*
 *
 */
bool MultiSync::FillLocalSystemInfo(void) {
    MultiSyncSystem newSystem;

    std::string model = GetHardwareModel();
#ifdef PLATFORM_ARMBIAN
    MultiSyncSystemType type = ModelStringToType("Armbian");
#else
    MultiSyncSystemType type = ModelStringToType(model);
#endif

    std::string multiSyncInterface = getSetting("MultiSyncInterface");
    char addressBuf[128];
    std::list<std::string> addresses;

    std::string dockerAddress;
    if (FileExists("/.dockerenv")) {
        if (getenv("FPP_DOCKER_IP")) {
            dockerAddress = getenv("FPP_DOCKER_IP");
            addresses.push_back(dockerAddress);
        }

        std::string a = getSetting("MultiSyncExternalIPAddress");
        if (a != "" && a != dockerAddress) {
            addresses.push_back(a);
            dockerAddress = a;
        }
    }

    if (multiSyncInterface == "" && dockerAddress == "") {
        // get all the addresses
        struct ifaddrs *interfaces, *tmp;
        getifaddrs(&interfaces);
        tmp = interfaces;
        while (tmp) {
            if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
                if (strncmp("usb", tmp->ifa_name, 3) != 0) {
                    // skip the usb* interfaces as we won't support multisync on those
                    memset(addressBuf, 0, sizeof(addressBuf));

                    struct sockaddr_in* sa = (struct sockaddr_in*)(tmp->ifa_addr);
                    strcpy(addressBuf, inet_ntoa(sa->sin_addr));
                    if (isSupportedForMultisync(addressBuf, tmp->ifa_name)) {
                        addresses.push_back(addressBuf);
                    }
                }
            } else if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
                // FIXME for ipv6 multisync
            }
            tmp = tmp->ifa_next;
        }
        freeifaddrs(interfaces);
    } else if (dockerAddress == "") {
        memset(addressBuf, 0, sizeof(addressBuf));
        GetInterfaceAddress(multiSyncInterface.c_str(), addressBuf, NULL, NULL);
        addresses.push_back(addressBuf);
    }

    if (m_hostname == "") {
        m_hostname = getSetting("HostName");
    }

    if (m_hostname == "") {
        m_hostname = "FPP";
    }

    newSystem.lastSeen = (unsigned long)time(NULL);
    newSystem.type = type;
    newSystem.majorVersion = atoi(getFPPMajorVersion());
    newSystem.minorVersion = atoi(getFPPMinorVersion());
    newSystem.hostname = m_hostname;
    newSystem.fppMode = getFPPmode();
    newSystem.sendingMultiSync = m_multiSyncEnabled;
    newSystem.version = getFPPVersion();
    newSystem.model = model;
    newSystem.ipa = 0;
    newSystem.ipb = 0;
    newSystem.ipc = 0;
    newSystem.ipd = 0;
    newSystem.uuid = getSetting("SystemUUID");

    LogDebug(VB_SYNC, "Host name: %s\n", newSystem.hostname.c_str());
    LogDebug(VB_SYNC, "Version: %s\n", newSystem.version.c_str());
    LogDebug(VB_SYNC, "Model: %s\n", newSystem.model.c_str());

    bool changed = false;
    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);

    for (auto address : addresses) {
        bool found = false;
        for (auto& sys : m_localSystems) {
            if (sys.address == address) {
                found = true;
            }
        }
        if (!found) {
            LogDebug(VB_SYNC, "Adding Local System Address: %s\n", address.c_str());
            changed = true;
            newSystem.address = address;
            std::vector<std::string> parts = split(newSystem.address, '.');
            newSystem.ipa = atoi(parts[0].c_str());
            newSystem.ipb = atoi(parts[1].c_str());
            newSystem.ipc = atoi(parts[2].c_str());
            newSystem.ipd = atoi(parts[3].c_str());
            m_localSystems.push_back(newSystem);
        }
    }
    return changed;
}

/*
 *
 */
std::string MultiSync::GetHardwareModel(void) {
#ifdef PLATFORM_OSX
    return "MacOS";
#else
    std::string result;
    std::string filename;

    if (FileExists("/sys/firmware/devicetree/base/model"))
        filename = "/sys/firmware/devicetree/base/model";
    else if (FileExists("/sys/class/dmi/id/product_name"))
        filename = "/sys/class/dmi/id/product_name";

    if (filename != "") {
        char buf[128];
        FILE* fd = fopen(filename.c_str(), "r");
        if (fd) {
            if (fgets(buf, 127, fd))
                result = buf;
            else
                result = "Unknown Hardware Platform";
            fclose(fd);
        } else {
            result = "Unknown Hardware Platform";
        }
    } else {
        result = "Unknown Hardware Platform";
    }

    if (endsWith(result, "\n")) {
        replaceAll(result, "\n", "");
    }

    TrimWhiteSpace(result);

    return result;
#endif
}

/*
 *
 */
std::string MultiSync::GetTypeString(MultiSyncSystemType type, bool local) {
    if (local && type == kSysTypeFPP) {
        // unknown hardware, but we can figure out the OS version
        if (FileExists("/etc/os-release")) {
            std::string file = GetFileContents("/etc/os-release");
            std::vector<std::string> lines = split(file, '\n');
            std::map<std::string, std::string> values;
            for (auto& str : lines) {
                size_t pos = str.find("=");
                if (pos != std::string::npos) {
                    std::string key = str.substr(0, pos);
                    std::string val = str.substr(pos + 1);
                    if (val[0] == '"') {
                        val = val.substr(1, val.length() - 2);
                    }
                    TrimWhiteSpace(key);
                    TrimWhiteSpace(val);
                    values[key] = val;
                }
            }
            if (values["NAME"] != "") {
                return "FPP (" + values["NAME"] + ")";
            }
            if (values["PRETTY_NAME"] != "") {
                return "FPP (" + values["PRETTY_NAME"] + ")";
            }
        }
        return "FPP (unknown hardware)";
    }
    switch (type) {
    case kSysTypeUnknown:
        return "Unknown System Type";
    case kSysTypeFPP:
        return "FPP (unknown hardware)";
    case kSysTypeFPPRaspberryPiA:
        return "Raspberry Pi A";
    case kSysTypeFPPRaspberryPiB:
        return "Raspberry Pi B";
    case kSysTypeFPPRaspberryPiAPlus:
        return "Raspberry Pi A Plus";
    case kSysTypeFPPRaspberryPiBPlus:
        return "Raspberry Pi B+";
    case kSysTypeFPPRaspberryPi2B:
        return "Raspberry Pi 2 B";
    case kSysTypeFPPRaspberryPi2BNew:
        return "Raspberry Pi 2 B v1.2+";
    case kSysTypeFPPRaspberryPi3B:
        return "Raspberry Pi 3 B";
    case kSysTypeFPPRaspberryPi3BPlus:
        return "Raspberry Pi 3 B+";
    case kSysTypeFPPRaspberryPiZero:
        return "Raspberry Pi Zero";
    case kSysTypeFPPRaspberryPiZeroW:
        return "Raspberry Pi Zero W";
    case kSysTypeFPPRaspberryPiZero2W:
        return "Raspberry Pi Zero 2 W";
    case kSysTypeFPPRaspberryPi3APlus:
        return "Raspberry Pi 3 A+";
    case kSysTypeFPPRaspberryPi4:
        return "Raspberry Pi 4";
    case kSysTypeFPPRaspberryPi5:
        return "Raspberry Pi 5";
    case kSysTypeFalconController:
        return "Falcon Controller";
    case kSysTypeFalconF16v2:
        return "Falcon F16v2";
    case kSysTypeFalconF4v2_64Mb:
        return "Falcon F4v2_64Mb";
    case kSysTypeFalconF16v2R:
        return "Falcon F16v2R";
    case kSysTypeFalconF4v2:
        return "Falcon F4v2";
    case kSysTypeFalconF4v3:
        return "Falcon F4v3";
    case kSysTypeFalconF16v3:
        return "Falcon F16v3";
    case kSysTypeFalconF48:
        return "Falcon F48";
    case kSysTypeFalconF16v4:
        return "Falcon F16v4";
    case kSysTypeFalconF48v4:
        return "Falcon F48v4";
    case kSysTypeFalconF16v5:
        return "Falcon F16v5";
    case kSysTypeExperienceGP16:
        return "Genius Pixel 16";
    case kSysTypeExperienceGP8:
        return "Genius Pixel 8";
    case kSysTypeExperienceGLR:
        return "Genius Long Range";
    case kSysTypeExperienceG16Pro:
        return "Genius PRO 16";
    case kSysTypeExperienceG32Pro:
        return "Genius PRO 32";
    case kSysTypeExperienceGenius:
        return "Genius Controller";
    case kSysTypeOtherSystem:
        return "Other Unknown System";
    case kSysTypeFPPBeagleBoneBlack:
        return "BeagleBone Black";
    case kSysTypeFPPBeagleBoneBlackWireless:
        return "BeagleBone Black Wireless";
    case kSysTypeFPPBeagleBoneGreen:
        return "BeagleBone Green";
    case kSysTypeFPPBeagleBoneGreenWireless:
        return "BeagleBone Green Wireless";
    case kSysTypeFPPPocketBeagle:
        return "PocketBeagle";
    case kSysTypeFPPSanCloudBeagleBoneEnhanced:
        return "SanCloud BeagleBone Enhanced";

    case kSysTypexSchedule:
        return "xSchedule";
    case kSysTypeESPixelStick:
        return "ESPixelStick-ESP8266";
    case kSysTypeESPixelStickESP32:
        return "ESPixelStick-ESP32";
    case kSysTypeMacOS:
        return "MacOS";
    case kSysTypeFPPArmbian:
        return "Armbian";
    case kSysTypeSanDevices:
        return "SanDevices";
    case kSysTypeAlphaPix:
        return "AlphaPix";
    case kSysTypeHinksPix:
        return "HinksPix";
    case kSysTypeDIYLEDExpress:
        return "DIYLEDExpress";
    case kSysTypeWLED:
        return "WLED";
    default:
        return "Unknown System Type";
    }
}

static std::string createRanges(std::vector<std::pair<uint32_t, uint32_t>> ranges, int limit) {
    bool first = true;
    std::string range("");
    char buf[64];
    memset(buf, 0, sizeof(buf));
    for (auto& a : ranges) {
        if (!first) {
            range += ",";
        }
        snprintf(buf, sizeof(buf), "%d-%d", a.first, (a.first + a.second - 1));
        range += buf;
        first = false;
    }
    while (range.size() > limit) {
        // range won't fit within the space in the Ping packet, we need to shrink the range
        //  we'll find the smallest gap and combine into a larger range
        int minGap = 9999999;
        int minIdx = -1;
        int last = ranges[0].first + ranges[1].second - 1;
        for (int x = 1; x < ranges.size(); x++) {
            int gap = ranges[x].first - last;
            if (gap < minGap) {
                minIdx = x;
                minGap = gap;
            }
            last = ranges[x].first + ranges[x].second - 1;
        }
        if (minIdx) {
            int newLast = ranges[minIdx].first + ranges[minIdx].second;
            ranges[minIdx - 1].second = newLast - ranges[minIdx - 1].first;
            ranges.erase(ranges.begin() + minIdx);
        }
        range = createRanges(ranges, 999999);
    }
    return range;
}

Json::Value MultiSync::GetSystems(bool localOnly, bool timestamps) {
    Json::Value result;
    Json::Value systems(Json::arrayValue);

    const std::vector<std::pair<uint32_t, uint32_t>>& ranges = GetOutputRanges(true);
    std::string range = createRanges(ranges, 999999);

    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
    for (auto& sys : m_localSystems) {
        sys.ranges = range;
        systems.append(sys.toJSON(true, timestamps));
    }
    if (!localOnly) {
        for (auto& sys : m_remoteSystems) {
            systems.append(sys.toJSON(false, timestamps));
        }
    }
    result["systems"] = systems;
    return result;
}

Json::Value MultiSync::GetSyncStats() {
    Json::Value result;
    Json::Value systems(Json::arrayValue);

    std::unique_lock<std::recursive_mutex> slock(m_statsLock);
    for (auto& a : m_syncStats) {
        MultiSyncStats* stats = (MultiSyncStats*)a.second;
        systems.append(stats->toJSON());
    }
    slock.unlock();

    result["systems"] = systems;

    std::string masterHostname;

    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
    for (auto& sys : m_remoteSystems) {
        if (sys.address == m_syncMaster)
            masterHostname = sys.hostname;
    }
    lock.unlock();

    result["masterIP"] = m_syncMaster;
    result["masterHostname"] = masterHostname;

    return result;
}

void MultiSync::ResetSyncStats() {
    std::unique_lock<std::recursive_mutex> slock(m_statsLock);
    for (auto& a : m_syncStats) {
        MultiSyncStats* stats = (MultiSyncStats*)a.second;
        delete stats;
    }

    m_syncStats.clear();
}

void MultiSync::Discover() {
    Ping(1);
    PerformHTTPDiscovery();
}

void MultiSync::PerformHTTPDiscovery() {
    std::string subnetsStr = getSetting("MultiSyncHTTPSubnets");

    Json::Value outputs = LoadJsonFromFile(FPP_DIR_CONFIG("/co-universes.json"));
    if (outputs.isMember("channelOutputs")) {
        for (int co = 0; co < outputs["channelOutputs"].size(); co++) {
            if (outputs["channelOutputs"][co].isMember("universes")) {
                for (int i = 0; i < outputs["channelOutputs"][co]["universes"].size(); i++) {
                    if (outputs["channelOutputs"][co]["universes"][i].isMember("address")) {
                        std::string ip = outputs["channelOutputs"][co]["universes"][i]["address"].asString();
                        if (subnetsStr != "") {
                            subnetsStr += ",";
                        }
                        subnetsStr += ip;
                    }
                }
            }
        }
    }

    if (subnetsStr != "") {
        std::vector<std::string> tokens = split(subnetsStr, ',');
        std::set<std::string> subnets;
        std::set<std::string> exacts;
        for (std::string token : tokens) {
            TrimWhiteSpace(token);
            if (token != "") {
                size_t pos = token.find('[');
                size_t posSlash = token.find('/');
                if (pos != std::string::npos) {
                    std::string rng = token.substr(pos + 1);
                    token = token.substr(0, pos);
                    size_t pos2 = rng.find(']');
                    std::string postfix = "";
                    if (pos2 != (rng.size() - 1)) {
                        postfix = rng.substr(pos2 + 1);
                    }
                    std::vector<std::string> r1 = split(rng, ',');
                    for (const auto& a : r1) {
                        std::vector<std::string> r = split(a, '-');
                        int start = atoi(r[0].c_str());
                        int len = 1;
                        if (r.size() == 2) {
                            len = atoi(r[1].c_str()) - start + 1;
                        }

                        for (int x = start; x < (start + len); x++) {
                            subnets.insert(token + std::to_string(x) + postfix);
                        }
                    }
                } else if (posSlash != std::string::npos) {
                    std::vector<std::string> parts = split(token, '/');
                    int prefix = 32;
                    if (parts.size() == 2) {
                        prefix = atoi(parts[1].c_str());
                    }
                    int ips = (int)(powl(2, 32 - prefix));
                    unsigned long firstIP = htonl(inet_addr(parts[0].c_str()));
                    // If prefix is a /31 or /32, we scan all IPs, otherwise skip network
                    // and broadcast
                    if (prefix <= 30) {
                        ips -= 2;
                        firstIP++;
                    }
                    struct in_addr ia;
                    char ip[16];
                    char url[24];
                    for (int i = 0; i < ips; i++) {
                        ia.s_addr = ntohl(firstIP + i);
                        strcpy(ip, inet_ntoa(ia));
                        subnets.insert(ip);
                    }
                } else {
                    std::string address2 = token;
                    GetIPForHost(address2);
                    if (isSupportedForMultisync(address2.c_str(), "")) {
                        subnets.insert(token);
                        exacts.insert(token);
                    }
                }
            }
        }
        if (!subnets.empty()) {
            DiscoverViaHTTP(subnets, exacts);
        }
    }
}

static size_t curl_write_data(void* ptr, size_t size, size_t nmemb, void* ourpointer) {
    LogExcess(VB_SYNC, "write_data(%p, %d, %d, %p)\n", ptr, size, nmemb, ourpointer);
    multiSync->StoreHTTPResponse((std::string*)ourpointer, (uint8_t*)ptr, size * nmemb);

    return size * nmemb;
}

void MultiSync::StoreHTTPResponse(std::string* ipp, uint8_t* data, int sz) {
    std::string ip = *ipp;
    std::unique_lock<std::mutex> lock(m_httpResponsesLock);

    int pos = m_httpResponses[ip].size();
    m_httpResponses[ip].resize(m_httpResponses[ip].size() + sz);
    memcpy(&m_httpResponses[ip][pos], data, sz);
}

void MultiSync::DiscoverIPViaHTTP(const std::string& ip, bool allowUnknown) {
    LogDebug(VB_SYNC, "Checking HTTP response from %s\n", ip.c_str());

    std::unique_lock<std::mutex> lock(m_httpResponsesLock);
    auto search = m_httpResponses.find(ip);
    if (search == m_httpResponses.end()) {
        LogErr(VB_SYNC, "Error, no value in m_httpResponses for %s IP\n", ip.c_str());
        return;
    }
    /*
    // if you need to debug thing, uncomment this.  Any \r in the string
    // will likely make a printf("%s") not work as each "line" will overwrite itself
    for (int x = 0; x < search->second.size(); x++) {
        if (search->second[x] == '\n' || search->second[x] == '\r') {
            search->second[x] = ' ';
        }
    }
    */
    std::string data((char*)&search->second[0], search->second.size());

    // determine if the ip is on the local subnet.
    // right now it assumes a /24 subnet, not ideal
    bool isLocalSubnet = false;
    in_addr_t add = inet_addr(ip.c_str());
    unsigned char ipd = (add >> 24) & 0xFF;
    unsigned char ipc = (add >> 16) & 0xFF;
    unsigned char ipb = (add >> 8) & 0xFF;
    unsigned char ipa = add & 0xFF;
    std::unique_lock<std::recursive_mutex> slock(m_systemsLock);
    for (auto& a : m_localSystems) {
        if (ipa == a.ipa && ipb == a.ipb & ipc == a.ipc) {
            isLocalSubnet = true;
        }
    }

    if (data.size()) {
        std::string d = data;
        if (d.size() > 500) {
            d = d.substr(0, 500);
        }
        LogExcess(VB_SYNC, "IP: %s    Resp: %s\n", ip.c_str(), d.c_str());
    }

    NetworkController* nc = nullptr;

    struct hostent* uhost = gethostbyname(ip.c_str());
    std::string address2 = ip;
    GetIPForHost(address2);

    if (isSupportedForMultisync(ip.c_str(), "") && isSupportedForMultisync(address2.c_str(), "")) {
        nc = NetworkController::DetectControllerViaHTML(ip, data);
    }

    if (nc) {
        // This block was designed to avoid updating from NetworkControl if
        // the device was found by ping.  However, UUID doesn't come from ping
        // and the NC update has already be executed, so removing it out for now
        // so that uuid gets updated.  update() function already has
        // smarts not to override discovery protoocol data anyway.

        /*
        if (isLocalSubnet && nc->typeId < 0x80) {
            std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
            bool found = false;
            for (auto & sys : m_remoteSystems) {
                if ((nc->ip == sys.address) &&
                    ((nc->hostname == sys.hostname) ||
                     (nc->ip == sys.hostname) ||
                     (nc->hostname == sys.address))) {
                    // we already found this via normal multicast discovery, ignore
                    found = true;
                }
            }
            if (found) {
                delete nc;
                nc = nullptr;
            }
        }
        */

        if (nc) {
            UpdateSystem(nc->typeId, nc->majorVersion, nc->minorVersion,
                         nc->systemMode, nc->ip, nc->hostname, nc->version,
                         nc->typeStr, nc->ranges, nc->uuid, false, nc->sendingMultiSync);
            delete nc;
        }
    } else if (allowUnknown) {
        UpdateSystem(kSysTypeUnknown, 0, 0, UNKNOWN_MODE, ip, ip, "Unknown", "Unknown", "0-0", "Unknown", false, false);
    }
}

void MultiSync::DiscoverViaHTTP(const std::set<std::string>& ipSet, const std::set<std::string>& exacts) {
    std::vector<CURL*> handles;
    std::vector<std::string> ipList;
    handles.resize(ipSet.size());
    ipList.resize(ipSet.size());
    CURLM* multi_handle;
    CURLMsg* msg;
    int still_running = 0;
    int msgs_left;

    std::string userAgent = "FPP/";
    userAgent += getFPPVersionTriplet();

    multi_handle = curl_multi_init();
    int ips = 0;
    char url[24];
    for (auto& ip : ipSet) {
        LogExcess(VB_SYNC, "  %s\n", ip.c_str());
        handles[ips] = curl_easy_init();
        ipList[ips] = ip;
        m_httpResponses.erase(ip);

        snprintf(url, sizeof(url), "http://%s/", ip.c_str());
        curl_easy_setopt(handles[ips], CURLOPT_URL, url);
        curl_easy_setopt(handles[ips], CURLOPT_CONNECTTIMEOUT_MS, 1000L);
        curl_easy_setopt(handles[ips], CURLOPT_TIMEOUT_MS, 5000L);
        curl_easy_setopt(handles[ips], CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handles[ips], CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(handles[ips], CURLOPT_PRIVATE, &ipList[ips]);
        curl_easy_setopt(handles[ips], CURLOPT_WRITEFUNCTION, curl_write_data);
        curl_easy_setopt(handles[ips], CURLOPT_WRITEDATA, &ipList[ips]);
        curl_easy_setopt(handles[ips], CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(handles[ips], CURLOPT_TCP_FASTOPEN, 1L);
        curl_easy_setopt(handles[ips], CURLOPT_HTTP09_ALLOWED, 1L);
        curl_easy_setopt(handles[ips], CURLOPT_NOSIGNAL, 1);

        curl_multi_add_handle(multi_handle, handles[ips]);
        if ((ips % 10) == 0) {
            // periodically need to do a perform so DNS can work, otherwise
            // it seems to max out at aroung 70 or 80
            curl_multi_perform(multi_handle, &still_running);
        }
        ips++;
    }

    int start = handles.size();
    curl_multi_perform(multi_handle, &still_running);
    while (still_running || start != still_running) {
        if (start != still_running) {
            int msgq = 0;
            while ((msg = curl_multi_info_read(multi_handle, &msgq))) {
                if (msg->msg == CURLMSG_DONE) {
                    CURL* e = msg->easy_handle;
                    int idx = -1;
                    for (idx = 0; idx < ips; idx++) {
                        if (e == handles[idx]) {
                            break;
                        }
                    }
                    if (msg->data.result == CURLE_OK || msg->data.result == 0) {
                        long responseCode = 0;
                        curl_easy_getinfo(e, CURLINFO_HTTP_CODE, &responseCode);
                        if (responseCode == 200 || (msg->data.result == 0 && responseCode == 0)) {
                            LogDebug(VB_SYNC, "IP index %d (%s) completed with %d status, code: %d\n", idx, ipList[idx].c_str(), msg->data.result, responseCode);
                        } else {
                            LogDebug(VB_SYNC, "Error response from %s.  ResponseCode: %d\n", ipList[idx].c_str(), responseCode);
                            ipList[idx] = "";
                        }
                    } else {
                        LogDebug(VB_SYNC, "No/Error response from %s.  Response code: %d\n", ipList[idx].c_str(), msg->data.result);
                        ipList[idx] = "";
                    }
                    curl_multi_remove_handle(multi_handle, e);
                    curl_easy_cleanup(e);
                    handles[idx] = nullptr;
                }
            }
            start = still_running;
        }
        int numfds = 0;
        // process ping packets in the loop as well
        ProcessControlPacket(true);
        int res = curl_multi_wait(multi_handle, NULL, 0, 100, &numfds);
        if (res != CURLM_OK) {
            LogErr(VB_SYNC, "error: curl_multi_wait() returned %d\n", res);
            return;
        }
        curl_multi_perform(multi_handle, &still_running);
    }

    for (int idx = 0; idx < ips; idx++) {
        if (ipList[idx] != "") {
            bool exact = exacts.find(ipList[idx]) != exacts.end();
            DiscoverIPViaHTTP(ipList[idx], exact);
        }
    }
    for (int i = 0; i < ips; i++) {
        if (handles[i]) {
            curl_multi_remove_handle(multi_handle, handles[i]);
            curl_easy_cleanup(handles[i]);
        }
    }
    curl_multi_cleanup(multi_handle);
}

/*
 *
 */
void MultiSync::Ping(int discover, bool broadcast) {
    LogDebug(VB_SYNC, "MultiSync::Ping(%d, %d)\n", discover, broadcast);
    time_t t = time(NULL);
    m_lastPingTime = (unsigned long)t;

    if (m_broadcastSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send ping packet but control socket is not open.\n");
        return;
    }

    // update the range for local systems so it's accurate
    const std::vector<std::pair<uint32_t, uint32_t>>& ranges = GetOutputRanges(true);
    std::string range = createRanges(ranges, 120);
    char outBuf[768];

    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
    for (auto& sys : m_localSystems) {
        memset(outBuf, 0, sizeof(outBuf));
        sys.ranges = range;
        int len = CreatePingPacket(sys, outBuf, discover);
        lock.unlock();
        if (broadcast) {
            SendBroadcastPacket(outBuf, len);
        } else {
            SendMulticastPacket(outBuf, len);
        }
        lock.lock();
    }

    if (discover) {
        std::string extraRemotes = getSetting("MultiSyncExtraRemotes");
        if (extraRemotes != "") {
            std::vector<std::string> tokens = split(extraRemotes, ',');
            std::set<std::string> remotes;
            for (auto& token : tokens) {
                TrimWhiteSpace(token);
                if (token != "") {
                    PingSingleRemote(token.c_str(), 1);
                }
            }
        }
    }
}
void MultiSync::PeriodicPing() {
    time_t t = time(NULL);
    if (m_lastCheckTime == 0) {
        m_lastCheckTime = (unsigned long)t;
    }
    unsigned long lpt = m_lastPingTime + 60 * 60;
    if (lpt < (unsigned long)t) {
        // once an hour, we'll send a ping letting everyone know we're still here
        // mark ourselves as seen
        std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
        for (auto& sys : m_localSystems) {
            sys.lastSeen = (unsigned long)t;
        }
        lock.unlock();
        Ping();
    }
    // every minute we'll loop through real quick and check for remote instances
    // we haven't heard from in a while
    lpt = m_lastCheckTime + 60;
    bool superLongGap = false;
    if (lpt < (unsigned long)t) {
        m_lastCheckTime = (unsigned long)t;
        // anything we haven't heard from in 80 minutes we will re-ping to force
        unsigned long timeoutRePing = (unsigned long)t - 60 * 80;
        // anything we haven't heard from in 2 hours we will remove.   That would
        // have caused at least 4 pings to have been sent.  If it has responded
        // to any of those 4, it's got to be down/gone.   Remove it.
        unsigned long timeoutRemove = (unsigned long)t - 60 * 120;
        // if we hadn't heard from them in 10 hours, it's likely a clock change
        // event, we'll resend a ping out to all to hopefully get everything
        // updated and new timestamps
        unsigned long timeoutRePingAll = (unsigned long)t - 60 * 600;
        std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
        for (auto it = m_remoteSystems.begin(); it != m_remoteSystems.end();) {
            if (it->lastSeen < timeoutRemove) {
                LogInfo(VB_SYNC, "Have not seen %s in over 2 hours, removing\n", it->address.c_str());
                m_remoteSystems.erase(it);
                if (it->lastSeen < timeoutRePingAll) {
                    superLongGap = true;
                }
            } else if (it->lastSeen < timeoutRePing) {
                if (it->multiSync) {
                    PingSingleRemote(*it, 1);
                } else {
                    PingSingleRemoteViaHTTP(it->address);
                }
                ++it;
            } else {
                ++it;
            }
        }
    }
    if (superLongGap) {
        Ping(1);
    }
}

void MultiSync::PingSingleRemoteViaHTTP(const std::string& address) {
    std::string url("http://");
    std::string resp;

    url += address;

    if (urlHelper("GET", url, resp, 1)) {
        if (resp != "") {
            NetworkController* nc = NetworkController::DetectControllerViaHTML(address.c_str(), resp);

            if (nc) {
                UpdateSystem(nc->typeId, nc->majorVersion, nc->minorVersion,
                             nc->systemMode, nc->ip, nc->hostname, nc->version,
                             nc->typeStr, nc->ranges, nc->uuid, false, nc->sendingMultiSync);
                delete nc;
            } else {
                UpdateSystem(kSysTypeUnknown, 0, 0, UNKNOWN_MODE, address,
                             address, "Unknown", "Unknown", "0-0", "Unknown", false, false);
            }
        }
    }
}

void MultiSync::PingSingleRemote(const char* address, int discover) {
    MultiSyncSystem sys;

    sys.address = address;

    PingSingleRemote(sys, discover);
}

void MultiSync::PingSingleRemote(MultiSyncSystem& sys, int discover) {
    if (m_localSystems.empty()) {
        return;
    }
    char outBuf[512];
    memset(outBuf, 0, sizeof(outBuf));
    int len = CreatePingPacket(m_localSystems[0], outBuf, discover);
    SendUnicastPacket(sys.address, outBuf, len);
}
int MultiSync::CreatePingPacket(MultiSyncSystem& sysInfo, char* outBuf, int discover) {
    ControlPkt* cpkt = (ControlPkt*)outBuf;
    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_PING;
    cpkt->extraDataLen = 294; // v3 ping length

    unsigned char* ed = (unsigned char*)(outBuf + 7);
    memset(ed, 0, cpkt->extraDataLen - 7);

    ed[0] = 3;                    // ping version 3
    ed[1] = discover > 0 ? 1 : 0; // 0 = ping, 1 = discover
    ed[2] = sysInfo.type;
    ed[3] = (sysInfo.majorVersion & 0xFF00) >> 8;
    ed[4] = (sysInfo.majorVersion & 0x00FF);
    ed[5] = (sysInfo.minorVersion & 0xFF00) >> 8;
    ed[6] = (sysInfo.minorVersion & 0x00FF);
    ed[7] = sysInfo.fppMode;
    if (sysInfo.sendingMultiSync) {
        ed[7] |= 0x04;
    }
    ed[8] = sysInfo.ipa;
    ed[9] = sysInfo.ipb;
    ed[10] = sysInfo.ipc;
    ed[11] = sysInfo.ipd;

    strncpy((char*)(ed + 12), sysInfo.hostname.c_str(), 64);
    strncpy((char*)(ed + 77), sysInfo.version.c_str(), 40);
    strncpy((char*)(ed + 118), sysInfo.model.c_str(), 40);
    strncpy((char*)(ed + 159), sysInfo.ranges.c_str(), 120);
    return sizeof(ControlPkt) + cpkt->extraDataLen;
}

void MultiSync::SendSeqOpenPacket(const std::string& filename) {
    if (filename.empty()) {
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
        return;
    }

    LogDebug(VB_SYNC, "SendSeqOpenPacket('%s')\n", filename.c_str());
    for (auto a : m_plugins) {
        a->SendSeqOpenPacket(filename);
    }
    m_lastFrame = -1;
    m_lastFrameSent = -1;

    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    SyncPkt* spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_SYNC;
    cpkt->extraDataLen = sizeof(SyncPkt) + filename.length();

    spkt->pktType = SYNC_PKT_OPEN;
    spkt->fileType = SYNC_FILE_SEQ;
    spkt->frameNumber = 0;
    spkt->secondsElapsed = 0;
    strcpy(spkt->filename, filename.c_str());

    SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

void MultiSync::SendSeqSyncStartPacket(const std::string& filename) {
    if (filename.empty()) {
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
        return;
    }

    LogDebug(VB_SYNC, "SendSeqSyncStartPacket('%s')\n", filename.c_str());
    for (auto a : m_plugins) {
        a->SendSeqSyncStartPacket(filename);
    }
    m_lastFrame = -1;
    m_lastFrameSent = -1;

    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    SyncPkt* spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_SYNC;
    cpkt->extraDataLen = sizeof(SyncPkt) + filename.length();

    spkt->pktType = SYNC_PKT_START;
    spkt->fileType = SYNC_FILE_SEQ;
    spkt->frameNumber = 0;
    spkt->secondsElapsed = 0;
    strcpy(spkt->filename, filename.c_str());

    SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

/*
 *
 */
void MultiSync::SendSeqSyncStopPacket(const std::string& filename) {
    if (filename.empty()) {
        return;
    }
    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send stop packet but sync socket is not open.\n");
        return;
    }
    LogDebug(VB_SYNC, "SendSeqSyncStopPacket(%s)\n", filename.c_str());

    for (auto a : m_plugins) {
        a->SendSeqSyncStopPacket(filename);
    }

    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    SyncPkt* spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_SYNC;
    cpkt->extraDataLen = sizeof(SyncPkt) + filename.length();

    spkt->pktType = SYNC_PKT_STOP;
    spkt->fileType = SYNC_FILE_SEQ;
    spkt->frameNumber = 0;
    spkt->secondsElapsed = 0;
    strcpy(spkt->filename, filename.c_str());

    SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());

    m_lastFrame = -1;
    m_lastFrameSent = -1;
}

/*
 *
 */
void MultiSync::SendSeqSyncPacket(const std::string& filename, int frames, float seconds) {
    if (filename.empty()) {
        return;
    }
    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send sync packet but sync socket is not open.\n");
        return;
    }
    for (auto a : m_plugins) {
        a->SendSeqSyncPacket(filename, frames, seconds);
    }
    m_lastFrame = frames;
    int diff = frames - m_lastFrameSent;
    if (frames > 32) {
        // after 32 frames, we send every 10
        //  that's either twice a second (50ms sequences) or 4 times (25ms)
        if (diff < 10) {
            return;
        }
    } else if (frames && diff < 4) {
        // under 32 frames, we send every 4
        return;
    }
    m_lastFrameSent = frames;

    LogDebug(VB_SYNC, "SendSeqSyncPacket( '%s', %d, %.2f)\n",
             filename.c_str(), frames, seconds);
    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    SyncPkt* spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_SYNC;
    cpkt->extraDataLen = sizeof(SyncPkt) + filename.length();

    spkt->pktType = SYNC_PKT_SYNC;
    spkt->fileType = SYNC_FILE_SEQ;
    spkt->frameNumber = frames;
    spkt->secondsElapsed = seconds;
    strcpy(spkt->filename, filename.c_str());

    SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

void MultiSync::SendMediaOpenPacket(const std::string& filename) {
    if (filename.empty()) {
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
        return;
    }
    LogDebug(VB_SYNC, "SendMediaOpenPacket('%s')\n", filename.c_str());

    for (auto a : m_plugins) {
        a->SendMediaOpenPacket(filename);
    }

    m_lastMediaHalfSecond = 0;

    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    SyncPkt* spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_SYNC;
    cpkt->extraDataLen = sizeof(SyncPkt) + filename.length();

    spkt->pktType = SYNC_PKT_OPEN;
    spkt->fileType = SYNC_FILE_MEDIA;
    spkt->frameNumber = 0;
    spkt->secondsElapsed = 0;
    strcpy(spkt->filename, filename.c_str());

    SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}
void MultiSync::SendMediaSyncStartPacket(const std::string& filename) {
    if (filename.empty()) {
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
        return;
    }
    LogDebug(VB_SYNC, "SendMediaSyncStartPacket('%s')\n", filename.c_str());

    for (auto a : m_plugins) {
        a->SendMediaSyncStartPacket(filename);
    }

    m_lastMediaHalfSecond = 0;

    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    SyncPkt* spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_SYNC;
    cpkt->extraDataLen = sizeof(SyncPkt) + filename.length();

    spkt->pktType = SYNC_PKT_START;
    spkt->fileType = SYNC_FILE_MEDIA;
    spkt->frameNumber = 0;
    spkt->secondsElapsed = 0;
    strcpy(spkt->filename, filename.c_str());

    SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

/*
 *
 */
void MultiSync::SendMediaSyncStopPacket(const std::string& filename) {
    if (filename.empty()) {
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send stop packet but sync socket is not open.\n");
        return;
    }
    LogDebug(VB_SYNC, "SendMediaSyncStopPacket(%s)\n", filename.c_str());
    for (auto a : m_plugins) {
        a->SendMediaSyncStopPacket(filename);
    }

    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    SyncPkt* spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_SYNC;
    cpkt->extraDataLen = sizeof(SyncPkt) + filename.length();

    spkt->pktType = SYNC_PKT_STOP;
    spkt->fileType = SYNC_FILE_MEDIA;
    spkt->frameNumber = 0;
    spkt->secondsElapsed = 0;
    strcpy(spkt->filename, filename.c_str());

    SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

/*
 *
 */
void MultiSync::SendMediaSyncPacket(const std::string& filename, float seconds) {
    if (filename.empty()) {
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send sync packet but sync socket is not open.\n");
        return;
    }

    for (auto a : m_plugins) {
        a->SendMediaSyncPacket(filename, seconds);
    }

    int curTS = (seconds * 2.0f);
    if (m_lastMediaHalfSecond == curTS) {
        // not time to send
        return;
    }
    m_lastMediaHalfSecond = curTS;

    LogExcess(VB_SYNC, "SendMediaSyncPacket( '%s', %.2f)\n",
              filename.c_str(), seconds);

    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    SyncPkt* spkt = (SyncPkt*)(outBuf + sizeof(ControlPkt));

    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_SYNC;
    cpkt->extraDataLen = sizeof(SyncPkt) + filename.length();

    spkt->pktType = SYNC_PKT_SYNC;
    spkt->fileType = SYNC_FILE_MEDIA;
    spkt->frameNumber = 0;
    spkt->secondsElapsed = seconds;
    strcpy(spkt->filename, filename.c_str());

    SendControlPacket(outBuf, sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length());
}

void MultiSync::SendPluginData(const std::string& name, const uint8_t* data, int len) {
    if (name.empty()) {
        return;
    }
    for (auto a : m_plugins) {
        a->SendPluginData(name, data, len);
    }

    LogDebug(VB_SYNC, "SendPluginData('%s')\n", name.c_str());
    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send plugin data packet but control socket is not open.\n");
        return;
    }

    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    CommandPkt* epkt = (CommandPkt*)(outBuf + sizeof(ControlPkt));

    InitControlPacket(cpkt);
    int nlen = strlen(name.c_str()) + 1; // add the null
    cpkt->pktType = CTRL_PKT_PLUGIN;
    cpkt->extraDataLen = len + nlen;

    strcpy(epkt->command, name.c_str());
    memcpy(&epkt->command[nlen], data, len);

    SendControlPacket(outBuf, sizeof(ControlPkt) + len + nlen);
}

/*
 *
 */
void MultiSync::SendBlankingDataPacket(void) {
    LogDebug(VB_SYNC, "SendBlankingDataPacket()\n");

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send blanking data packet but control socket is not open.\n");
        return;
    }
    for (auto a : m_plugins) {
        a->SendBlankingDataPacket();
    }
    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;

    InitControlPacket(cpkt);

    cpkt->pktType = CTRL_PKT_BLANK;
    cpkt->extraDataLen = 0;

    SendControlPacket(outBuf, sizeof(ControlPkt));
}
void MultiSync::addMultiSyncPlugin(MultiSyncPlugin* p) {
    m_plugins.push_back(p);
}
void MultiSync::removeMultiSyncPlugin(MultiSyncPlugin* p) {
    auto a = std::find(m_plugins.begin(), m_plugins.end(), p);
    if (a != m_plugins.end()) {
        m_plugins.erase(a);
    }
}

/*
 *
 */
void MultiSync::ShutdownSync(void) {
    LogDebug(VB_SYNC, "ShutdownSync()\n");

    for (auto a : m_plugins) {
        a->ShutdownSync();
    }
    m_plugins.clear();

    std::unique_lock<std::mutex> lock(m_socketLock);
    if (m_broadcastSock >= 0) {
        close(m_broadcastSock);
        m_broadcastSock = -1;
    }

    if (m_controlSock >= 0) {
        close(m_controlSock);
        m_controlSock = -1;
    }

    if (m_receiveSock >= 0) {
        close(m_receiveSock);
        m_receiveSock = -1;
    }
}

/*
 *
 */
int MultiSync::OpenBroadcastSocket(void) {
    LogDebug(VB_SYNC, "OpenBroadcastSocket()\n");

    m_broadcastSock = socket(AF_INET, SOCK_DGRAM, 0);

    if (m_broadcastSock < 0) {
        LogErr(VB_SYNC, "Error opening MultiSync broadcast socket\n");
        return 0;
    }

    char loopch = 0;
    if (setsockopt(m_broadcastSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
               strerror(errno));
        return 0;
    }

    int broadcast = 1;
    if (setsockopt(m_broadcastSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", strerror(errno));
        return 0;
    }

    return 1;
}

/*
 *
 */
int MultiSync::OpenControlSockets() {
    LogDebug(VB_SYNC, "OpenControlSockets()\n");
    if (m_controlSock >= 0) {
        return 1;
    }

    m_controlSock = socket(AF_INET, SOCK_DGRAM, 0);

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "Error opening MultiSync socket\n");
        return 0;
    }

    char loopch = 0;
    if (setsockopt(m_controlSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
               strerror(errno));
        return 0;
    }

    std::string remotesString = getSetting("MultiSyncRemotes");
    std::string extraRemotes = getSetting("MultiSyncExtraRemotes");
    if (extraRemotes != "") {
        if (remotesString == "") {
            remotesString = extraRemotes;
        } else {
            remotesString += ",";
            remotesString += extraRemotes;
        }
    }

    std::vector<std::string> tokens = split(remotesString, ',');
    std::set<std::string> remotes;
    for (auto& token : tokens) {
        TrimWhiteSpace(token);
        if (token != "") {
            remotes.insert(token);
        }
    }

    if (getSettingInt("MultiSyncBroadcast")) {
        m_sendBroadcast = true;
    }

    if (getSettingInt("MultiSyncMulticast")) {
        m_sendMulticast = true;
    }
    if (remotesString == "" && m_multiSyncEnabled) {
        m_sendMulticast = true;
    }

    for (auto& s : remotes) {
        LogDebug(VB_SYNC, "Setting up Remote Sync for %s\n", s.c_str());
        struct sockaddr_in newRemote;

        newRemote.sin_family = AF_INET;
        newRemote.sin_port = htons(FPP_CTRL_PORT);

        bool isAlpha = std::find_if(s.begin(), s.end(), [](char c) { return (isalpha(c) || (c == ' ')); }) == s.end();
        bool valid = true;
        if (isAlpha) {
            struct hostent* uhost = gethostbyname(s.c_str());
            if (!uhost) {
                LogErr(VB_SYNC,
                       "Error looking up Remote hostname: %s\n",
                       s.c_str());
                valid = false;
            } else {
                newRemote.sin_addr.s_addr = *((unsigned long*)uhost->h_addr);
            }
        } else {
            newRemote.sin_addr.s_addr = inet_addr(s.c_str());
        }
        if (valid) {
            m_destAddr.push_back(newRemote);
        }
    }
    for (int x = 0; x < m_destAddr.size(); x++) {
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));

        msg.msg_hdr.msg_name = &m_destAddr[x];
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        msg.msg_hdr.msg_iov = &m_destIovec;
        msg.msg_hdr.msg_iovlen = 1;
        msg.msg_len = 0;
        m_destMsgs.push_back(msg);
    }

    LogDebug(VB_SYNC, "%d Remote Sync systems configured\n",
             m_destAddr.size());
    FillInInterfaces();
    return 1;
}

void MultiSync::SendControlPacket(void* outBuf, int len) {
    if (WillLog(LOG_EXCESSIVE, VB_SYNC)) {
        LogExcess(VB_SYNC, "SendControlPacket()\n");
        HexDump("Sending Control packet with contents:", outBuf, len, VB_SYNC);
    }

    int msgCount = m_destMsgs.size();
    if (msgCount != 0) {
        m_destIovec.iov_base = outBuf;
        m_destIovec.iov_len = len;

        std::unique_lock<std::mutex> lock(m_socketLock);
        int oc = sendmmsg(m_controlSock, &m_destMsgs[0], msgCount, MSG_DONTWAIT);
        int outputCount = oc;
        long long startTime = GetTimeMS();
        while (oc >= 0 && outputCount != msgCount) {
            int oc = sendmmsg(m_controlSock, &m_destMsgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
            if (oc > 0) {
                outputCount += oc;
            } else {
                long long tm = GetTimeMS();
                long long totalTime = tm - startTime;
                if (totalTime < 10) {
                    // we'll keep trying for up to 15ms, but give the network stack some time to flush some buffers
                    std::this_thread::sleep_for(std::chrono::microseconds(250));
                } else {
                    oc = -1;
                }
            }
        }
        if (outputCount != msgCount) {
            LogErr(VB_SYNC, "Error: Unable to send multisync packet: %s   (%d/%d)\n", strerror(errno), outputCount, msgCount);
        }
    }
    if (m_sendMulticast) {
        SendMulticastPacket(outBuf, len);
    }
    if (m_sendBroadcast) {
        SendBroadcastPacket(outBuf, len);
    }
}
void MultiSync::SendBroadcastPacket(void* outBuf, int len) {
    if (WillLog(LOG_EXCESSIVE, VB_SYNC)) {
        HexDump("Sending Broadcast packet with contents:", outBuf, len, VB_SYNC);
    }

    std::unique_lock<std::mutex> lock(m_socketLock);
    for (auto& a : m_interfaces) {
        struct sockaddr_in bda;
        memset((void*)&bda, 0, sizeof(struct sockaddr_in));
        bda.sin_family = AF_INET;
        bda.sin_port = htons(FPP_CTRL_PORT);
        bda.sin_addr.s_addr = a.second.broadcastAddress;

        if (sendto(m_broadcastSock, outBuf, len, 0, (struct sockaddr*)&bda, sizeof(struct sockaddr_in)) < 0)
            LogErr(VB_SYNC, "Error: Unable to send packet: %s\n", strerror(errno));
    }
}
void MultiSync::SendMulticastPacket(void* outBuf, int len) {
    std::unique_lock<std::mutex> lock(m_socketLock);
    for (auto& a : m_interfaces) {
        struct sockaddr_in bda;
        memset((void*)&bda, 0, sizeof(struct sockaddr_in));
        bda.sin_family = AF_INET;
        bda.sin_port = htons(FPP_CTRL_PORT);
        bda.sin_addr.s_addr = MULTISYNC_MULTICAST_ADD;

        if (a.second.multicastSocket == -1) {
            // create the socket
            a.second.multicastSocket = socket(AF_INET, SOCK_DGRAM, 0);

            if (a.second.multicastSocket < 0) {
                LogErr(VB_SYNC, "Error opening Multicast socket for %s\n", a.second.interfaceName.c_str());
            } else {
                char loopch = 0;
                if (setsockopt(a.second.multicastSocket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch)) < 0) {
                    LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP for %s: %s\n", a.second.interfaceName.c_str(), strerror(errno));
                }
#ifdef PLATFORM_OSX
                int idx = if_nametoindex(a.second.interfaceName.c_str());
                if (setsockopt(a.second.multicastSocket, IPPROTO_IP, IP_BOUND_IF, &idx, sizeof(idx)) < 0) {
                    LogErr(VB_SYNC, "Error setting IP_MULTICAST Device for %s: %s\n", a.second.interfaceName.c_str(), strerror(errno));
                }
#else
                if (setsockopt(a.second.multicastSocket, SOL_SOCKET, SO_BINDTODEVICE, a.second.interfaceName.c_str(), a.second.interfaceName.size()) < 0) {
                    LogErr(VB_SYNC, "Error setting IP_MULTICAST Device for %s: %s\n", a.second.interfaceName.c_str(), strerror(errno));
                }
#endif
            }
        }

        if (a.second.multicastSocket >= 0) {
            if (sendto(a.second.multicastSocket, outBuf, len, 0, (struct sockaddr*)&bda, sizeof(struct sockaddr_in)) < 0)
                LogErr(VB_SYNC, "Error: Unable to send packet: %s\n", strerror(errno));
        }
    }
}
void MultiSync::SendUnicastPacket(const std::string& address, void* outBuf, int len) {
    if (WillLog(LOG_EXCESSIVE, VB_SYNC)) {
        LogExcess(VB_SYNC, "SendControlPacket()\n");
        HexDump("Sending Control packet with contents:", outBuf, len, VB_SYNC);
    }
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;

    std::string ipAd = address;
    if (GetIPForHost(ipAd)) {
        dest_addr.sin_addr.s_addr = inet_addr(ipAd.c_str());
    } else {
        dest_addr.sin_addr.s_addr = inet_addr(address.c_str());
    }
    dest_addr.sin_port = htons(FPP_CTRL_PORT);
    std::unique_lock<std::mutex> lock(m_socketLock);
    if (m_controlSock >= 0) {
        sendto(m_controlSock, outBuf, len, MSG_DONTWAIT, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    } else {
        int uSock = socket(AF_INET, SOCK_DGRAM, 0);
        if (uSock < 0) {
            LogErr(VB_SYNC, "Error opening Unicast socket\n");
            return;
        }

        sendto(uSock, outBuf, len, MSG_DONTWAIT, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        close(uSock);
    }
}

bool MultiSync::FillInInterfaces() {
    struct ifaddrs *interfaces, *tmp;
    getifaddrs(&interfaces);
    tmp = interfaces;

    bool change = false;

    std::unique_lock<std::mutex> lock(m_socketLock);
    while (tmp) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
            if (isSupportedForMultisync("", tmp->ifa_name)) {
                // skip the usb* interfaces as we won't support multisync on those
#ifdef PLATFORM_OSX
                struct sockaddr_in* ba = (struct sockaddr_in*)(tmp->ifa_dstaddr);
#else
                struct sockaddr_in* ba = (struct sockaddr_in*)(tmp->ifa_ifu.ifu_broadaddr);
#endif
                struct sockaddr_in* sa = (struct sockaddr_in*)(tmp->ifa_addr);

                NetInterfaceInfo& info = m_interfaces[tmp->ifa_name];
                change |= info.interfaceName == "";
                change |= info.interfaceAddress == "";
                info.interfaceName = tmp->ifa_name;
                info.interfaceAddress = inet_ntoa(sa->sin_addr);
                info.address = sa->sin_addr.s_addr;
                info.broadcastAddress = ba->sin_addr.s_addr;
            }
        } else if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET6) {
            // FIXME for ipv6 multisync
        }
        tmp = tmp->ifa_next;
    }
    freeifaddrs(interfaces);
    return change;
}
bool MultiSync::RemoveInterface(const std::string& interface) {
    std::unique_lock<std::mutex> lock(m_socketLock);
    auto it = m_interfaces.find(interface);
    if (it != m_interfaces.end()) {
        LogDebug(VB_SYNC, "Removing interface %s - %s\n", it->second.interfaceName.c_str(), it->second.interfaceAddress.c_str());

        struct ip_mreq mreq;
        memset(&mreq, 0, sizeof(mreq));
        mreq.imr_multiaddr.s_addr = inet_addr(MULTISYNC_MULTICAST_ADDRESS);
        mreq.imr_interface.s_addr = it->second.address;
        int rc = 0;
        if ((rc = setsockopt(m_receiveSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq))) < 0) {
            LogDebug(VB_SYNC, "   Did not drop Multicast membership for interface %s - %s\n", it->second.interfaceName.c_str(), it->second.interfaceAddress.c_str());
        }
        m_interfaces.erase(it);
        return true;
    }
    return false;
}

/*
 *
 */
void MultiSync::InitControlPacket(ControlPkt* pkt) {
    bzero(pkt, sizeof(ControlPkt));

    pkt->fppd[0] = 'F';
    pkt->fppd[1] = 'P';
    pkt->fppd[2] = 'P';
    pkt->fppd[3] = 'D';
    pkt->pktType = 0;
    pkt->extraDataLen = 0;
}

/*
 *
 */
int MultiSync::OpenReceiveSocket(void) {
    LogDebug(VB_SYNC, "OpenReceiveSocket()\n");

    int UniverseOctet[2];
    int i;
    char strMulticastGroup[16];

    /* set up socket */
    m_receiveSock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (m_receiveSock < 0) {
        LogErr(VB_SYNC, "Error opening Receive socket; %s\n", strerror(errno));
        return 0;
    }
    LogDebug(VB_SYNC, "Receive socket: %d\n", m_receiveSock);

    bzero((char*)&m_receiveSrcAddr, sizeof(m_receiveSrcAddr));
    m_receiveSrcAddr.sin_family = AF_INET;
    m_receiveSrcAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_receiveSrcAddr.sin_port = htons(FPP_CTRL_PORT);

    int optval = 1;
    if (setsockopt(m_receiveSock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        LogErr(VB_SYNC, "Error turning on SO_REUSEPORT; %s\n", strerror(errno));
        return 0;
    }

    // Bind the socket to address/port
    if (bind(m_receiveSock, (struct sockaddr*)&m_receiveSrcAddr, sizeof(m_receiveSrcAddr)) < 0) {
        LogErr(VB_SYNC, "Error binding socket; %s\n", strerror(errno));
        return 0;
    }

    if (setsockopt(m_receiveSock, IPPROTO_IP, IP_PKTINFO, &optval, sizeof(optval)) < 0) {
        LogErr(VB_SYNC, "Error calling setsockopt; %s\n", strerror(errno));
        return 0;
    }

    if (getFPPmode() == REMOTE_MODE) {
        int remoteOffsetInt = getSettingInt("remoteOffset");
        if (remoteOffsetInt)
            m_remoteOffset = (float)remoteOffsetInt * -0.001;
        else
            m_remoteOffset = 0.0;

        LogDebug(VB_SYNC, "Using remoteOffset of %.3f\n", m_remoteOffset);
    }

    memset(rcvMsgs, 0, sizeof(rcvMsgs));
    for (int i = 0; i < MAX_MS_RCV_MSG; i++) {
        rcvIovecs[i].iov_base = rcvBuffers[i];
        rcvIovecs[i].iov_len = MAX_MS_RCV_BUFSIZE;
        rcvMsgs[i].msg_hdr.msg_iov = &rcvIovecs[i];
        rcvMsgs[i].msg_hdr.msg_iovlen = 1;
        rcvMsgs[i].msg_hdr.msg_name = &rcvSrcAddr[i];
        rcvMsgs[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_storage);
        rcvMsgs[i].msg_hdr.msg_control = &rcvCmbuf[i];
        rcvMsgs[i].msg_hdr.msg_controllen = 0x100;
    }

    setupMulticastReceive();
    return 1;
}
bool MultiSync::isSupportedForMultisync(const char* address, const char* intface) {
    if (strlen(address) > 3 && address[0] == '1' && address[1] == '2' && address[2] == '7') {
        // the entire 127.* subnet is localhost
        return false;
    }
    if (!strncmp(intface, "usb", 3) || !strcmp(intface, "lo") || !strncmp(intface, "tether", 6) || !strncmp(intface, "SoftAp", 6)) {
        return false;
    }
    return true;
}

void MultiSync::setupMulticastReceive() {
    LogDebug(VB_SYNC, "setupMulticastReceive()\n");
    // loop through all the interfaces and subscribe to the group
    std::unique_lock<std::mutex> lock(m_socketLock);
    for (auto& a : m_interfaces) {
        LogDebug(VB_SYNC, "   Adding interface %s - %s\n", a.second.interfaceName.c_str(), a.second.interfaceAddress.c_str());
        struct ip_mreq mreq;
        memset(&mreq, 0, sizeof(mreq));
        mreq.imr_multiaddr.s_addr = inet_addr(MULTISYNC_MULTICAST_ADDRESS);
        mreq.imr_interface.s_addr = a.second.address;
        int rc = 0;
        if ((rc = setsockopt(m_receiveSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) < 0) {
            if (m_broadcastSock < 0) {
                // first time through, log as warning, otherise error is likely due t already being subscribed
                LogWarn(VB_SYNC, "   Could not setup Multicast Group for interface %s    rc: %d\n", a.second.interfaceName.c_str(), rc);
            } else {
                LogDebug(VB_SYNC, "   Could not setup Multicast Group for interface %s    rc: %d\n", a.second.interfaceName.c_str(), rc);
            }
        }
    }
}

static bool shouldSkipPacket(int i, int num, std::vector<unsigned char*>& rcvBuffers) {
    ControlPkt* pkt = (ControlPkt*)(rcvBuffers[i]);
    for (int x = i + 1; x < num; x++) {
        ControlPkt* npkt = (ControlPkt*)(rcvBuffers[x]);
        if (pkt->pktType == npkt->pktType && pkt->pktType == CTRL_PKT_SYNC) {
            SyncPkt* spkt = (SyncPkt*)(((char*)pkt) + sizeof(ControlPkt));
            SyncPkt* snpkt = (SyncPkt*)(((char*)npkt) + sizeof(ControlPkt));
            if (spkt->fileType == snpkt->fileType && spkt->pktType == snpkt->pktType) {
                return true;
            }
        }
    }
    return false;
}

/*
 *
 */
void MultiSync::ProcessControlPacket(bool pingOnly) {
    LogExcess(VB_SYNC, "ProcessControlPacket()\n");

    ControlPkt* pkt;

    int msgcnt = recvmmsg(m_receiveSock, rcvMsgs, MAX_MS_RCV_MSG, MSG_DONTWAIT, nullptr);
    while (msgcnt > 0) {
        std::vector<unsigned char*> v;
        for (int msg = 0; msg < msgcnt; msg++) {
            int len = rcvMsgs[msg].msg_len;
            if (len <= 0) {
                LogErr(VB_SYNC, "Error: recvmsg failed: %s\n", strerror(errno));
                continue;
            }
            unsigned char* inBuf = rcvBuffers[msg];
            inBuf[len] = 0;
            v.push_back(inBuf);
        }
        LogExcess(VB_SYNC, "ProcessControlPacket msgcnt: %d\n", msgcnt);
        for (int msg = 0; msg < msgcnt; msg++) {
            int len = rcvMsgs[msg].msg_len;
            if (len <= 0) {
                LogErr(VB_SYNC, "Error: recvmsg failed: %s\n", strerror(errno));
                continue;
            }
            unsigned char* inBuf = rcvBuffers[msg];

            if (inBuf[0] == 0x55 || inBuf[0] == 0xCC) {
                struct in_addr recvAddr;
                struct cmsghdr* cmsg;

                for (cmsg = CMSG_FIRSTHDR(&rcvMsgs[msg].msg_hdr); cmsg != NULL; cmsg = CMSG_NXTHDR(&rcvMsgs[msg].msg_hdr, cmsg)) {
                    if (cmsg->cmsg_level != IPPROTO_IP || cmsg->cmsg_type != IP_PKTINFO) {
                        continue;
                    }

                    struct in_pktinfo* pi = (struct in_pktinfo*)CMSG_DATA(cmsg);
                    recvAddr = pi->ipi_addr;
                    recvAddr = pi->ipi_spec_dst;
                }

                ProcessFalconPacket(m_receiveSock, (struct sockaddr_in*)&rcvSrcAddr[msg], recvAddr, inBuf);
                continue;
            }

            if (len < sizeof(ControlPkt)) {
                LogErr(VB_SYNC, "Error: Received control packet too short\n");
                HexDump("Received data:", (void*)inBuf, len, VB_SYNC);
                continue;
            }
            if (shouldSkipPacket(msg, msgcnt, v)) {
                LogExcess(VB_SYNC, "Skipping sync packet %d/%d\n", msg, msgcnt);
                continue;
            }

            char tmpIP[INET_ADDRSTRLEN];
            inet_ntop(rcvSrcAddr[msg].ss_family, &(((struct sockaddr_in*)&rcvSrcAddr[msg])->sin_addr), tmpIP, INET_ADDRSTRLEN);
            std::string sourceIP(tmpIP);
            std::string hostname;

            std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
            bool isLocal = false;
            for (auto& sys : m_localSystems) {
                if (sys.address == sourceIP) {
                    isLocal = true;
                }
            }
            lock.unlock();

            MultiSyncStats tempStats("", "");
            MultiSyncStats* stats = &tempStats;
            std::unique_lock<std::recursive_mutex> slock(m_statsLock);

            if (!isLocal) {
                for (auto& sys : m_remoteSystems) {
                    if (sys.address == sourceIP)
                        hostname = sys.hostname;
                }

                auto a = m_syncStats.find(sourceIP);
                if (a != m_syncStats.end()) {
                    stats = (MultiSyncStats*)a->second;
                } else {
                    stats = new MultiSyncStats(sourceIP, hostname);
                    m_syncStats[sourceIP] = stats;
                }
                stats->lastReceiveTime = time(NULL);
            }
            slock.unlock();

            pkt = (ControlPkt*)inBuf;

            if ((pkt->fppd[0] != 'F') ||
                (pkt->fppd[1] != 'P') ||
                (pkt->fppd[2] != 'P') ||
                (pkt->fppd[3] != 'D')) {
                LogErr(VB_SYNC, "Error: Invalid Received Control Packet, missing 'FPPD' header\n");
                HexDump("Received data:", (void*)inBuf, len, VB_SYNC);
                continue;
            }

            if (len != (sizeof(ControlPkt) + pkt->extraDataLen)) {
                LogErr(VB_SYNC, "Error: Expected %d data bytes, received %d\n",
                       pkt->extraDataLen, len - sizeof(ControlPkt));
                HexDump("Received data:", (void*)inBuf, len, VB_SYNC);
                continue;
            }

            if (WillLog(LOG_EXCESSIVE, VB_SYNC)) {
                HexDump("Received MultiSync packet with contents:", (void*)inBuf, len, VB_SYNC);
            }

            if (!pingOnly || pkt->pktType == CTRL_PKT_PING) {
                switch (pkt->pktType) {
                case CTRL_PKT_CMD:
                    ProcessCommandPacket(pkt, len, stats);
                    break;
                case CTRL_PKT_SYNC:
                    if (getFPPmode() == REMOTE_MODE)
                        ProcessSyncPacket(pkt, len, stats);
                    break;
                case CTRL_PKT_BLANK:
                    if (getFPPmode() == REMOTE_MODE) {
                        stats->pktBlank++;
                        sequence->SendBlankingData();
                    }
                    break;
                case CTRL_PKT_PING: {
                    struct sockaddr_in* inAddr = (struct sockaddr_in*)(&rcvSrcAddr[msg]);
                    std::string ip = inet_ntoa(inAddr->sin_addr);
                    ProcessPingPacket(pkt, len, sourceIP, stats, ip);
                    break;
                }
                case CTRL_PKT_PLUGIN:
                    ProcessPluginPacket(pkt, len, stats);
                    break;
                case CTRL_PKT_FPPCOMMAND:
                    ProcessFPPCommandPacket(pkt, len, stats);
                    break;
                }
            }
        }
        msgcnt = recvmmsg(m_receiveSock, rcvMsgs, MAX_MS_RCV_MSG, MSG_DONTWAIT, nullptr);
    }
}

void MultiSync::OpenSyncedSequence(const char* filename) {
    LogDebug(VB_SYNC, "OpenSyncedSequence(%s)\n", filename);

    ResetMasterPosition();
    sequence->OpenSequenceFile(filename);
}

void MultiSync::StartSyncedSequence(const char* filename) {
    LogDebug(VB_SYNC, "StartSyncedSequence(%s)\n", filename);

    ResetMasterPosition();
    if (!sequence->IsSequenceRunning(filename) && !sequence->IsSequenceRunning("fallback.fseq")) {
        sequence->StartSequence(filename, 0);
    }
}

void MultiSync::StopSyncedSequence(const char* filename) {
    LogDebug(VB_SYNC, "StopSyncedSequence(%s)\n", filename);

    sequence->CloseIfOpen(filename);
}
void MultiSync::SyncStopAll() {
    LogDebug(VB_SYNC, "SyncStopAll()\n");
    sequence->CloseSequenceFile();
    if (!mediaOutput) {
        return;
    }
    CloseMediaOutput();
}

void MultiSync::SyncPlaylistToMS(uint64_t ms, const std::string& pl, bool sendSyncPackets) {
    SyncPlaylistToMS(ms, -1, pl, sendSyncPackets);
}

void MultiSync::SyncPlaylistToMS(uint64_t ms, int pos, const std::string& pl, bool sendSyncPackets) {
    if (Player::INSTANCE.GetPlaylistName() != pl) {
        if (pl == "") {
            return;
        }
        Player::INSTANCE.Load(pl);
    }

    int desiredpos = pos;
    if (pos <= -1) {
        desiredpos = Player::INSTANCE.FindPosForMS(ms, pos == -2);
    }

    float seconds = ms;
    seconds /= 1000;
    if (desiredpos >= 0) {
        if (m_controlSock < 0 && sendSyncPackets) {
            OpenControlSockets();
        }

        std::string seq, med;
        Player::INSTANCE.GetFilenamesForPos(desiredpos, seq, med);
        if (seq != "") {
            if (!sequence->IsSequenceRunning(seq)) {
                if (sequence->IsSequenceRunning()) {
                    if (sendSyncPackets)
                        SendSeqSyncStopPacket(sequence->m_seqFilename);
                    sequence->CloseSequenceFile();
                }
                ResetMasterPosition();
                sequence->OpenSequenceFile(seq.c_str());
                if (sendSyncPackets)
                    SendSeqOpenPacket(seq);
                int frame = ms / sequence->GetSeqStepTime();
                sequence->StartSequence(seq.c_str(), frame);
                if (sendSyncPackets)
                    SendSeqSyncStartPacket(seq);
            } else {
                int frame = ms / sequence->GetSeqStepTime();
                SyncSyncedSequence(seq.c_str(), frame, seconds);
                if (sendSyncPackets)
                    SendSeqSyncPacket(seq, frame, seconds);
            }
        }
        if (med != "") {
            if (mediaOutput && !MatchesRunningMediaFilename(med.c_str())) {
                StopSyncedMedia(med.c_str());
                if (sendSyncPackets)
                    SendMediaSyncStopPacket(med);
            }
            if (!mediaOutput) {
                OpenSyncedMedia(med.c_str());
                if (sendSyncPackets)
                    SendMediaOpenPacket(med);
                StartSyncedMedia(med.c_str());
                if (sendSyncPackets)
                    SendMediaSyncStartPacket(med);
            }
            UpdateMasterMediaPosition(med.c_str(), seconds);
            if (sendSyncPackets)
                SendMediaSyncPacket(med, seconds);
        }
    }
}

/*
 *
 */
void MultiSync::SyncSyncedSequence(const char* filename, int frameNumber, float secondsElapsed) {
    LogExcess(VB_SYNC, "SyncSyncedSequence('%s', %d, %.2f)\n",
              filename, frameNumber, secondsElapsed);

    if (!sequence->IsSequenceRunning(filename) && !sequence->IsSequenceRunning("fallback.fseq")) {
        sequence->StartSequence(filename, frameNumber);
    }
    if (sequence->IsSequenceRunning(filename)) {
        if (secondsElapsed > 0.0001f) {
            // recalculate the frame number based on the seconds
            float step = sequence->GetSeqStepTime();
            float newFrame = secondsElapsed * 1000.0f / step;
            frameNumber = std::round(newFrame);
        }
        UpdateMasterPosition(frameNumber);
    }
}

void MultiSync::OpenSyncedMedia(const char* filename) {
    LogDebug(VB_SYNC, "OpenSyncedMedia(%s)\n", filename);

    if (mediaOutput) {
        LogDebug(VB_SYNC, "Start media %s received while playing media %s\n",
                 filename, mediaOutput->m_mediaFilename.c_str());

        CloseMediaOutput();
    }

    OpenMediaOutput(filename);
}

void MultiSync::StartSyncedMedia(const char* filename) {
    LogDebug(VB_SYNC, "StartSyncedMedia(%s)\n", filename);

    StartMediaOutput(filename);
}

/*
 *
 */
void MultiSync::StopSyncedMedia(const char* filename) {
    LogDebug(VB_SYNC, "StopSyncedMedia(%s)\n", filename);

    if (!mediaOutput) {
        return;
    }

    if (MatchesRunningMediaFilename(filename)) {
        LogDebug(VB_SYNC, "Stopping synced media: %s\n", mediaOutput->m_mediaFilename.c_str());
        CloseMediaOutput();
    }
}

/*
 *
 */
void MultiSync::SyncSyncedMedia(const char* filename, int frameNumber, float secondsElapsed) {
    LogExcess(VB_SYNC, "SyncSyncedMedia('%s', %d, %.2f)\n",
              filename, frameNumber, secondsElapsed);

    UpdateMasterMediaPosition(filename, secondsElapsed);
}

/*
 *
 */
void MultiSync::ProcessSyncPacket(ControlPkt* pkt, int len, MultiSyncStats* stats) {
    if (pkt->extraDataLen < sizeof(SyncPkt)) {
        LogErr(VB_SYNC, "Error: Invalid length of received sync packet\n");
        HexDump("Received data:", (void*)&pkt, len, VB_SYNC);
        stats->pktError++;
        return;
    }

    m_syncMaster = stats->sourceIP;

    SyncPkt* spkt = (SyncPkt*)(((char*)pkt) + sizeof(ControlPkt));

    LogDebug(VB_SYNC, "ProcessSyncPacket()   filename: %s    type: %d   filetype: %d   frameNumber: %d   secondsElapsed: %0.2f\n",
             spkt->filename, spkt->pktType, spkt->fileType, spkt->frameNumber, spkt->secondsElapsed);

    float secondsElapsed = 0.0;

    if (spkt->fileType == SYNC_FILE_SEQ) {
        switch (spkt->pktType) {
        case SYNC_PKT_OPEN:
            OpenSyncedSequence(spkt->filename);
            stats->pktSyncSeqOpen++;
            break;
        case SYNC_PKT_START:
            StartSyncedSequence(spkt->filename);
            stats->pktSyncSeqStart++;
            break;
        case SYNC_PKT_STOP:
            StopSyncedSequence(spkt->filename);
            stats->pktSyncSeqStop++;
            break;
        case SYNC_PKT_SYNC:
            secondsElapsed = spkt->secondsElapsed - m_remoteOffset;
            if (secondsElapsed < 0)
                secondsElapsed = 0.0;

            SyncSyncedSequence(spkt->filename,
                               spkt->frameNumber, secondsElapsed);
            stats->pktSyncSeqSync++;
            break;
        }
    } else if (spkt->fileType == SYNC_FILE_MEDIA) {
        switch (spkt->pktType) {
        case SYNC_PKT_OPEN:
            OpenSyncedMedia(spkt->filename);
            stats->pktSyncMedOpen++;
            break;
        case SYNC_PKT_START:
            StartSyncedMedia(spkt->filename);
            stats->pktSyncMedStart++;
            break;
        case SYNC_PKT_STOP:
            StopSyncedMedia(spkt->filename);
            stats->pktSyncMedStop++;
            break;
        case SYNC_PKT_SYNC:
            secondsElapsed = spkt->secondsElapsed - m_remoteOffset;
            if (secondsElapsed < 0)
                secondsElapsed = 0.0;

            SyncSyncedMedia(spkt->filename,
                            spkt->frameNumber, secondsElapsed);
            stats->pktSyncMedSync++;
            break;
        }
    }
}

/*
 *
 */
void MultiSync::ProcessCommandPacket(ControlPkt* pkt, int len, MultiSyncStats* stats) {
    LogDebug(VB_SYNC, "ProcessCommandPacket()\n");

    if (pkt->extraDataLen < sizeof(CommandPkt)) {
        LogErr(VB_SYNC, "Error: Invalid length of received command packet\n");
        HexDump("Received data:", (void*)&pkt, len, VB_SYNC);
        stats->pktError++;
        return;
    }

    stats->pktCommand++;

    CommandPkt* cpkt = (CommandPkt*)(((char*)pkt) + sizeof(ControlPkt));

    char response[1500];
    char* r2 = ProcessCommand(cpkt->command, response);
    if (r2) {
        free(r2);
    }
}

/*
 *
 */
void MultiSync::ProcessPingPacket(ControlPkt* pkt, int len, const std::string& srcIp, MultiSyncStats* stats, const std::string& incomingIp) {
    LogDebug(VB_SYNC, "ProcessPingPacket()\n");

    if (pkt->extraDataLen < 169) { // v1 packet length
        LogErr(VB_SYNC, "ERROR: Invalid length of received Ping packet\n");
        HexDump("Received data:", (void*)&pkt, len, VB_SYNC);
        stats->pktError++;
        return;
    }

    unsigned char* extraData = (unsigned char*)(((char*)pkt) + sizeof(ControlPkt));

    unsigned char pingVersion = extraData[0];

    if ((pingVersion == 1) && (pkt->extraDataLen > 169)) {
        LogErr(VB_SYNC, "ERROR: Ping v1 packet too long: %d\n", pkt->extraDataLen);
        HexDump("Received data:", (void*)&pkt, len, VB_SYNC);
        stats->pktError++;
        return;
    }
    if ((pingVersion == 2) && (pkt->extraDataLen > 214)) {
        LogErr(VB_SYNC, "ERROR: Ping v2 packet too long %d\n", pkt->extraDataLen);
        HexDump("Received data:", (void*)&pkt, len, VB_SYNC);
        stats->pktError++;
        return;
    }
    int discover = extraData[1];

    MultiSyncSystemType type = (MultiSyncSystemType)extraData[2];

    unsigned int majorVersion = ((unsigned int)extraData[3] << 8) | extraData[4];
    unsigned int minorVersion = ((unsigned int)extraData[5] << 8) | extraData[6];

    FPPMode systemMode = (FPPMode)extraData[7];

    bool isInstance = true;
    std::string address;
    if (extraData[8] == 0 && extraData[9] == 0 && extraData[10] == 0 && extraData[11] == 0) {
        if (discover) {
            // No ip address in packet, this is a ping/discovery packet
            // from something (xLights?) that is just trying to
            // get a list of FPP instances, we won't record this
            isInstance = false;
            address = "0.0.0.0";
        } else if (!incomingIp.empty()) {
            address = incomingIp;
        } else {
            isInstance = false;
            address = "0.0.0.0";
        }
    } else {
        char addrStr[16];
        memset(addrStr, 0, sizeof(addrStr));
        snprintf(addrStr, 16, "%d.%d.%d.%d", extraData[8], extraData[9],
                 extraData[10], extraData[11]);
        address = addrStr;
    }

    char tmpStr[128];
    memset(tmpStr, 0, sizeof(tmpStr));
    strcpy(tmpStr, (char*)(extraData + 12));
    std::string hostname(tmpStr);

    strcpy(tmpStr, (char*)(extraData + 77));
    std::string version(tmpStr);

    strcpy(tmpStr, (char*)(extraData + 118));
    std::string typeStr(tmpStr);
    // End of v1 packet fields

    std::string ranges;
    if ((pkt->extraDataLen) > 169) {
        strcpy(tmpStr, (char*)(extraData + 166 - 7));
        ranges = tmpStr;
    }

    bool isLocal = false;
    std::unique_lock<std::mutex> lock(m_socketLock);
    for (auto& a : m_interfaces) {
        if (address == a.second.interfaceAddress) {
            isLocal = true;
        }
    }

    if (isInstance) {
        std::string localUUID(isLocal ? getSetting("SystemUUID") : "Unknown");
        multiSync->UpdateSystem(type, majorVersion, minorVersion,
                                systemMode, address, hostname, version,
                                typeStr, ranges, localUUID.c_str(), true,
                                systemMode & 0x04 ? true : false);
    }
    if (discover) {
        lock.unlock();
        if ((hostname != m_hostname) && !isLocal) {
            // very slight random delay so all the remotes don't send
            // packets out at the same time
            std::srand(std::time(nullptr));
            int rand = std::rand() % 5000;
            std::this_thread::sleep_for(std::chrono::microseconds(rand));
            multiSync->Ping();
            rand = std::rand() % 1000;
            std::this_thread::sleep_for(std::chrono::microseconds(rand));
            multiSync->PingSingleRemote(address.c_str(), 0);
            if (address != srcIp) {
                multiSync->PingSingleRemote(srcIp.c_str(), 0);
            }
        }
    }

    stats->pktPing++;
}

void MultiSync::ProcessPluginPacket(ControlPkt* pkt, int plen, MultiSyncStats* stats) {
    LogDebug(VB_SYNC, "ProcessPluginPacket()\n");
    CommandPkt* cpkt = (CommandPkt*)(((char*)pkt) + sizeof(ControlPkt));
    int len = pkt->extraDataLen;
    char* pn = &cpkt->command[0];
    int nlen = strlen(pn) + 1;
    len -= nlen;
    uint8_t* data = (uint8_t*)&cpkt->command[nlen];
    PluginManager::INSTANCE.multiSyncData(pn, data, len);

    stats->pktPlugin++;
}

static bool MyHostMatches(const std::string& host, const std::string& hostName, std::vector<MultiSyncSystem>& localSystems) {
    std::vector<std::string> names = split(host, ',');
    if (std::find(names.begin(), names.end(), hostName) != names.end()) {
        return true;
    }
    for (auto& h : names) {
        for (auto& ls : localSystems) {
            if (h == ls.address || h == ls.hostname) {
                return true;
            }
        }
    }
    return false;
}

void MultiSync::ProcessFPPCommandPacket(ControlPkt* pkt, int len, MultiSyncStats* stats) {
    char* b = (char*)pkt;
    int pos = sizeof(ControlPkt);
    int numArgs = b[pos++];
    std::string host = &b[pos];
    pos += host.length() + 1;
    std::string cmd = &b[pos];
    pos += cmd.length() + 1;
    std::vector<std::string> args;
    for (int x = 0; x < numArgs; x++) {
        std::string arg = &b[pos];
        pos += arg.length() + 1;
        args.push_back(arg);
    }
    if (host == "" || MyHostMatches(host, m_hostname, m_localSystems)) {
        stats->pktFPPCommand++;

        CommandManager::INSTANCE.run(cmd, args);
    }
}

void MultiSync::SendFPPCommandPacket(const std::string& host, const std::string& cmd, const std::vector<std::string>& args) {
    if (m_controlSock < 0) {
        OpenControlSockets();
    }
    if (m_controlSock < 0) {
        OpenControlSockets();
        LogErr(VB_SYNC, "ERROR: Tried to send FPP Command packet but sync socket is not open.\n");
        return;
    }
    LogDebug(VB_SYNC, "SendFPPCommandPacket\n");
    for (auto a : m_plugins) {
        a->SendFPPCommandPacket(host, cmd, args);
    }
    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    InitControlPacket(cpkt);
    cpkt->pktType = CTRL_PKT_FPPCOMMAND;

    int pos = sizeof(ControlPkt);
    outBuf[pos++] = args.size();
    strcpy(&outBuf[pos], host.c_str());
    pos += host.length() + 1;
    strcpy(&outBuf[pos], cmd.c_str());
    pos += cmd.length() + 1;
    for (auto& a : args) {
        strcpy(&outBuf[pos], a.c_str());
        pos += a.length() + 1;
    }
    cpkt->extraDataLen = pos - sizeof(ControlPkt);

    if (host != "" && host.find(",") == std::string::npos) {
        if (MyHostMatches(host, m_hostname, m_localSystems)) {
            // only targetting myself, just run and don't send the packet
            CommandManager::INSTANCE.run(cmd, args);
        } else {
            SendUnicastPacket(host, outBuf, pos);
        }
    } else {
        SendControlPacket(outBuf, pos);
        // the packet won't loop back so if it's supposed to run on this host as well,
        // we need to force it
        if (host == "" || MyHostMatches(host, m_hostname, m_localSystems)) {
            CommandManager::INSTANCE.run(cmd, args);
        }
    }
}

MultiSyncStats::MultiSyncStats(std::string ip, std::string host) :
    sourceIP(ip),
    hostname(host),
    pktCommand(0),
    pktSyncSeqOpen(0),
    pktSyncSeqStart(0),
    pktSyncSeqStop(0),
    pktSyncSeqSync(0),
    pktSyncMedOpen(0),
    pktSyncMedStart(0),
    pktSyncMedStop(0),
    pktSyncMedSync(0),
    pktBlank(0),
    pktPing(0),
    pktPlugin(0),
    pktFPPCommand(0),
    pktError(0) {
    lastReceiveTime = time(NULL);
}

Json::Value MultiSyncStats::toJSON() {
    Json::Value result;

    result["sourceIP"] = sourceIP;
    result["hostname"] = hostname;

    char timeStr[34];
    memset(timeStr, 0, sizeof(timeStr));
    struct tm tm;
    localtime_r(&lastReceiveTime, &tm);
    snprintf(timeStr, sizeof(timeStr), "%4d-%.2d-%.2d %.2d:%.2d:%.2d",
             1900 + tm.tm_year, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);

    result["lastReceiveTime"] = timeStr;
    result["pktCommand"] = pktCommand;
    result["pktSyncSeqOpen"] = pktSyncSeqOpen;
    result["pktSyncSeqStart"] = pktSyncSeqStart;
    result["pktSyncSeqStop"] = pktSyncSeqStop;
    result["pktSyncSeqSync"] = pktSyncSeqSync;
    result["pktSyncMedOpen"] = pktSyncMedOpen;
    result["pktSyncMedStart"] = pktSyncMedStart;
    result["pktSyncMedStop"] = pktSyncMedStop;
    result["pktSyncMedSync"] = pktSyncMedSync;
    result["pktBlank"] = pktBlank;
    result["pktPing"] = pktPing;
    result["pktPlugin"] = pktPlugin;
    result["pktFPPCommand"] = pktFPPCommand;
    result["pktError"] = pktError;

    return result;
}
