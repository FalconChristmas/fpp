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

#include "Warnings.h" // WarningHolder -- needed directly for NOPCH builds

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

#include "CurlManager.h"
#include "FileMonitor.h"
#include "NetworkController.h"
#include "NetworkMonitor.h"
#include "Player.h"
#include "Plugins.h"
#include "Sequence.h"
#include "command.h"
#include "common.h"
#include "fppversion.h"
#include "log.h"
#include "settings.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "channeloutput/channeloutputthread.h"
#include "e131bridge.h"
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
    // gethostbyname()/inet_ntoa() return pointers into static, per-process
    // buffers and are not thread-safe. MultiSync resolves hosts from several
    // threads concurrently (e.g. PingSingleRemoteViaHTTP and the main-loop
    // ProcessControlPacket path), and a concurrent call could corrupt the
    // static hostent, leaving h_addr dangling and crashing here. getaddrinfo()
    // and inet_ntop() are reentrant. We still resolve to the first IPv4 address
    // and rewrite target as a dotted-quad, because callers depend on that form
    // (split(target, '.') and inet_addr(target)).
    struct addrinfo hints{};
    hints.ai_family = AF_INET;      // IPv4 only -- callers expect a dotted-quad
    hints.ai_socktype = SOCK_DGRAM; // one result per address, not one per socktype

    struct addrinfo* res = nullptr;
    if (getaddrinfo(target.c_str(), nullptr, &hints, &res) != 0 || res == nullptr) {
        return false;
    }
    char buf[INET_ADDRSTRLEN] = {0};
    struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
    const char* str = inet_ntop(AF_INET, &addr->sin_addr, buf, sizeof(buf));
    freeaddrinfo(res);
    if (!str) {
        return false;
    }
    target = buf;
    return true;
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
    // UUID precedence.  A device that reports its own UUID always wins; the
    // MAC-derived identity below is only ever a stand-in for one, so it must
    // never overwrite a real UUID, and a real UUID arriving later must replace
    // it.
    if (uuid != "Unknown" && uuid != "") {
        if (!startsWith(uuid, MAC_UUID_PREFIX) || this->uuid.empty() ||
            startsWith(this->uuid, MAC_UUID_PREFIX)) {
            this->uuid = uuid;
        }
    }

    // Nothing reported a UUID for this device.  That is the normal case for
    // every controller that isn't full FPP: the ping packet carries no UUID
    // field at all, so ProcessPingPacket() has none to pass on, and
    // NetworkController only fills one in for an FPP instance.  Without an
    // identity the UI has to key the row on the hostname, which collides as
    // soon as two controllers ship with the same default name and changes
    // under it whenever someone renames one.
    //
    // The MAC is the stable identity such a device does have, and the kernel
    // already knows it for anything on a directly attached subnet.  Only look
    // it up while we still have nothing -- once an identity of either kind is
    // recorded this stops running, so it costs one small read per device
    // rather than one per ping.
    if (this->uuid.empty()) {
        // The parameter, not this->address: on a system being created that
        // member is still empty here, and is only assigned further down.
        std::string mac = GetMacForAddress(address);
        if (!mac.empty()) {
            this->uuid = MAC_UUID_PREFIX + mac;
        }
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
    if (!ranges.empty() && (ranges != "0-0" || this->ranges.empty())) {
        this->ranges = ranges;
    }

    if (!this->multiSync && multiSync) {
        this->multiSync = true;
    }

    std::vector<std::string> parts = split(address, '.');
    // ipa-ipd hold the legacy IPv4 octets used in the (IPv4-only) ping packet.
    // An IPv6 peer has no such octets, so skip the IPv4 hostname lookup that
    // would only fail and log an error; the octets stay zeroed below.
    if (parts.size() != 4 && address.find(':') == std::string::npos) {
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

    // A system is a valid target for "Send MultiSync to ALL KNOWN remotes via
    // Unicast" only if it's a full FPP instance (type below the 0x80 boundary
    // that separates full-FPP systems from external controllers) running in
    // Remote mode.  This deliberately excludes WLED, ESPixelStick, Falcon and
    // Experience controllers, xSchedule, and any FPP instance acting as a
    // player/master.  We intentionally do NOT require that the system was
    // learned via the UDP MultiSync protocol so that FPP remotes discovered
    // via HTTP (e.g. on a network segment where multicast is blocked - the
    // very case this mode exists for) are still targeted.
    // JBoards also qualifies: a MultiSync remote that does not run fppd.
    this->supportsUnicast = ((this->type < kSysTypeFalconController) ||
                             (this->type == kSysTypeJBoards)) &&
                            (this->fppMode == REMOTE_MODE);
}

static void SetIfNotEmpty(Json::Value& v, const char* key, const std::string& s) {
    if (!s.empty()) {
        v[key] = s;
    }
}

Json::Value MultiSyncSystemInfo::toJSON() const {
    Json::Value v;
    SetIfNotEmpty(v, "Platform", platform);
    SetIfNotEmpty(v, "Variant", variant);
    SetIfNotEmpty(v, "SubPlatform", subPlatform);
    SetIfNotEmpty(v, "OSVersion", osVersion);
    SetIfNotEmpty(v, "OSRelease", osRelease);
    SetIfNotEmpty(v, "Kernel", kernel);
    SetIfNotEmpty(v, "HostDescription", hostDescription);
    SetIfNotEmpty(v, "backgroundColor", backgroundColor);
    SetIfNotEmpty(v, "Branch", branch);
    SetIfNotEmpty(v, "LocalGitVersion", localGitVersion);
    SetIfNotEmpty(v, "RemoteGitVersion", remoteGitVersion);
    SetIfNotEmpty(v, "UpgradeSource", upgradeSource);
    if (channelInputsEnabled >= 0) {
        v["channelInputsEnabled"] = channelInputsEnabled ? true : false;
    }
    if (channelOutputsEnabled >= 0) {
        v["channelOutputsEnabled"] = channelOutputsEnabled ? true : false;
    }
    if (!ips.empty()) {
        Json::Value a(Json::arrayValue);
        for (auto& ip : ips) {
            a.append(ip);
        }
        v["IPs"] = a;
    }
    return v;
}

Json::Value MultiSyncCapeInfo::toJSON() const {
    Json::Value v;
    v["present"] = present;
    if (!present) {
        return v;
    }
    SetIfNotEmpty(v, "id", id);
    SetIfNotEmpty(v, "name", name);
    SetIfNotEmpty(v, "description", description);
    SetIfNotEmpty(v, "version", version);
    SetIfNotEmpty(v, "designer", designer);
    // Only emitted when the cape opts out, so the common case adds no bytes.
    if (!sendStats) {
        v["sendStats"] = 0;
    }
    Json::Value vendor;
    SetIfNotEmpty(vendor, "name", vendorName);
    SetIfNotEmpty(vendor, "url", vendorURL);
    SetIfNotEmpty(vendor, "email", vendorEmail);
    SetIfNotEmpty(vendor, "image", vendorImage);
    if (!vendor.empty()) {
        v["vendor"] = vendor;
    }
    return v;
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
        system["channelInputsEnabled"] = InputsEnabled();
        system["channelOutputsEnabled"] = HasUniverseOutputs();
    }

    // Detail fetched over HTTP from FPP remotes (see CheckSystemInfoRefreshes).
    // Absent for the local systems -- a browser rendering the multisync page
    // already has all of this for the box serving the page -- and for anything
    // that isn't a full FPP instance.
    if (systemInfo.valid) {
        system["systemInfo"] = systemInfo.toJSON();
    }
    if (capeInfo.valid) {
        system["capeInfo"] = capeInfo.toJSON();
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

    // Keep the configured-output-range cache (used to backfill ranges for
    // non-FPP remotes in GetSystems()) in sync with co-universes.json. Watching
    // the file confines the file read + hostname resolution to actual config
    // changes instead of every GetSystems() call. TriggerFileChanged() does the
    // initial load now.
    std::string coUniversesFile = FPP_DIR_CONFIG("/co-universes.json");
    FileMonitor::INSTANCE.AddFile("MultiSync/co-universes.json", coUniversesFile,
                                  [this]() { ReloadConfiguredOutputRanges(); })
        .TriggerFileChanged(coUniversesFile);

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
            setupMulticastReceive(true);
            if (changed) {
                FillLocalSystemInfo();
                Ping(0, false);
            }
        }
    };
    NetworkMonitor::INSTANCE.registerCallback(f);

    // The sync send methods and the unicast remote list are picked up live.
    // fppd already re-reads /media/settings whenever the web UI writes it
    // (FileMonitor in fppd.cpp), so all that was missing was rebuilding the
    // destination list these settings feed -- editing the remote list on the
    // MultiSync page no longer needs an fppd restart to take effect.  The
    // control socket itself is unaffected by any of them; if it isn't open yet
    // (MultiSync disabled, or nothing has needed it), OpenControlSockets()
    // reads the current settings when it does open.
    for (const char* s : { "MultiSyncRemotes", "MultiSyncExtraRemotes",
                           "MultiSyncBroadcast", "MultiSyncMulticast", "MultiSyncUnicast" }) {
        registerSettingsListener("MultiSync", s, [this](const std::string& v) {
            if (m_controlSock >= 0) {
                ReloadSyncDestinations();
            }
        });
    }

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

    // Loopback is this box finding itself, not a discoverable peer.  Avahi
    // resolves our own _fppd._udp advertisement to 127.0.0.1, so mDNS hands it
    // straight back to us; a channel output or an HTTP discovery subnet aimed
    // at localhost does the same.  The resulting entry is useless to every
    // consumer of the systems list -- no peer can reach it, and the multisync
    // page drops the row rather than render a link a browser cannot follow.
    // Reject it here, at the one place m_remoteSystems ever grows, so no
    // discovery path can reintroduce it.  Local interface addresses come from
    // FillInInterfaces(), which already skips "lo", so this never rejects one
    // of our own entries.
    if (IsLoopbackAddress(address) || IsLoopbackAddress(ipForAddress)) {
        LogDebug(VB_SYNC, "Ignoring loopback address %s in discovery\n", address.c_str());
        return;
    }

    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
    bool found = false;
    bool unicastChanged = false;
    for (auto& sys : m_remoteSystems) {
        if ((address == sys.address || ipForAddress == sys.address) &&
            ((hostname == sys.hostname) ||
             (address == sys.hostname) ||
             (hostname == sys.address))) {
            found = true;
            bool wasUnicast = sys.supportsUnicast;
            sys.update(type, majorVersion, minorVersion, fppMode, address, hostname, version, model, ranges, uuid, multiSync, sendingMultiSync);
            sys.lastSeenStr = timeStr;
            sys.lastSeen = t;
            unicastChanged |= (wasUnicast != sys.supportsUnicast);
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
        unicastChanged |= sys.supportsUnicast;
        m_remoteSystems.push_back(sys);
    }

    ReconcileDeviceIdentity(hostname, fppMode);

    // If a remote became (or stopped being) a unicast target, refresh the
    // cached "all known remotes" destination list.  The snapshot is taken here
    // while we still hold m_systemsLock, but the rebuild has to run after it is
    // released: UpdateUnicastDestinations() takes m_unicastUpdateLock, which is
    // never acquired while m_systemsLock is held.
    bool rebuildUnicast = m_sendUnicast && unicastChanged;
    std::vector<std::string> unicastAddrs;
    if (rebuildUnicast) {
        for (auto& sys : m_remoteSystems) {
            if (sys.supportsUnicast) {
                unicastAddrs.push_back(sys.address);
            }
        }
    }
    lock.unlock();
    if (rebuildUnicast) {
        UpdateUnicastDestinations(unicastAddrs);
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
    if (startsWith(model, "Raspberry Pi 5") || startsWith(model, "Raspberry Pi Compute Module 5"))
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
    // This is fed both the device-tree model ("BeagleBoard.org PocketBeagle2")
    // and, for HTTP-discovered controllers, a remote's 'Variant' setting, which
    // spells the board with a space ("PocketBeagle 2", "PocketBeagle 2
    // Industrial").  Both spellings have to be caught here, and before the
    // PocketBeagle test below -- otherwise a PocketBeagle 2 falls through and
    // gets reported as a PocketBeagle 1.
    if (contains(model, "PocketBeagle2") || contains(model, "PocketBeagle 2") ||
        contains(model, "BeaglePlay")) {
        return kSysTypeFPPPocketBeagle2;
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
    if (FileExists("/etc/fpp/container")) {
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
                    inet_ntop(AF_INET, &sa->sin_addr, addressBuf, INET_ADDRSTRLEN);
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
        if (address.empty()) {
            // A failed GetInterfaceAddress()/inet_ntop() leaves the buffer empty;
            // skip it rather than record a bogus local system.
            continue;
        }
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
            if (parts.size() >= 4) {
                newSystem.ipa = atoi(parts[0].c_str());
                newSystem.ipb = atoi(parts[1].c_str());
                newSystem.ipc = atoi(parts[2].c_str());
                newSystem.ipd = atoi(parts[3].c_str());
            } else {
                newSystem.ipa = 0;
                newSystem.ipb = 0;
                newSystem.ipc = 0;
                newSystem.ipd = 0;
            }
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
    case kSysTypeFalconF32v5:
        return "Falcon F32v5";
    case kSysTypeFalconF48v5:
        return "Falcon F48v5";
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
    case kSysTypeExperienceAURORACORE16:
        return "LOR AURORA CORE 16";
    case kSysTypeExperienceVIVID8:
        return "YPS VIVID 8";
    case kSysTypeExperiencePIXELLINK4:
        return "Genius PIXEL LINK 4";
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
    case kSysTypeFPPPocketBeagle2:
        return "PocketBeagle2";
    case kSysTypeFPPSanCloudBeagleBoneEnhanced:
        return "SanCloud BeagleBone Enhanced";
    case kSysTypexSchedule:
        return "xSchedule";
    case kSysTypeESPixelStick:
        return "ESPixelStick-ESP8266";
    case kSysTypeESPixelStickESP32:
        return "ESPixelStick-ESP32";
    case kSysTypeBaldrick:
        return "Baldrick";
    case kSysTypeJBoards:
        return "JBoards";
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
        int last = ranges[0].first + ranges[0].second - 1;
        for (int x = 1; x < ranges.size(); x++) {
            int gap = ranges[x].first - last;
            if (gap < minGap) {
                minIdx = x;
                minGap = gap;
            }
            last = ranges[x].first + ranges[x].second - 1;
        }
        if (minIdx > 0) {
            int newLast = ranges[minIdx].first + ranges[minIdx].second;
            ranges[minIdx - 1].second = newLast - ranges[minIdx - 1].first;
            ranges.erase(ranges.begin() + minIdx);
        }
        range = createRanges(ranges, 999999);
    }
    return range;
}

// Build a map of configured-output controller address -> channel range string.
// Output-only controllers (WLED, Falcon, etc.) generally don't report their own
// channel ranges, but FPP knows them from its universe output configuration, so
// we backfill the ranges from our own config (issue #2667).  Each address is
// keyed both by the raw configured value and by its resolved IP so that we can
// match a system regardless of whether it was discovered/stored by hostname or
// by IP.
static std::map<std::string, std::string> GetConfiguredOutputRanges() {
    std::map<std::string, std::vector<std::pair<uint32_t, uint32_t>>> byAddress;

    if (FileExists(FPP_DIR_CONFIG("/co-universes.json"))) {
        Json::Value outputs = LoadJsonFromFile(FPP_DIR_CONFIG("/co-universes.json"), JsonRoot::Object);
        for (const auto& co : outputs["channelOutputs"]) {
            if (!co.isMember("universes")) {
                continue;
            }
            for (const auto& u : co["universes"]) {
                if (!u.isMember("address") || !u.isMember("startChannel") || !u.isMember("channelCount")) {
                    continue;
                }
                std::string address = u["address"].asString();
                uint32_t count = u["channelCount"].asUInt();
                if (address.empty() || count == 0) {
                    continue;
                }
                byAddress[address].emplace_back(u["startChannel"].asUInt(), count);
            }
        }
    }

    std::map<std::string, std::string> result;
    for (auto& [address, ranges] : byAddress) {
        std::sort(ranges.begin(), ranges.end());
        // merge contiguous/overlapping ranges so the reported string is clean
        std::vector<std::pair<uint32_t, uint32_t>> merged;
        for (const auto& r : ranges) {
            if (!merged.empty() && r.first <= (merged.back().first + merged.back().second)) {
                uint32_t end = std::max(merged.back().first + merged.back().second, r.first + r.second);
                merged.back().second = end - merged.back().first;
            } else {
                merged.push_back(r);
            }
        }
        std::string rangeStr = createRanges(merged, 999999);
        result[address] = rangeStr;
        std::string ip = address;
        if (GetIPForHost(ip) && ip != address) {
            result[ip] = rangeStr;
        }
    }
    return result;
}

void MultiSync::ReloadConfiguredOutputRanges() {
    // Build the map outside the lock: GetConfiguredOutputRanges() reads
    // co-universes.json and may run blocking DNS lookups to resolve configured
    // output hostnames. Then swap it in under the lock that guards GetSystems().
    // This runs once at startup and again only when co-universes.json changes.
    std::map<std::string, std::string> ranges = GetConfiguredOutputRanges();
    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
    m_configuredOutputRanges = std::move(ranges);
}

Json::Value MultiSync::GetSystems(bool localOnly, bool timestamps) {
    Json::Value result;
    Json::Value systems(Json::arrayValue);

    auto ranges = GetOutputRangesSnapshot(true);
    std::string range = createRanges(*ranges, 999999);

    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
    for (auto& sys : m_localSystems) {
        sys.ranges = range;
        systems.append(sys.toJSON(true, timestamps));
    }
    if (!localOnly) {
        // m_configuredOutputRanges is rebuilt only when co-universes.json changes
        // (see ReloadConfiguredOutputRanges()), so this is a plain in-memory map
        // lookup -- no per-call file read and, crucially, no blocking DNS. (This
        // endpoint is polled frequently by the UI and is also invoked indirectly
        // by the config backup generated on every settings change, so the old
        // resolve-per-remote-per-call cost was very visible.) We only backfill a
        // range for remotes that didn't advertise one of their own, and we do it
        // in the emitted JSON rather than mutating the stored system so a later
        // co-universes.json change is still reflected.
        for (auto& sys : m_remoteSystems) {
            Json::Value sysJson = sys.toJSON(false, timestamps);
            if ((sys.ranges.empty() || sys.ranges == "0-0") && !m_configuredOutputRanges.empty()) {
                auto it = m_configuredOutputRanges.find(sys.address);
                if (it == m_configuredOutputRanges.end()) {
                    it = m_configuredOutputRanges.find(sys.hostname);
                }
                if (it != m_configuredOutputRanges.end()) {
                    sysJson["channelRanges"] = it->second;
                }
            }
            systems.append(sysJson);
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
    // m_syncMaster is rewritten from the sync-receive path on the main loop, so
    // it is only ever touched under m_statsLock.  Copy it out here and use the
    // copy below: m_statsLock and m_systemsLock are never held together.
    std::string syncMaster = m_syncMaster;
    slock.unlock();

    result["systems"] = systems;

    std::string masterHostname;

    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
    for (auto& sys : m_remoteSystems) {
        if (sys.address == syncMaster)
            masterHostname = sys.hostname;
    }
    lock.unlock();

    result["masterIP"] = syncMaster;
    result["masterHostname"] = masterHostname;

    return result;
}

void MultiSync::ResetSyncStats() {
    std::unique_lock<std::recursive_mutex> slock(m_statsLock);
    for (auto& a : m_syncStats) {
        MultiSyncStats* stats = (MultiSyncStats*)a.second;
        stats->pktCommand = 0;
        stats->pktSyncSeqOpen = 0;
        stats->pktSyncSeqStart = 0;
        stats->pktSyncSeqStop = 0;
        stats->pktSyncSeqSync = 0;
        stats->pktSyncMedOpen = 0;
        stats->pktSyncMedStart = 0;
        stats->pktSyncMedStop = 0;
        stats->pktSyncMedSync = 0;
        stats->pktBlank = 0;
        stats->pktPing = 0;
        stats->pktPlugin = 0;
        stats->pktFPPCommand = 0;
        stats->pktError = 0;
    }
}

void MultiSync::Discover() {
    Ping(1);
    PerformHTTPDiscovery();
}

void MultiSync::PerformHTTPDiscovery() {
    std::string subnetsStr = getSetting("MultiSyncHTTPSubnets");

    if (FileExists(FPP_DIR_CONFIG("/co-universes.json"))) {
        Json::Value outputs = LoadJsonFromFile(FPP_DIR_CONFIG("/co-universes.json"), JsonRoot::Object);
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
                    for (int i = 0; i < ips; i++) {
                        ia.s_addr = ntohl(firstIP + i);
                        inet_ntop(AF_INET, &ia, ip, sizeof(ip));
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
    for (auto& ip : ipSet) {
        LogExcess(VB_SYNC, "  %s\n", ip.c_str());
        handles[ips] = curl_easy_init();
        ipList[ips] = ip;
        m_httpResponses.erase(ip);

        // ip may be a hostname, so this must not be a fixed-size buffer
        // (a fixed buffer would silently truncate hostnames and they'd
        //  never get discovered - see issue #2667)
        std::string url = "http://" + ip + "/";
        curl_easy_setopt(handles[ips], CURLOPT_URL, url.c_str());
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
                    if (idx == ips) {
                        // Handle not one of ours; nothing to update in ipList, but
                        // still remove/cleanup so it isn't leaked.
                        curl_multi_remove_handle(multi_handle, e);
                        curl_easy_cleanup(e);
                        continue;
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
            // Fall through to the cleanup below instead of leaking the
            // remaining easy handles and the multi handle.
            break;
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

void MultiSync::WriteRuntimeInfoFile() {
    Json::Value v = GetSystems(true, false);
    std::string filename = FPP_DIR_MEDIA("/fpp-info.json");
    Json::Value systems = v["systems"];

    // If there is no network, then delete the file
    // to avoid confusion.
    if (systems.empty()) {
        const int ok = remove(filename.c_str());
        if (!ok) {
            LogWarn(VB_ALL, "Failed to remove file: %s\n", filename.c_str());
        }
        LogInfo(VB_ALL, "Not creating %s as no IP addresses found\n", filename.c_str());
        return;
    }

    std::string addresses = "";
    for (int x = 0; x < systems.size(); x++) {
        if (addresses != "") {
            addresses += ",";
        }
        addresses += systems[x]["address"].asString();
    }
    Json::Value local = systems[0];
    local.removeMember("address");
    local["addresses"] = addresses;

    SaveJsonToFile(local, filename);
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
    setupMulticastReceive(discover ? true : false);

    // update the range for local systems so it's accurate
    auto ranges = GetOutputRangesSnapshot(true);
    std::string range = createRanges(*ranges, 120);
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
        bool unicastChanged = false;
        // PingSingleRemoteViaHTTP -> UpdateSystem can push_back onto
        // m_remoteSystems (the recursive mutex doesn't protect against our own
        // thread), reallocating the vector and invalidating `it` mid-loop.
        // Collect the addresses (by value) and do the HTTP probes after the
        // loop, outside the lock - they are blocking curl calls anyway.
        std::vector<std::string> httpPingAddresses;
        for (auto it = m_remoteSystems.begin(); it != m_remoteSystems.end();) {
            if (it->lastSeen < timeoutRemove) {
                LogInfo(VB_SYNC, "Have not seen %s in over 2 hours, removing\n", it->address.c_str());
                if (it->lastSeen < timeoutRePingAll) {
                    superLongGap = true;
                }
                unicastChanged |= it->supportsUnicast;
                it = m_remoteSystems.erase(it);
            } else if (it->lastSeen < timeoutRePing) {
                if (it->multiSync) {
                    PingSingleRemote(*it, 1);
                } else {
                    httpPingAddresses.push_back(it->address);
                }
                ++it;
            } else {
                ++it;
            }
        }
        // Drop any removed remotes from the cached unicast destination list.
        // Snapshot the surviving targets under m_systemsLock, rebuild after
        // releasing it -- m_unicastUpdateLock is never acquired while
        // m_systemsLock is held.
        bool rebuildUnicast = m_sendUnicast && unicastChanged;
        std::vector<std::string> unicastAddrs;
        if (rebuildUnicast) {
            for (auto& sys : m_remoteSystems) {
                if (sys.supportsUnicast) {
                    unicastAddrs.push_back(sys.address);
                }
            }
        }
        lock.unlock();
        if (rebuildUnicast) {
            UpdateUnicastDestinations(unicastAddrs);
        }
        if (!httpPingAddresses.empty()) {
            // These are blocking curl probes (connect timeout of a couple
            // seconds each) to remotes we haven't heard from in a while.  This
            // runs from the fppd main loop, which is the same thread that drains
            // the MultiSync control socket (ProcessControlPacket).  Doing the
            // probes inline stalls sync-packet processing for the duration of the
            // timeouts, which on a remote shows up as the output freezing for a
            // second or two and then jumping to catch up.  Run them on a
            // short-lived detached thread instead; UpdateSystem() takes
            // m_systemsLock so it is safe to touch the systems list from here.
            bool expected = false;
            if (m_httpPingInProgress.compare_exchange_strong(expected, true)) {
                std::thread([this, addrs = std::move(httpPingAddresses)]() {
                    SetThreadName("FPP-HTTPPing");
                    for (auto& address : addrs) {
                        PingSingleRemoteViaHTTP(address);
                    }
                    m_httpPingInProgress = false;
                }).detach();
            }
        }
    }
    if (superLongGap) {
        Ping(1);
    }

    CheckSystemInfoRefreshes();
}

// How often a remote's HTTP-fetched detail is refreshed, how soon a failed
// attempt is retried, and how many fetches may be outstanding at once.  The
// cap matters on a large show network: without it the first sweep after
// discovery would open a connection to every remote at the same moment, from a
// player that may be a single-core BeagleBone.  The rest are simply picked up
// by a later sweep.
#define INFO_REFRESH_INTERVAL (30 * 60)
#define INFO_RETRY_INTERVAL (5 * 60)
#define INFO_SCAN_INTERVAL 5
#define INFO_MAX_IN_FLIGHT 4

// Every fetch is delayed by a random slice of a window that grows with the size
// of the known fleet.  INFO_MAX_IN_FLIGHT bounds what one player sends, but the
// dangerous direction is the other one: every peer sees the same discover ping,
// so on a 100-instance show a single fppd restart would otherwise land ~200 HTTP
// requests on the box that just came up -- the box least able to serve them, and
// on a BeagleBone that is a real stall.  A whole-show power-on is the same
// pattern squared, with every box both hammering and being hammered.  Budgeting
// roughly one second of window per known system holds the aggregate arrival rate
// at the target near two requests a second no matter how large the show is.
#define INFO_JITTER_BASE 5
#define INFO_JITTER_MAX 300

// The multisync page needs a fair amount of slow-changing detail about every
// remote -- OS version, host description, background color, git branch/commit,
// whether channel inputs/outputs are enabled, what cape is installed.  It used
// to fetch all of it from the browser, at least one HTTP request per remote per
// page load, which is why the table visibly reflowed as the answers trickled
// in.  fppd already knows every remote and can collect it once per remote per
// INFO_REFRESH_INTERVAL over the async CurlManager, so the UI gets it in the
// very first GetSystems() response.
//
// Runs on the fppd main loop (from PeriodicPing), which is also the thread that
// drains the CurlManager completions -- so a callback below can never interleave
// with this sweep.  It must stay non-blocking: the same loop feeds the sync
// output, which is why this uses CurlManager rather than the blocking
// urlHelper() path PingSingleRemoteViaHTTP() uses from its own thread.
void MultiSync::CheckSystemInfoRefreshes() {
    time_t now = time(nullptr);
    if (now < m_nextInfoScan) {
        return;
    }
    m_nextInfoScan = now + INFO_SCAN_INTERVAL;

    std::vector<std::string> toFetch;
    {
        std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
        // We routinely discover ourselves as a "remote" -- over loopback, and
        // over any address that isn't one of the interface addresses in
        // m_localSystems (a global IPv6 address, typically).  Those entries
        // merge into the local row in the UI, which already has everything
        // about the box serving the page, so probing them is pure waste.
        std::set<std::string> localUUIDs;
        for (auto& sys : m_localSystems) {
            if (!sys.uuid.empty() && sys.uuid != "Unknown") {
                localUUIDs.insert(sys.uuid);
            }
        }
        for (size_t idx = 0; idx < m_remoteSystems.size(); ++idx) {
            MultiSyncSystem& sys = m_remoteSystems[idx];
            // Only full FPP instances: these are FPP's own API endpoints, and
            // anything at or above kSysTypeFalconController is a third-party
            // controller that would just 404 (or worse, be confused by) them.
            if (sys.type == kSysTypeUnknown || sys.type >= kSysTypeFalconController) {
                continue;
            }
            if (sys.infoFetchPending) {
                continue;
            }
            if (localUUIDs.find(sys.uuid) != localUUIDs.end()) {
                continue;
            }
            // Link-local addresses are reachable from here but not from a
            // browser (the IPv6 zone id is specific to this host), which is why
            // the multisync page drops those rows.  Loopback never reaches this
            // list at all -- UpdateSystem() rejects it.
            if (startsWith(sys.address, "169.254.") || startsWith(sys.address, "fe80:")) {
                continue;
            }
            // A device with more than one NIC has one entry per address.  Act
            // on it once and stamp every sibling entry the same way, otherwise
            // the next sweep would pick the siblings up separately;
            // ForEachRemoteMatching() fans the single answer back out to all of
            // them.
            std::string uuid = sys.uuid;
            if (uuid == "Unknown") {
                uuid.clear();
            }
            std::string address = sys.address;
            auto stampSiblings = [&](const std::function<void(MultiSyncSystem&)>& apply) {
                for (auto& other : m_remoteSystems) {
                    if (other.address == address || (!uuid.empty() && other.uuid == uuid)) {
                        apply(other);
                    }
                }
            };

            // infoNextFetch == 0 means due but unscheduled: a remote we have
            // just discovered, or one InvalidateSystemInfo() marked stale after
            // its discover ping.  A changed version string means the remote was
            // upgraded under us, which rewrites most of what we cached -- and on
            // a fleet upgrade that fires everywhere at once, so it earns the
            // same treatment.  Either way, pick a jittered time and come back
            // for it on a later sweep instead of fetching right now.
            if (sys.infoNextFetch == 0 || sys.infoFetchedVersion != sys.version) {
                // Entry count, not device count: a multi-NIC box contributes
                // several, as do non-FPP controllers.  That only ever widens
                // the window, which is the safe direction.
                int window = INFO_JITTER_BASE + (int)m_remoteSystems.size();
                if (window > INFO_JITTER_MAX) {
                    window = INFO_JITTER_MAX;
                }
                time_t when = now + (FPPrand() % window);
                stampSiblings([&](MultiSyncSystem& other) {
                    other.infoNextFetch = when;
                    other.infoFetchedVersion = other.version;
                });
                continue;
            }
            if (now < sys.infoNextFetch) {
                continue;
            }
            // Cap only what is actually sent.  Scheduling above is free and has
            // to keep running even while the cap is reached, or a large fleet
            // would never get its jittered times assigned.  Anything held back
            // here is simply picked up by the next sweep.
            if ((int)toFetch.size() + m_infoFetchesInFlight >= INFO_MAX_IN_FLIGHT) {
                continue;
            }

            stampSiblings([&](MultiSyncSystem& other) {
                other.infoFetchPending = true;
                other.infoFetchedVersion = other.version;
            });
            toFetch.push_back(address);
        }
    }
    for (auto& address : toFetch) {
        FetchSystemInfo(address);
    }
}

void MultiSync::InvalidateSystemInfo(const std::string& address) {
    ForEachRemoteMatching(address, [](MultiSyncSystem& sys) {
        // Due, but unscheduled: the next sweep gives it a jittered time.  Never
        // fetch straight from here -- this runs for every peer that heard the
        // same discover ping, which is exactly the pile-on being avoided.
        sys.infoNextFetch = 0;
    });
}

// One device, one identity.
//
// A box with several addresses gets an entry per address, and a real UUID only
// ever reaches the entry that was actually probed for it -- UpdateSystem()
// matches on the address.  The others keep whatever they were given, which
// since MAC-derived identities exist means a stand-in rather than nothing.  Two
// different identities for one device is worse than none: the UI keys a row on
// the UUID and draws the device twice, and the statistics count it twice, which
// defeats the deduplication the identity exists to provide.
//
// So once any entry knows a real UUID, every other entry for that device adopts
// it.  Guarded the way the UI guards its own hostname fallback: a hostname
// claimed by more than one real UUID is not identifying a device at all -- two
// controllers left on the default name -- so in that case nothing is touched.
//
// This is only about filling in for entries that have no identity of their own.
// A real UUID is never overwritten by another.
void MultiSync::ReconcileDeviceIdentity(const std::string& hostname, FPPMode fppMode) {
    if (hostname.empty()) {
        return;
    }
    auto isStandIn = [](const std::string& u) {
        return u.empty() || startsWith(u, MAC_UUID_PREFIX);
    };

    std::string real;
    for (auto& sys : m_remoteSystems) {
        if (sys.hostname != hostname || sys.fppMode != fppMode || isStandIn(sys.uuid)) {
            continue;
        }
        if (real.empty()) {
            real = sys.uuid;
        } else if (real != sys.uuid) {
            return; // hostname shared by more than one device
        }
    }
    if (real.empty()) {
        return;
    }
    for (auto& sys : m_remoteSystems) {
        if (sys.hostname == hostname && sys.fppMode == fppMode && isStandIn(sys.uuid)) {
            sys.uuid = real;
        }
    }
}

void MultiSync::ForEachRemoteMatching(const std::string& address,
                                      const std::function<void(MultiSyncSystem&)>& apply) {
    std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
    std::string uuid;
    for (auto& sys : m_remoteSystems) {
        if (sys.address == address) {
            uuid = sys.uuid;
            break;
        }
    }
    if (uuid == "Unknown") {
        uuid.clear();
    }
    for (auto& sys : m_remoteSystems) {
        if (sys.address == address || (!uuid.empty() && sys.uuid == uuid)) {
            apply(sys);
        }
    }
}

void MultiSync::FetchSystemInfo(const std::string& address) {
    std::string url = buildHttpURL(address, "/api/system/info");
    ++m_infoFetchesInFlight;
    // `address` is captured by value on purpose: m_remoteSystems is a vector of
    // values that UpdateSystem() push_back()s into, so any pointer or reference
    // to an entry can be dangling by the time this callback runs.  Everything
    // below re-finds the entries by address instead.
    CurlManager::INSTANCE.addGet(url, [this, address](int rc, const std::string& resp) {
        --m_infoFetchesInFlight;
        Json::Value v;
        if (rc != 200 || !LoadJsonFromString(resp, v, JsonRoot::Object)) {
            LogDebug(VB_SYNC, "Could not fetch system info from %s (rc: %d)\n", address.c_str(), rc);
            time_t retryAt = time(nullptr) + INFO_RETRY_INTERVAL;
            ForEachRemoteMatching(address, [retryAt](MultiSyncSystem& sys) {
                sys.infoFetchPending = false;
                // A zero here means InvalidateSystemInfo() ran while this fetch
                // was in flight -- the remote restarted mid-request, so what we
                // just read may already be wrong.  Leave it zero and let the
                // sweep schedule a fresh attempt.
                if (sys.infoNextFetch != 0) {
                    sys.infoNextFetch = retryAt;
                }
            });
            return;
        }

        MultiSyncSystemInfo info;
        info.valid = true;
        info.platform = v.get("Platform", "").asString();
        info.variant = v.get("Variant", "").asString();
        info.subPlatform = v.get("SubPlatform", "").asString();
        info.osVersion = v.get("OSVersion", "").asString();
        info.osRelease = v.get("OSRelease", "").asString();
        info.kernel = v.get("Kernel", "").asString();
        info.hostDescription = v.get("HostDescription", "").asString();
        info.backgroundColor = v.get("backgroundColor", "").asString();
        info.branch = v.get("Branch", "").asString();
        info.localGitVersion = v.get("LocalGitVersion", "").asString();
        info.remoteGitVersion = v.get("RemoteGitVersion", "").asString();
        info.upgradeSource = v.get("UpgradeSource", "").asString();
        if (JsonHas(v, "channelInputsEnabled")) {
            info.channelInputsEnabled = v["channelInputsEnabled"].asBool() ? 1 : 0;
        }
        if (JsonHas(v, "channelOutputsEnabled")) {
            info.channelOutputsEnabled = v["channelOutputsEnabled"].asBool() ? 1 : 0;
        }
        if (JsonHas(v, "IPs") && v["IPs"].isArray()) {
            for (const auto& ip : v["IPs"]) {
                if (ip.isString()) {
                    info.ips.push_back(ip.asString());
                }
            }
        }

        ForEachRemoteMatching(address, [&info](MultiSyncSystem& sys) {
            sys.systemInfo = info;
        });

        // Chain the cape fetch rather than issuing both at once: it reuses the
        // connection just established and halves the peak request count.
        // infoFetchPending stays set until it finishes so the sweep above does
        // not re-queue this remote in between.
        FetchCapeInfo(address);
    });
}

void MultiSync::FetchCapeInfo(const std::string& address) {
    std::string url = buildHttpURL(address, "/api/cape");
    ++m_infoFetchesInFlight;
    CurlManager::INSTANCE.addGet(url, [this, address](int rc, const std::string& resp) {
        --m_infoFetchesInFlight;
        MultiSyncCapeInfo cape;
        Json::Value v;
        if (rc == 200 && LoadJsonFromString(resp, v, JsonRoot::Object)) {
            cape.valid = true;
            cape.present = true;
            cape.id = v.get("id", "").asString();
            cape.name = v.get("name", "").asString();
            cape.description = v.get("description", "").asString();
            cape.version = v.get("version", "").asString();
            cape.designer = v.get("designer", "").asString();
            // Absent (including on every unsigned cape) means no opt-out.
            cape.sendStats = v.get("sendStats", 1).asInt() != 0;
            if (JsonHas(v, "vendor") && v["vendor"].isObject()) {
                const Json::Value& vendor = v["vendor"];
                cape.vendorName = vendor.get("name", "").asString();
                cape.vendorURL = vendor.get("url", "").asString();
                cape.vendorEmail = vendor.get("email", "").asString();
                cape.vendorImage = vendor.get("image", "").asString();
            }
        } else if (rc == 404) {
            // GetCapeInfo() answers 404 with {"id": "No Cape!"} when nothing is
            // installed.  Cache that: it is an answer, not a failure.  (An FPP
            // old enough to predate the route would 404 the same way and be
            // recorded as having no cape, which is what the UI shows for it
            // anyway.)
            cape.valid = true;
            cape.present = false;
        } else {
            // Anything else (connection failure, or an FPP old enough not to
            // have /api/cape at all) leaves capeInfo unset and untrusted.
            LogDebug(VB_SYNC, "Could not fetch cape info from %s (rc: %d)\n", address.c_str(), rc);
        }

        // End of the chain, so this is where the next refresh is scheduled --
        // the system info the caller already stored is good either way, so a
        // cape that could not be read doesn't earn the shorter retry interval.
        time_t nextAt = time(nullptr) + INFO_REFRESH_INTERVAL;
        ForEachRemoteMatching(address, [&cape, nextAt](MultiSyncSystem& sys) {
            if (cape.valid) {
                sys.capeInfo = cape;
            }
            sys.infoFetchPending = false;
            // Zero means the remote sent a discover ping while this fetch was
            // in flight, i.e. it restarted mid-request and what we just stored
            // may already be stale.  Don't paper over that with a 30 minute
            // interval; leave it due so the sweep reschedules.
            if (sys.infoNextFetch != 0) {
                sys.infoNextFetch = nextAt;
            }
        });
    });
}

void MultiSync::PingSingleRemoteViaHTTP(const std::string& address) {
    std::string url = buildHttpURL(address);
    std::string resp;

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

// The sync senders below all write filename into a fixed-size SyncPkt within a
// 2048-byte outBuf; a filename that wouldn't fit must be rejected before the
// strcpy rather than silently truncated or overflowed.
static bool FitsInSyncOutBuf(const std::string& filename, size_t bufSize) {
    return (sizeof(ControlPkt) + sizeof(SyncPkt) + filename.length() + 1) <= bufSize;
}

void MultiSync::SendSeqOpenPacket(const std::string& filename) {
    if (filename.empty()) {
        return;
    }
    if (!FitsInSyncOutBuf(filename, 2048)) {
        LogErr(VB_SYNC, "ERROR: SendSeqOpenPacket filename '%s' is too long to send\n", filename.c_str());
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
        return;
    }

    LogDebug(VB_SYNC, "SendSeqOpenPacket('%s')\n", filename.c_str());
    for (auto a : getPluginsCopy()) {
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
    if (!FitsInSyncOutBuf(filename, 2048)) {
        LogErr(VB_SYNC, "ERROR: SendSeqSyncStartPacket filename '%s' is too long to send\n", filename.c_str());
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
        return;
    }

    LogDebug(VB_SYNC, "SendSeqSyncStartPacket('%s')\n", filename.c_str());
    for (auto a : getPluginsCopy()) {
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
    if (!FitsInSyncOutBuf(filename, 2048)) {
        LogErr(VB_SYNC, "ERROR: SendSeqSyncStopPacket filename '%s' is too long to send\n", filename.c_str());
        return;
    }
    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send stop packet but sync socket is not open.\n");
        return;
    }
    LogDebug(VB_SYNC, "SendSeqSyncStopPacket(%s)\n", filename.c_str());

    for (auto a : getPluginsCopy()) {
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
    if (!FitsInSyncOutBuf(filename, 2048)) {
        LogErr(VB_SYNC, "ERROR: SendSeqSyncPacket filename '%s' is too long to send\n", filename.c_str());
        return;
    }
    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send sync packet but sync socket is not open.\n");
        return;
    }
    for (auto a : getPluginsCopy()) {
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
    if (!FitsInSyncOutBuf(filename, 2048)) {
        LogErr(VB_SYNC, "ERROR: SendMediaOpenPacket filename '%s' is too long to send\n", filename.c_str());
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
        return;
    }
    LogDebug(VB_SYNC, "SendMediaOpenPacket('%s')\n", filename.c_str());

    for (auto a : getPluginsCopy()) {
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
    if (!FitsInSyncOutBuf(filename, 2048)) {
        LogErr(VB_SYNC, "ERROR: SendMediaSyncStartPacket filename '%s' is too long to send\n", filename.c_str());
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send start packet but sync socket is not open.\n");
        return;
    }
    LogDebug(VB_SYNC, "SendMediaSyncStartPacket('%s')\n", filename.c_str());

    for (auto a : getPluginsCopy()) {
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
    if (!FitsInSyncOutBuf(filename, 2048)) {
        LogErr(VB_SYNC, "ERROR: SendMediaSyncStopPacket filename '%s' is too long to send\n", filename.c_str());
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send stop packet but sync socket is not open.\n");
        return;
    }
    LogDebug(VB_SYNC, "SendMediaSyncStopPacket(%s)\n", filename.c_str());
    for (auto a : getPluginsCopy()) {
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
    if (!FitsInSyncOutBuf(filename, 2048)) {
        LogErr(VB_SYNC, "ERROR: SendMediaSyncPacket filename '%s' is too long to send\n", filename.c_str());
        return;
    }

    if (m_controlSock < 0) {
        LogErr(VB_SYNC, "ERROR: Tried to send sync packet but sync socket is not open.\n");
        return;
    }

    for (auto a : getPluginsCopy()) {
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
    for (auto a : getPluginsCopy()) {
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
    if ((sizeof(ControlPkt) + (size_t)nlen + (size_t)len) > sizeof(outBuf)) {
        LogErr(VB_SYNC, "ERROR: Plugin data packet for '%s' (%d bytes) is too large to send\n",
               name.c_str(), len);
        return;
    }
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
    for (auto a : getPluginsCopy()) {
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
std::vector<MultiSyncPlugin*> MultiSync::getPluginsCopy() {
    std::unique_lock<std::mutex> lock(m_pluginsLock);
    return m_plugins;
}
void MultiSync::addMultiSyncPlugin(MultiSyncPlugin* p) {
    std::unique_lock<std::mutex> lock(m_pluginsLock);
    m_plugins.push_back(p);
}
void MultiSync::removeMultiSyncPlugin(MultiSyncPlugin* p) {
    std::unique_lock<std::mutex> lock(m_pluginsLock);
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

    // Idempotent, like WLEDAudioSync::Cleanup(): fppd's shutdown calls this and
    // ~MultiSync() calls it again at static-destruction time, by which point the
    // global SettingsConfig may already be gone -- locking its destroyed mutex
    // would throw out of a noexcept destructor.  Unregistering also acts as a
    // barrier: unregisterSettingsListener() takes the listener list's write
    // lock, which the firing loop holds for reading while a callback runs, so it
    // cannot return while ReloadSyncDestinations() is still in flight on the
    // settings-reload thread.
    if (!m_settingsListenersRemoved.exchange(true)) {
        for (const char* s : { "MultiSyncRemotes", "MultiSyncExtraRemotes",
                               "MultiSyncBroadcast", "MultiSyncMulticast", "MultiSyncUnicast" }) {
            unregisterSettingsListener("MultiSync", s);
        }
    }

    {
        std::unique_lock<std::mutex> pluginLock(m_pluginsLock);
        auto plugins = m_plugins;
        pluginLock.unlock();
        for (auto a : plugins) {
            a->ShutdownSync();
        }
    }
    {
        std::unique_lock<std::mutex> pluginLock(m_pluginsLock);
        m_plugins.clear();
    }

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
        WarningHolder::AddWarning(42, "MultiSync: could not open broadcast socket");
        return 0;
    }

    char loopch = 0;
    if (setsockopt(m_broadcastSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
               FPPstrerror(errno));
        return 0;
    }

    int broadcast = 1;
    if (setsockopt(m_broadcastSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        LogErr(VB_SYNC, "Error setting SO_BROADCAST: \n", FPPstrerror(errno));
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
        WarningHolder::AddWarning(42, "MultiSync: could not open control socket");
        return 0;
    }

    char loopch = 0;
    if (setsockopt(m_controlSock, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&loopch, sizeof(loopch)) < 0) {
        LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP: \n",
               FPPstrerror(errno));
        return 0;
    }

    ReloadSyncDestinations();

    FillInInterfaces();

    return 1;
}

void MultiSync::ReloadSyncDestinations() {
    // Same lock UpdateUnicastDestinations() uses, for the same reason: the
    // resolve step below is slow and runs without m_socketLock, so two
    // overlapping reloads could otherwise finish out of order and leave the
    // older result in place.  Holding it also keeps UpdateUnicastDestinations()
    // from reading m_destAddr for its dedupe set mid-swap.  Nothing in this
    // function may take m_systemsLock while it is held; it is released before
    // the UpdateUnicastDestinations() call at the end, which snapshots the
    // remote list under m_systemsLock and then takes this lock itself.
    std::unique_lock<std::mutex> updateLock(m_unicastUpdateLock);

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
        // The web UI PUTs this setting as a JSON string, so an empty list is
        // stored as a literal pair of quote characters rather than as an empty
        // value.  Left in, that becomes a "hostname" of "" and every reload
        // spends a full DNS timeout (~4s, on the settings-reload thread)
        // failing to resolve it.
        while (token.size() >= 2 && token.front() == '"' && token.back() == '"') {
            token = token.substr(1, token.size() - 2);
            TrimWhiteSpace(token);
        }
        if (token != "") {
            remotes.insert(token);
        }
    }

    // Assign rather than |=: this runs again on every settings change, so a
    // method the user just turned off has to go back to false.
    bool sendBroadcast = getSettingInt("MultiSyncBroadcast") != 0;
    bool sendMulticast = getSettingInt("MultiSyncMulticast") != 0;
    bool sendUnicast = getSettingInt("MultiSyncUnicast") != 0;
    if (remotesString == "" && !sendBroadcast && !sendMulticast && !sendUnicast && m_multiSyncEnabled) {
        // No explicit remotes or send method configured; default to multicast.
        sendMulticast = true;
    }

    // Resolve into a local list first: getaddrinfo() below is an unbounded
    // network lookup and must not run while m_socketLock is held, since that
    // lock is taken by the send path on the output thread every frame.
    std::vector<struct sockaddr_in> newAddrs;
    for (auto& s : remotes) {
        LogDebug(VB_SYNC, "Setting up Remote Sync for %s\n", s.c_str());
        struct sockaddr_in newRemote;
        memset(&newRemote, 0, sizeof(newRemote));

        newRemote.sin_family = AF_INET;
        newRemote.sin_port = htons(FPP_CTRL_PORT);

        // A letter (or a space) means this is a hostname and has to be resolved;
        // anything else is a dotted-quad to parse directly.  The test used to
        // read `... == s.end()`, i.e. "contains no letters", which is backwards:
        // hostnames took the inet_addr() path, came back INADDR_NONE, and were
        // then installed as a destination of 255.255.255.255 -- so every entry
        // that wasn't already an IP address quietly broadcast its sync packets.
        bool isHostname = std::find_if(s.begin(), s.end(), [](char c) { return (isalpha(c) || (c == ' ')); }) != s.end();
        bool valid = true;
        if (isHostname) {
            // Use the reentrant getaddrinfo() rather than gethostbyname(), which
            // shares a single static hostent across the process.
            struct addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            struct addrinfo* res = nullptr;
            if (getaddrinfo(s.c_str(), nullptr, &hints, &res) != 0 || res == nullptr) {
                LogErr(VB_SYNC,
                       "Error looking up Remote hostname: %s\n",
                       s.c_str());
                valid = false;
            } else {
                newRemote.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
                freeaddrinfo(res);
            }
        } else {
            newRemote.sin_addr.s_addr = inet_addr(s.c_str());
            if (newRemote.sin_addr.s_addr == INADDR_NONE) {
                LogErr(VB_SYNC, "Error parsing Remote IP address: %s\n", s.c_str());
                valid = false;
            }
        }
        if (valid) {
            newAddrs.push_back(newRemote);
        }
    }

    m_sendBroadcast = sendBroadcast;
    m_sendMulticast = sendMulticast;
    m_sendUnicast = sendUnicast;

    {
        // Swap the whole list in at once.  Each mmsghdr points at its own
        // element of m_destAddr, so the two vectors must be rebuilt together
        // and never observed half-updated by SendControlPacketViaMsgs().
        std::unique_lock<std::mutex> lock(m_socketLock);
        m_destAddr = std::move(newAddrs);
        m_destMsgs.clear();
        m_destMsgs.reserve(m_destAddr.size());
        for (size_t x = 0; x < m_destAddr.size(); x++) {
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
                 (int)m_destAddr.size());

        if (!sendUnicast) {
            // "Send to ALL KNOWN remotes" is off, so drop that list rather than
            // leaving a stale copy behind for the next time it is turned on.
            m_unicastDestMsgs.clear();
            m_unicastDestAddr.clear();
        }
    }
    updateLock.unlock();

    // Rebuild the "all known remotes" list too: it dedupes itself against
    // m_destAddr, which just changed.  Must run with both locks released --
    // UpdateUnicastDestinations() snapshots the remote list under m_systemsLock
    // and then takes m_unicastUpdateLock and m_socketLock itself.
    if (sendUnicast) {
        UpdateUnicastDestinations();
    }
}

void MultiSync::SendControlPacketViaMsgs(std::vector<struct mmsghdr>& msgs, struct iovec& iovec, void* outBuf, int len) {
    // The msgs vector (and the sockaddrs its entries point at) may be rebuilt
    // concurrently by UpdateUnicastDestinations(), so read its size, point the
    // shared iovec at our buffer, and send all while holding m_socketLock.
    std::unique_lock<std::mutex> lock(m_socketLock);
    int msgCount = msgs.size();
    if (msgCount == 0) {
        return;
    }
    iovec.iov_base = outBuf;
    iovec.iov_len = len;

    // sendmmsg stops at the first message it cannot send and reports how many
    // it did send, so each pass simply resumes at the destination that stopped
    // it.
    //
    // This runs on the channel output thread ahead of the frame's channel data,
    // so the whole call is bounded well inside a frame.  The bound matters more
    // than it looks: a remote that has dropped off the LAN leaves its neighbour
    // entry unresolved, and the kernel then answers sends to it with EAGAIN
    // (not EHOSTUNREACH) for as long as it is gone - measured here at ~97% of
    // sends.  Retrying that for long turns one dead remote into a per-frame
    // stall for every controller, which is exactly what the budget prevents.
    // A skipped sync packet is cheap by comparison: the next frame sends another.
    constexpr long long SEND_BUDGET_MS = 2;
    int outputCount = 0;
    int undelivered = 0;
    long long startTime = GetTimeMS();
    while (outputCount < msgCount) {
        int sent = sendmmsg(m_controlSock, &msgs[outputCount], msgCount - outputCount, MSG_DONTWAIT);
        if (sent > 0) {
            outputCount += sent;
            continue;
        }
        bool transient = (sent == 0) || errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS;
        if (transient && (GetTimeMS() - startTime) < SEND_BUDGET_MS) {
            // Genuine buffer pressure clears in well under the budget.
            std::this_thread::sleep_for(std::chrono::microseconds(250));
            continue;
        }
        // Either a hard error for this destination, or it is still refusing the
        // packet after the budget.  Step past it rather than retrying or
        // bailing out entirely, so the remaining healthy remotes still get this
        // frame's packet - one unreachable remote must not desync the rest.
        ++outputCount;
        ++undelivered;
    }
    if (undelivered) {
        LogErr(VB_SYNC, "Error: Unable to send multisync packet: %s   (%d/%d sent, %d undelivered)\n",
               FPPstrerror(errno), msgCount - undelivered, msgCount, undelivered);
    }
}

void MultiSync::SendControlPacket(void* outBuf, int len) {
    if (WillLog(LOG_EXCESSIVE, VB_SYNC)) {
        LogExcess(VB_SYNC, "SendControlPacket()\n");
        HexDump("Sending Control packet with contents:", outBuf, len, VB_SYNC);
    }

    // Statically-configured unicast remotes (MultiSyncRemotes/ExtraRemotes).
    SendControlPacketViaMsgs(m_destMsgs, m_destIovec, outBuf, len);

    // All known FPP remotes in Remote mode (MultiSyncUnicast setting).
    if (m_sendUnicast) {
        SendControlPacketViaMsgs(m_unicastDestMsgs, m_unicastDestIovec, outBuf, len);
    }
    if (m_sendMulticast) {
        SendMulticastPacket(outBuf, len);
    }
    if (m_sendBroadcast) {
        SendBroadcastPacket(outBuf, len);
    }
}

void MultiSync::UpdateUnicastDestinations() {
    // Snapshot the addresses to resolve while holding m_systemsLock only
    // briefly, then release it before the rebuild.  The rebuild takes
    // m_unicastUpdateLock, which must never be acquired while m_systemsLock is
    // held (see the overload below), so the snapshot and the rebuild cannot
    // share a lock scope.
    std::vector<std::string> addrsToResolve;
    {
        std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
        for (auto& sys : m_remoteSystems) {
            if (sys.supportsUnicast) {
                addrsToResolve.push_back(sys.address);
            }
        }
    }
    UpdateUnicastDestinations(addrsToResolve);
}

void MultiSync::UpdateUnicastDestinations(const std::vector<std::string>& addrsToResolve) {
    // Serialize rebuilds against each other -- DNS resolution below happens
    // without any other lock held, so without this, two overlapping calls could
    // interleave and the slower resolution could finish last and clobber a
    // newer result.  Callers must not hold m_systemsLock here: ReloadSyncDestinations()
    // holds this lock and its tail reaches m_systemsLock through the no-arg
    // overload, so taking the two in the other order would deadlock.
    std::unique_lock<std::mutex> updateLock(m_unicastUpdateLock);

    // getaddrinfo() (via GetIPForHost()) is an unbounded, potentially slow
    // DNS/mDNS lookup, which is why the caller resolves from a snapshot rather
    // than iterating m_remoteSystems here: resolving under m_systemsLock used to
    // stall every other m_systemsLock caller (notably the frequently-polled
    // GetSystems()) for as long as the lookup took.  GetIPForHost() itself is
    // reentrant/thread-safe, so it's fine to call without any MultiSync lock held.
    std::vector<struct sockaddr_in> newAddrs;
    for (auto& address : addrsToResolve) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(FPP_CTRL_PORT);

        std::string ipAd = address;
        if (GetIPForHost(ipAd)) {
            addr.sin_addr.s_addr = inet_addr(ipAd.c_str());
        } else {
            addr.sin_addr.s_addr = inet_addr(address.c_str());
        }
        // Skip anything that didn't resolve to a usable IPv4 address
        // (e.g. an IPv6-only peer); sockaddr_in can't represent it.
        if (addr.sin_addr.s_addr == INADDR_NONE || addr.sin_addr.s_addr == 0) {
            continue;
        }
        newAddrs.push_back(addr);
    }

    // Build the new sockaddr list, then swap it into place under m_socketLock.
    // m_socketLock is the innermost lock here and is never held while acquiring
    // m_systemsLock, matching the m_systemsLock -> m_socketLock order used by Ping().
    std::unique_lock<std::mutex> lock(m_socketLock);
    // Skip any address already covered by the statically-configured remote list
    // (m_destAddr, rebuilt by ReloadSyncDestinations(), which holds the same
    // m_unicastUpdateLock we hold here while it swaps) so
    // a remote that is both individually selected for unicast AND picked up by
    // "all known remotes" only receives each packet once.  The same set also
    // dedupes the all-known list against itself (e.g. a remote known under both
    // a hostname and an IP).
    std::set<uint32_t> seen;
    for (auto& s : m_destAddr) {
        seen.insert(s.sin_addr.s_addr);
    }
    m_unicastDestAddr.clear();
    m_unicastDestAddr.reserve(newAddrs.size());
    for (auto& addr : newAddrs) {
        if (seen.insert(addr.sin_addr.s_addr).second) {
            m_unicastDestAddr.push_back(addr);
        }
    }
    m_unicastDestMsgs.clear();
    m_unicastDestMsgs.reserve(m_unicastDestAddr.size());
    for (size_t x = 0; x < m_unicastDestAddr.size(); x++) {
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));
        msg.msg_hdr.msg_name = &m_unicastDestAddr[x];
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        msg.msg_hdr.msg_iov = &m_unicastDestIovec;
        msg.msg_hdr.msg_iovlen = 1;
        msg.msg_len = 0;
        m_unicastDestMsgs.push_back(msg);
    }
    LogDebug(VB_SYNC, "%d unicast MultiSync destinations (all known remotes)\n",
             (int)m_unicastDestMsgs.size());
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
            LogErr(VB_SYNC, "Error: Unable to send packet: %s\n", FPPstrerror(errno));
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
                    LogErr(VB_SYNC, "Error setting IP_MULTICAST_LOOP for %s: %s\n", a.second.interfaceName.c_str(), FPPstrerror(errno));
                }
#ifdef PLATFORM_OSX
                int idx = if_nametoindex(a.second.interfaceName.c_str());
                if (setsockopt(a.second.multicastSocket, IPPROTO_IP, IP_BOUND_IF, &idx, sizeof(idx)) < 0) {
                    LogErr(VB_SYNC, "Error setting IP_MULTICAST Device for %s: %s\n", a.second.interfaceName.c_str(), FPPstrerror(errno));
                }
#else
                if (setsockopt(a.second.multicastSocket, SOL_SOCKET, SO_BINDTODEVICE, a.second.interfaceName.c_str(), a.second.interfaceName.size()) < 0) {
                    LogErr(VB_SYNC, "Error setting IP_MULTICAST Device for %s: %s\n", a.second.interfaceName.c_str(), FPPstrerror(errno));
                }
#endif
            }
        }

        if (a.second.multicastSocket >= 0) {
            if (sendto(a.second.multicastSocket, outBuf, len, 0, (struct sockaddr*)&bda, sizeof(struct sockaddr_in)) < 0)
                LogErr(VB_SYNC, "Error: Unable to send packet: %s\n", FPPstrerror(errno));
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
            // Check if interface is UP and RUNNING before adding it
            if (isSupportedForMultisync("", tmp->ifa_name) && 
                (tmp->ifa_flags & IFF_UP) && 
                (tmp->ifa_flags & IFF_RUNNING)) {
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
                char abuf[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &sa->sin_addr, abuf, sizeof(abuf));
                info.interfaceAddress = abuf;
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
        LogErr(VB_SYNC, "Error opening Receive socket; %s\n", FPPstrerror(errno));
        WarningHolder::AddWarning(42, "MultiSync: could not open receive socket");
        return 0;
    }
    LogDebug(VB_SYNC, "Receive socket: %d\n", m_receiveSock);

    bzero((char*)&m_receiveSrcAddr, sizeof(m_receiveSrcAddr));
    m_receiveSrcAddr.sin_family = AF_INET;
    m_receiveSrcAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_receiveSrcAddr.sin_port = htons(FPP_CTRL_PORT);

    int optval = 1;
    if (setsockopt(m_receiveSock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) < 0) {
        LogErr(VB_SYNC, "Error turning on SO_REUSEPORT; %s\n", FPPstrerror(errno));
        return 0;
    }

    // Bind the socket to address/port
    if (bind(m_receiveSock, (struct sockaddr*)&m_receiveSrcAddr, sizeof(m_receiveSrcAddr)) < 0) {
        LogErr(VB_SYNC, "Error binding socket; %s\n", FPPstrerror(errno));
        WarningHolder::AddWarning(42, "MultiSync: could not bind receive socket (port in use?)");
        return 0;
    }

    if (setsockopt(m_receiveSock, IPPROTO_IP, IP_PKTINFO, &optval, sizeof(optval)) < 0) {
        LogErr(VB_SYNC, "Error calling setsockopt; %s\n", FPPstrerror(errno));
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

    setupMulticastReceive(false);
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

void MultiSync::setupMulticastReceive(bool cycle) {
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
        if (cycle) {
            setsockopt(m_receiveSock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
        }
        if ((rc = setsockopt(m_receiveSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) < 0) {
            if (m_broadcastSock < 0) {
                // first time through, log as warning, otherwise error is likely due to already being subscribed
                LogWarn(VB_SYNC, "   Could not setup Multicast Group for interface %s    rc: %d\n", a.second.interfaceName.c_str(), rc);
            } else {
                LogDebug(VB_SYNC, "   Could not setup Multicast Group for interface %s    rc: %d\n", a.second.interfaceName.c_str(), rc);
            }
        }
    }
}

// True if a later message in this recvmmsg batch supersedes message i (same
// sync file type and sync packet type), so only the newest need be acted on.
// Entries are one per received message, with a null buffer for messages that
// were not received or are too short to hold a ControlPkt, so the indexes line
// up with the msg indexes the caller uses.  Only packets long enough to hold
// the SyncPkt that follows the header can be compared.
static bool shouldSkipPacket(int i, int num, const std::vector<std::pair<unsigned char*, int>>& rcvBuffers) {
    constexpr int MIN_SYNC_LEN = (int)(sizeof(ControlPkt) + sizeof(SyncPkt));
    if (rcvBuffers[i].first == nullptr || rcvBuffers[i].second < MIN_SYNC_LEN) {
        return false;
    }
    ControlPkt* pkt = (ControlPkt*)(rcvBuffers[i].first);
    if (pkt->pktType != CTRL_PKT_SYNC) {
        return false;
    }
    SyncPkt* spkt = (SyncPkt*)(((char*)pkt) + sizeof(ControlPkt));
    for (int x = i + 1; x < num; x++) {
        if (rcvBuffers[x].first == nullptr || rcvBuffers[x].second < MIN_SYNC_LEN) {
            continue;
        }
        ControlPkt* npkt = (ControlPkt*)(rcvBuffers[x].first);
        if (npkt->pktType != CTRL_PKT_SYNC) {
            continue;
        }
        SyncPkt* snpkt = (SyncPkt*)(((char*)npkt) + sizeof(ControlPkt));
        if (spkt->fileType == snpkt->fileType && spkt->pktType == snpkt->pktType) {
            return true;
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
        // One entry per received message, buffer and length, so that the
        // indexes shouldSkipPacket() walks are the same msg indexes used below.
        // Messages that failed or are too short for a ControlPkt get a null
        // placeholder rather than being left out of the vector.
        std::vector<std::pair<unsigned char*, int>> v;
        v.reserve(msgcnt);
        for (int msg = 0; msg < msgcnt; msg++) {
            int len = rcvMsgs[msg].msg_len;
            if (len <= 0) {
                LogErr(VB_SYNC, "Error: recvmsg failed: %s\n", FPPstrerror(errno));
            }
            if (len < (int)sizeof(ControlPkt)) {
                v.emplace_back(nullptr, 0);
                continue;
            }
            unsigned char* inBuf = rcvBuffers[msg];
            inBuf[len] = 0;
            v.emplace_back(inBuf, len);
        }
        LogExcess(VB_SYNC, "ProcessControlPacket msgcnt: %d\n", msgcnt);
        for (int msg = 0; msg < msgcnt; msg++) {
            int len = rcvMsgs[msg].msg_len;
            if (len <= 0) {
                LogErr(VB_SYNC, "Error: recvmsg failed: %s\n", FPPstrerror(errno));
                continue;
            }
            unsigned char* inBuf = rcvBuffers[msg];

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

            // Both system lists have to be read under m_systemsLock: UpdateSystem()
            // runs on the HTTP/mDNS probe threads and can push_back onto
            // m_remoteSystems, reallocating it out from under this loop.  The
            // stats map work below then runs under m_statsLock alone -- the two
            // locks are never held at the same time.
            bool isLocal = false;
            {
                std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
                for (auto& sys : m_localSystems) {
                    if (sys.address == sourceIP) {
                        isLocal = true;
                    }
                }
                if (!isLocal) {
                    for (auto& sys : m_remoteSystems) {
                        if (sys.address == sourceIP)
                            hostname = sys.hostname;
                    }
                }
            }

            MultiSyncStats tempStats("", "");
            MultiSyncStats* stats = &tempStats;
            if (!isLocal) {
                std::unique_lock<std::recursive_mutex> slock(m_statsLock);
                auto a = m_syncStats.find(sourceIP);
                if (a != m_syncStats.end()) {
                    stats = (MultiSyncStats*)a->second;
                } else {
                    stats = new MultiSyncStats(sourceIP, hostname);
                    m_syncStats[sourceIP] = stats;
                }
                stats->lastReceiveTime = time(NULL);
            }

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
                    if (getFPPmode() == REMOTE_MODE) {
                        ProcessSyncPacket(pkt, len, stats);
                    }
                    break;
                case CTRL_PKT_BLANK:
                    if (getFPPmode() == REMOTE_MODE) {
                        for (auto a : getPluginsCopy()) {
                            a->ReceivedBlankingDataPacket();
                        }
                        stats->pktBlank++;
                        sequence->SendBlankingData();
                    }
                    break;
                case CTRL_PKT_PING: {
                    struct sockaddr_in* inAddr = (struct sockaddr_in*)(&rcvSrcAddr[msg]);
                    char ipbuf[INET_ADDRSTRLEN] = {0};
                    inet_ntop(AF_INET, &inAddr->sin_addr, ipbuf, sizeof(ipbuf));
                    std::string ip = ipbuf;
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

void MultiSync::OpenSyncedSequence(const std::string& filename) {
    LogDebug(VB_SYNC, "OpenSyncedSequence(%s)\n", filename.c_str());

    for (auto a : getPluginsCopy()) {
        a->ReceivedSeqOpenPacket(filename);
    }
    ResetMasterPosition();
    sequence->OpenSequenceFile(filename);
}

void MultiSync::StartSyncedSequence(const std::string& filename) {
    LogDebug(VB_SYNC, "StartSyncedSequence(%s)\n", filename.c_str());

    ResetMasterPosition();
    for (auto a : getPluginsCopy()) {
        a->ReceivedSeqSyncStartPacket(filename);
    }
    if (!sequence->IsSequenceRunning(filename) && !sequence->IsSequenceRunning("fallback.fseq")) {
        sequence->StartSequence(filename, 0);
    }
}

void MultiSync::StopSyncedSequence(const std::string& filename) {
    LogDebug(VB_SYNC, "StopSyncedSequence(%s)\n", filename.c_str());
    for (auto a : getPluginsCopy()) {
        a->ReceivedSeqSyncStopPacket(filename);
    }
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
                        SendSeqSyncStopPacket(sequence->GetSeqFilenameCopy());
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
void MultiSync::SyncSyncedSequence(const std::string& filename, int frameNumber, float secondsElapsed) {
    LogExcess(VB_SYNC, "SyncSyncedSequence('%s', %d, %.2f)\n",
              filename.c_str(), frameNumber, secondsElapsed);

    for (auto a : getPluginsCopy()) {
        a->ReceivedSeqSyncPacket(filename, frameNumber, secondsElapsed);
    }
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

void MultiSync::OpenSyncedMedia(const std::string& filename) {
    LogDebug(VB_SYNC, "OpenSyncedMedia(%s)\n", filename.c_str());

    if (mediaOutput) {
        LogDebug(VB_SYNC, "Start media %s received while playing media %s\n",
                 filename.c_str(), mediaOutput->m_mediaFilename.c_str());

        CloseMediaOutput();
    }
    for (auto a : getPluginsCopy()) {
        a->ReceivedMediaOpenPacket(filename);
    }

    m_lastSyncedMediaPacketMS = (uint64_t)GetTimeMS();
    OpenMediaOutput(filename);
}

void MultiSync::StartSyncedMedia(const std::string& filename, float secondsElapsed) {
    LogDebug(VB_SYNC, "StartSyncedMedia(%s, %.2f)\n", filename.c_str(), secondsElapsed);
    for (auto a : getPluginsCopy()) {
        a->ReceivedMediaSyncStartPacket(filename);
    }
    int msTime = (secondsElapsed > 0.0f) ? (int)(secondsElapsed * 1000.0f) : 0;
    m_lastSyncedMediaPacketMS = (uint64_t)GetTimeMS();
    StartMediaOutput(filename, msTime);
}

/*
 *
 */
void MultiSync::StopSyncedMedia(const std::string& filename) {
    LogDebug(VB_SYNC, "StopSyncedMedia(%s)\n", filename.c_str());
    for (auto a : getPluginsCopy()) {
        a->ReceivedMediaSyncStopPacket(filename);
    }

    m_lastSyncedMediaPacketMS = 0;

    if (!mediaOutput) {
        return;
    }

    if (MatchesRunningMediaFilename(filename)) {
        LogDebug(VB_SYNC, "Stopping synced media: %s\n", mediaOutput->m_mediaFilename.c_str());
        CloseMediaOutput();
    }
}

/*
 * Close a synced media output that has run to its end while the player has
 * gone away (issue #2727: a wedged-decoder restart on the player left both
 * remotes holding an EOS'd pipeline forever -- slot still active, /dev/video10
 * and dmabufs still held -- while the UI happily reported "playing").
 *
 * The trigger is deliberately NOT "sync packets went quiet".  A remote's media
 * free-runs on its own clock, so a network glitch mid-clip is harmless: the
 * clip keeps playing and UpdateMasterMediaPosition nudges it back when packets
 * resume.  Tearing that down on packet silence would turn a glitch into a
 * visible outage.  The unrecoverable state is a clip that has ENDED with the
 * player gone -- nothing but a sync packet can ever move it forward again.
 *
 * The normal end-of-clip-to-next-clip gap on a remote is ~13ms, so the default
 * grace of 10s is ~750x the real-world gap; a glitch straddling a clip boundary
 * costs at most the last frame going away early, and the next START packet
 * opens a fresh output as usual.
 */
void MultiSync::CheckSyncedMediaIdleTimeout() {
    uint64_t last = m_lastSyncedMediaPacketMS;
    if (last == 0) {
        return; // no synced media open
    }
    // 0 disables the watchdog
    int timeoutSec = getSettingInt("RemoteSyncedMediaIdleTimeout", 10);
    if (timeoutSec <= 0) {
        return;
    }
    if (((uint64_t)GetTimeMS() - last) < (uint64_t)(timeoutSec * 1000)) {
        return;
    }

    std::string filename;
    {
        std::unique_lock<std::mutex> lock(mediaOutputLock);
        if (!mediaOutput) {
            m_lastSyncedMediaPacketMS = 0;
            return;
        }
        if (mediaOutput->IsPlaying()) {
            return; // still playing -- free-running through a sync gap is fine
        }
        filename = mediaOutput->m_mediaFilename;
    }

    LogWarn(VB_SYNC, "Synced media '%s' finished and no sync packet received in %d seconds "
                     "- player appears to have gone away, closing media output\n",
            filename.c_str(), timeoutSec);
    m_lastSyncedMediaPacketMS = 0;
    CloseMediaOutput();
}

/*
 *
 */
void MultiSync::SyncSyncedMedia(const std::string& filename, int frameNumber, float secondsElapsed) {
    LogExcess(VB_SYNC, "SyncSyncedMedia('%s', %d, %.2f)\n",
              filename.c_str(), frameNumber, secondsElapsed);
    for (auto a : getPluginsCopy()) {
        a->ReceivedMediaSyncPacket(filename, secondsElapsed);
    }

    m_lastSyncedMediaPacketMS = (uint64_t)GetTimeMS();
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

    {
        // Read by GetSyncStats() on an API thread; m_statsLock is the lock the
        // two sides share.  Do not reach for m_systemsLock while holding it.
        std::unique_lock<std::recursive_mutex> slock(m_statsLock);
        m_syncMaster = stats->sourceIP;
    }

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
            secondsElapsed = spkt->secondsElapsed - m_remoteOffset;
            if (secondsElapsed < 0)
                secondsElapsed = 0.0;

            StartSyncedMedia(spkt->filename, secondsElapsed);
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

    // CommandPkt is a flexible char[1] with no length field of its own, so the
    // command text is only as terminated as the sender chose to make it. Bound
    // the read by the packet's own extraDataLen before handing it to the log
    // (which is remote-supplied and unbounded, hence TruncateForLog too).
    LogDebug(VB_COMMAND, "Legacy command \"%s\" received from remote host %s (%s)\n",
            TruncateForLog(std::string(cpkt->command, strnlen(cpkt->command, pkt->extraDataLen))).c_str(),
            stats->hostname.c_str(), stats->sourceIP.c_str());

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
    // Bounded copy of a packet string field. The packet tail is not guaranteed
    // to be NUL-terminated and may be longer than tmpStr, so clamp to both the
    // field's own size and the destination buffer to avoid a stack overflow /
    // over-read.
    auto copyField = [&](int fieldOffset) -> std::string {
        int avail = (int)pkt->extraDataLen - fieldOffset;
        if (avail < 0) {
            avail = 0;
        }
        int toCopy = std::min(avail, (int)(sizeof(tmpStr) - 1));
        memset(tmpStr, 0, sizeof(tmpStr));
        memcpy(tmpStr, (char*)(extraData + fieldOffset), toCopy);
        return std::string(tmpStr);
    };
    std::string hostname = copyField(12);
    std::string version = copyField(77);
    std::string typeStr = copyField(118);
    // End of v1 packet fields

    std::string ranges;
    if ((pkt->extraDataLen) > 169) {
        ranges = copyField(166 - 7);
    }

    bool isLocal = false;
    {
        // Only m_interfaces needs m_socketLock; released before UpdateSystem()
        // below so it isn't held across a call that can re-enter m_socketLock
        // via UpdateUnicastDestinations() (m_socketLock is non-recursive).
        std::unique_lock<std::mutex> lock(m_socketLock);
        for (auto& a : m_interfaces) {
            if (address == a.second.interfaceAddress) {
                isLocal = true;
            }
        }
    }

    if (isInstance) {
        std::string localUUID(isLocal ? getSetting("SystemUUID") : "Unknown");
        multiSync->UpdateSystem(type, majorVersion, minorVersion,
                                systemMode, address, hostname, version,
                                typeStr, ranges, localUUID.c_str(), true,
                                systemMode & 0x04 ? true : false);

        // A discover ping is what an instance sends as its fppd starts (fppd.cpp
        // calls Discover() during startup), and a restart is exactly when most
        // of what we cache about a peer changes -- channel inputs/outputs, host
        // description, background color, anything else that needed a restart to
        // take effect.  Clearing the fetch timestamp makes the next sweep re-ask
        // within INFO_SCAN_INTERVAL instead of serving up to
        // INFO_REFRESH_INTERVAL of stale detail.  The cached values themselves
        // are left in place so the UI doesn't blank out mid-refetch.
        //
        // This cannot amplify into a fetch storm: a discover ping is only ever
        // answered with plain (non-discover) pings, so one restart invalidates
        // one system on each peer.  xLights' discovery ping is also a discover
        // ping, but it carries no address and so never reaches this branch.
        if (discover && !isLocal) {
            multiSync->InvalidateSystemInfo(address);
        }
    }
    if (discover) {
        if ((hostname != m_hostname) && !isLocal) {
            // very slight random delay so all the remotes don't send
            // packets out at the same time
            int rand = FPPrand() % 5000;
            std::this_thread::sleep_for(std::chrono::microseconds(rand));
            multiSync->Ping();
            rand = FPPrand() % 1000;
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
    // The plugin name must be NUL-terminated within the extra data.  An
    // unterminated name would otherwise run strlen() out to the terminator the
    // receive loop writes one byte past the datagram, making nlen larger than
    // the data actually present and leaving a negative len for the plugins --
    // which they take as a size_t and read far past the buffer.
    int nlen = (int)strnlen(pn, (size_t)len);
    if (nlen >= len) {
        LogErr(VB_SYNC, "Error: plugin packet name is not terminated within %d bytes of data\n", len);
        stats->pktError++;
        return;
    }
    nlen++; // include the terminator
    len -= nlen;
    uint8_t* data = (uint8_t*)&cpkt->command[nlen];

    for (auto a : getPluginsCopy()) {
        a->ReceivedPluginData(pn, data, len);
    }
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
    // The caller has already checked len == sizeof(ControlPkt) + extraDataLen,
    // so the packet's own data ends at `end`.  Every field below has to be
    // parsed strictly against that bound: a truncated or crafted packet whose
    // strings are not terminated inside it would otherwise be read out of the
    // stale contents of this reusable receive buffer -- and, for the last row of
    // that array, past the end of it -- and the result handed to CommandManager.
    const char* b = (const char*)pkt;
    const char* end = b + sizeof(ControlPkt) + pkt->extraDataLen;
    const char* pos = b + sizeof(ControlPkt);

    auto readString = [&pos, end](std::string& out) {
        if (pos >= end) {
            return false;
        }
        size_t avail = (size_t)(end - pos);
        size_t slen = strnlen(pos, avail);
        if (slen == avail) {
            // ran to the end of the packet without finding a terminator
            return false;
        }
        out.assign(pos, slen);
        pos += slen + 1;
        return true;
    };

    if (pos >= end) {
        LogErr(VB_SYNC, "Error: truncated FPP Command packet\n");
        stats->pktError++;
        return;
    }
    // Unsigned: as a signed char an argument count above 127 reads as negative
    // and silently skips the whole argument list.
    uint8_t numArgs = (uint8_t)*pos++;

    std::string host;
    std::string cmd;
    if (!readString(host) || !readString(cmd)) {
        LogErr(VB_SYNC, "Error: truncated FPP Command packet\n");
        stats->pktError++;
        return;
    }

    std::vector<std::string> args;
    for (uint8_t x = 0; x < numArgs; x++) {
        std::string arg;
        if (!readString(arg)) {
            LogErr(VB_SYNC, "Error: FPP Command packet claims %d arguments but only holds %d\n",
                   (int)numArgs, (int)args.size());
            stats->pktError++;
            return;
        }
        args.push_back(std::move(arg));
    }
    bool matches;
    if (host == "") {
        matches = true;
    } else {
        // Copy m_localSystems under m_systemsLock and release it before
        // evaluating/running -- MyHostMatches only needs a snapshot, and this
        // function must not hold m_systemsLock across CommandManager::run() or
        // the plugin callbacks below.
        std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
        std::vector<MultiSyncSystem> localSystems = m_localSystems;
        lock.unlock();
        matches = MyHostMatches(host, m_hostname, localSystems);
    }
    if (matches) {
        stats->pktFPPCommand++;
        // Remote-supplied and unbounded - truncate. (The args are covered by
        // CommandManager::run()'s own log line below.)
        LogDebug(VB_COMMAND, "Command \"%s\" received from remote host %s (%s)\n",
                TruncateForLog(cmd).c_str(), stats->hostname.c_str(), stats->sourceIP.c_str());
        for (auto a : getPluginsCopy()) {
            a->ReceivedFPPCommandPacket(cmd, args);
        }
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
    for (auto a : getPluginsCopy()) {
        a->SendFPPCommandPacket(host, cmd, args);
    }
    char outBuf[2048];
    bzero(outBuf, sizeof(outBuf));

    ControlPkt* cpkt = (ControlPkt*)outBuf;
    InitControlPacket(cpkt);
    cpkt->pktType = CTRL_PKT_FPPCOMMAND;

    int pos = sizeof(ControlPkt);
    int room = (int)sizeof(outBuf) - pos;
    // Bounded writes so a long host/cmd/arg can never overflow outBuf; strings
    // that don't fit are truncated rather than written past the buffer.
    auto appendField = [&](const std::string& v) {
        int n = std::min((int)v.length() + 1, room > 0 ? room : 0);
        if (n > 0) {
            memcpy(&outBuf[pos], v.c_str(), n);
            outBuf[pos + n - 1] = '\0';
        }
        pos += n;
        room -= n;
    };
    outBuf[pos++] = (unsigned char)args.size();
    room--;
    appendField(host);
    appendField(cmd);
    for (auto& a : args) {
        appendField(a);
    }
    cpkt->extraDataLen = pos - sizeof(ControlPkt);

    // SendFPPCommandPacket can run on API/command threads, so m_localSystems
    // must not be read without m_systemsLock; take a snapshot for both
    // MyHostMatches() calls below rather than locking per-call.
    std::vector<MultiSyncSystem> localSystems;
    {
        std::unique_lock<std::recursive_mutex> lock(m_systemsLock);
        localSystems = m_localSystems;
    }

    if (host != "" && host.find(",") == std::string::npos) {
        if (MyHostMatches(host, m_hostname, localSystems)) {
            // only targetting myself, just run and don't send the packet
            LogDebug(VB_COMMAND, "Command \"%s\" self-targeted, running locally without sending\n", cmd.c_str());
            CommandManager::INSTANCE.run(cmd, args);
        } else {
            SendUnicastPacket(host, outBuf, pos);
        }
    } else {
        SendControlPacket(outBuf, pos);
        // the packet won't loop back so if it's supposed to run on this host as well,
        // we need to force it
        if (host == "" || MyHostMatches(host, m_hostname, localSystems)) {
            LogDebug(VB_COMMAND, "Command \"%s\" broadcast includes this host, running locally too\n", cmd.c_str());
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
