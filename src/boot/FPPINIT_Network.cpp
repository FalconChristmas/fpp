/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2024 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <arpa/inet.h>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <ifaddrs.h>
#include <list>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <systemd/sd-daemon.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "FPPINIT.h"

std::string FindTetherWIFIAdapater() {
    std::string ret;
    if (!getRawSetting("TetherInterface", ret) || ret.empty()) {
        // see if we can find a non-associated interface
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net/")) {
            std::string dev = entry.path().filename();
            if (startsWith(dev, "wl")) {
                std::string output = execAndReturn("/usr/sbin/iw dev " + dev + " link");
                if (contains(output, "Not connected")) {
                    return dev;
                }
            }
        }
    }
    if (ret.empty()) {
        ret = "wlan0";
    }
    return ret;
}

static int getIntFromMap(const std::map<std::string, std::string>& m, const std::string& setting, int def) {
    if (m.find(setting) != m.end()) {
        std::string v = m.at(setting);
        if (v.empty()) {
            return def;
        }
        return std::stoi(v);
    }
    return def;
}
static std::string netmaskToSubnet(const std::string& nm) {
    const std::map<std::string, std::string> NETMASK2SUBNET = {
        { "255.255.255.252", "30" },
        { "255.255.255.248", "29" },
        { "255.255.255.240", "28" },
        { "255.255.255.224", "27" },
        { "255.255.255.192", "26" },
        { "255.255.255.128", "25" },
        { "255.255.255.0", "24" },
        { "255.255.254.0", "23" },
        { "255.255.252.0", "22" },
        { "255.255.248.0", "21" },
        { "255.255.240.0", "20" },
        { "255.255.224.0", "19" },
        { "255.255.192.0", "18" },
        { "255.255.128.0", "17" },
        { "255.255.0.0", "16" }
    };
    if (NETMASK2SUBNET.contains(nm)) {
        return NETMASK2SUBNET.at(nm);
    }
    return "24";
}

static bool compareAndCopyFiles(std::map<std::string, std::string> filesNeeded) {
    bool changed = false;
    for (auto& nf : filesNeeded) {
        if (FileExists(nf.first)) {
            std::string c = GetFileContents(nf.first);
            if (c != nf.second) {
                PutFileContents(nf.first, nf.second);
                changed = true;
            }
        } else {
            PutFileContents(nf.first, nf.second);
            changed = true;
        }
    }
    return changed;
}
static std::string CreateHostAPDConfig(const std::string& interface) {
    std::string ssid = "FPP";
    std::string psk = "Christmas";
    std::string WifiRegulatoryDomain = "US";
    getRawSetting("WifiRegulatoryDomain", WifiRegulatoryDomain);
    getRawSetting("TetherSSID", ssid);
    getRawSetting("TetherPSK", psk);

    std::string content;
    content.append("interface=").append(interface).append("\ndriver=nl80211\nwpa_passphrase=").append(psk).append("\nssid=");
    content.append(ssid).append("\ncountry_code=").append(WifiRegulatoryDomain);
    content.append("\nhw_mode=g\nchannel=11\nmacaddr_acl=0\nauth_algs=3\nwpa=2\n");
    content.append("wpa_key_mgmt=WPA-PSK\nwpa_pairwise=TKIP\nrsn_pairwise=CCMP\nwmm_enabled=0\nignore_broadcast_ssid=0\nwpa_group_rekey=3600\nwpa_gmk_rekey=86400\n");
    content.append("reassociation_deadline=3000\npmk_r1_push=1\nft_over_ds=0\nbss_transition=1\n");
    if (FileExists("/usr/bin/qrencode")) {
        // Low error correction MAY result in a QR that can be pixel doubled which generally works better for phones to pick up
        std::string cmd = "/usr/bin/qrencode -t ASCII -m 0 -s 6 -l L -o \"";
        cmd.append(FPP_MEDIA_DIR).append("/tmp/wifi-L.ascii\" \"WIFI:T:WPA;S:");
        cmd.append(ssid).append(";P:").append(psk).append(";;\"");
        exec(cmd);
        // Generate a High error correction as well.  If we cannot pixel double, this will give more info to the phone/device

        cmd = "/usr/bin/qrencode -t ASCII -m 3 -s 6 -l H -o \"";
        cmd.append(FPP_MEDIA_DIR).append("/tmp/wifi-H.ascii\" \"WIFI:T:WPA;S:");
        cmd.append(ssid).append(";P:").append(psk).append(";;\"");
        exec(cmd);
    }

    return content;
}
static void unblockWifi() {
    if (FileExists("/usr/sbin/rfkill")) {
        exec("/usr/sbin/rfkill unblock wifi");
        exec("/usr/sbin/rfkill unblock all");
    }
}

void setupNetwork(bool fullReload) {
    unblockWifi();
    auto dnsSettings = loadSettingsFile(FPP_MEDIA_DIR + "/config/dns");
    
    // Load global gateway configuration with migration support
    auto gatewaySettings = loadSettingsFile(FPP_MEDIA_DIR + "/config/gateway");
    std::string globalGateway = gatewaySettings["GATEWAY"];
    
    // Migration: If no global gateway exists, check for old per-interface gateways
    if (globalGateway.empty()) {
        for (const auto& entry : std::filesystem::directory_iterator(FPP_MEDIA_DIR + "/config")) {
            std::string filename = entry.path().filename();
            if (startsWith(filename, "interface.")) {
                auto interfaceSettings = loadSettingsFile(entry.path());
                std::string oldGateway = interfaceSettings["GATEWAY"];
                if (!oldGateway.empty() && oldGateway != "\"\"") {
                    // Found an old gateway setting - migrate it to global
                    printf("FPP - Migrating gateway %s from %s to global configuration\n", 
                           oldGateway.c_str(), filename.c_str());
                    
                    // Save to global gateway file
                    std::string gatewayContent = "GATEWAY=\"" + oldGateway + "\"\n";
                    PutFileContents(FPP_MEDIA_DIR + "/config/gateway", gatewayContent);
                    globalGateway = oldGateway;
                    
                    // Remove old GATEWAY setting from interface file (optional cleanup)
                    std::string interfaceContent = GetFileContents(entry.path());
                    size_t pos = interfaceContent.find("GATEWAY=");
                    if (pos != std::string::npos) {
                        size_t endPos = interfaceContent.find('\n', pos);
                        if (endPos != std::string::npos) {
                            interfaceContent.erase(pos, endPos - pos + 1);
                            PutFileContents(entry.path(), interfaceContent);
                        }
                    }
                    break; // Use first gateway found
                }
            }
        }
    }
    
    std::string dns = dnsSettings["DNS1"];
    if (dnsSettings["DNS2"] != "") {
        dns += ",";
        dns += dnsSettings["DNS2"];
    }
    std::string WifiRegulatoryDomain = "US";
    getRawSetting("WifiRegulatoryDomain", WifiRegulatoryDomain);
    exec("/usr/sbin/iw reg set " + WifiRegulatoryDomain);

    int tetherEnabled = getRawSettingInt("EnableTethering", 0);
    std::string tetherInterface = FindTetherWIFIAdapater();

    const std::string dhcpProxyFile = FPP_MEDIA_DIR + "/config/dhcp-proxy-config.conf";
    std::string dhcpProxies;

    bool ipForward = false;
    bool hostapd = false;
    std::map<std::string, std::string> filesNeeded;
    std::list<std::string> filesToConsider;
    std::list<std::string> commandsToRun;
    std::list<std::string> postCommandsToRun;
    for (const auto& entry : std::filesystem::directory_iterator("/etc/systemd/network")) {
        std::string dev = entry.path().filename();
        if (startsWith(dev, "10-") && endsWith(dev, ".network")) {
            filesToConsider.emplace_back("/etc/systemd/network/" + dev);
        }
    }
    for (const auto& entry : std::filesystem::directory_iterator("/etc/wpa_supplicant")) {
        std::string dev = entry.path().filename();
        if (startsWith(dev, "wpa_supplicant-wl") && endsWith(dev, ".conf")) {
            filesToConsider.emplace_back("/etc/wpa_supplicant/" + dev);
        }
    }
    if (FileExists("/etc/hostapd/hostapd.conf")) {
        filesToConsider.emplace_back("/etc/hostapd/hostapd.conf");
    }
    if (std::filesystem::exists(FPP_MEDIA_DIR + "/config")) {
        for (const auto& entry : std::filesystem::directory_iterator(FPP_MEDIA_DIR + "/config")) {
            std::string dev = entry.path().filename();
            if (startsWith(dev, "interface.")) {
                bool validConfig = true;
                auto interfaceSettings = loadSettingsFile(FPP_MEDIA_DIR + "/config/" + dev);
                std::string interface = dev.substr(10);

                std::string content = "[Match]\nName=";
                std::string addressLines;

                int DHCPSERVER = getIntFromMap(interfaceSettings, "DHCPSERVER", 0);
                int DHCPOFFSET = getIntFromMap(interfaceSettings, "DHCPOFFSET", 100);
                int DHCPPOOLSIZE = getIntFromMap(interfaceSettings, "DHCPPOOLSIZE", 50);
                int ROUTEMETRIC = getIntFromMap(interfaceSettings, "ROUTEMETRIC", 0);
                int IPFORWARDING = getIntFromMap(interfaceSettings, "IPFORWARDING", 0);

                if (DHCPSERVER == 1) {
                    std::string address = interfaceSettings["ADDRESS"];
                    address = address.substr(0, address.find_last_of("."));
                    dhcpProxies += "RewriteRule ^" + address + ".([0-9:]*)$  http://" + address + ".$1/  [P,L]\n";
                    dhcpProxies += "RewriteRule ^" + address + ".([0-9:]*)/(.*)$  http://" + address + ".$1/$2  [P,L]\n\n";
                }

                content.append(interface).append("\nType=");
                if (startsWith(interface, "wl")) {
                    content.append("wlan\n\n");
                } else {
                    content.append("ether\n\n");
                }
                content.append("[Network]\n");
                if (dnsSettings["DNS1"] != "") {
                    content.append("DNS=").append(dnsSettings["DNS1"]).append("\n");
                }
                if (dnsSettings["DNS2"] != "") {
                    content.append("DNS=").append(dnsSettings["DNS2"]).append("\n");
                }
                if (interfaceSettings["PROTO"] == "dhcp") {
                    addressLines = "DHCP=yes\n";
                    DHCPSERVER = 0;
                } else {
                    addressLines = "Address=";
                    addressLines.append(interfaceSettings["ADDRESS"]).append("/").append(netmaskToSubnet(interfaceSettings["NETMASK"])).append("\n");
                }
                if (startsWith(interface, "wl")) {
                    // wifi settings
                    if (tetherEnabled == 1 && interface == tetherInterface) {
                        filesNeeded["/etc/hostapd/hostapd.conf"] = CreateHostAPDConfig(interface);
                        DHCPSERVER = 1;
                        hostapd = true;
                        addressLines.append("Address=192.168.8.1/24\n");
                    } else if (!interfaceSettings["SSID"].empty()) {
                        std::string wpa = "ctrl_interface=/var/run/wpa_supplicant\nctrl_interface_group=0\nupdate_config=1\ncountry=";
                        wpa.append(WifiRegulatoryDomain);
                        if (!interfaceSettings["WPA3"].empty()) {
                            wpa.append("\nsae_pwe=1\n");
                        }
                        wpa.append("\n\nnetwork={\n  ssid=\"").append(interfaceSettings["SSID"]);
                        if (!interfaceSettings["PSK"].empty()) {
                            wpa.append("\"\n  psk=\"").append(interfaceSettings["PSK"]);
                        }
                        wpa.append("\"\n  key_mgmt=");
                        if (!interfaceSettings["WPA3"].empty()) {
                            wpa.append("SAE WPA-PSK\n  sae_password=\"").append(interfaceSettings["PSK"]).append("\"");
                        } else {
                            wpa.append("WPA-PSK");
                        }
                        if (!interfaceSettings["HIDDEN"].empty()) {
                            wpa.append("\n  scan_ssid=1");
                        }
                        wpa.append("\n  priority=100\n  ieee80211w=1\n}\n\n");
                        if (!interfaceSettings["BACKUPSSID"].empty() && interfaceSettings["BACKUPSSID"] != "\"\"") {
                            wpa.append("\nnetwork={\n  ssid=\"").append(interfaceSettings["BACKUPSSID"]);
                            if (!interfaceSettings["BACKUPPSK"].empty()) {
                                wpa.append("\"\n  psk=\"").append(interfaceSettings["BACKUPPSK"]);
                            }
                            wpa.append("\"\n  key_mgmt=");
                            if (!interfaceSettings["BACKUPWPA3"].empty()) {
                                wpa.append("SAE WPA-PSK\n  sae_password=\"").append(interfaceSettings["PSK"]).append("\"");
                            } else {
                                wpa.append("WPA-PSK");
                            }
                            if (!interfaceSettings["BACKUPHIDDEN"].empty()) {
                                wpa.append("\n  scan_ssid=1");
                            }
                            wpa.append("\n  priority=90\n  ieee80211w=1\n}\n\n");
                        }
                        filesNeeded["/etc/wpa_supplicant/wpa_supplicant-" + interface + ".conf"] = wpa;
                        // Queue these unconditionally rather than gating them on
                        // `systemctl is-active`/`is-enabled` queries: during early
                        // boot the systemd manager is saturated (dbus etc. take many
                        // seconds to come up), so those synchronous queries block for
                        // ~9s and stall fppinit -- and this wpa_supplicant@ branch
                        // only runs once a wifi interface exists. The commands run
                        // only if the wpa config actually changed (see commandsToRun
                        // handling below), and `enable` + `reload-or-restart` are both
                        // idempotent, so the pre-checks bought nothing but the stall.
                        commandsToRun.emplace_back("systemctl enable \"wpa_supplicant@" + interface + ".service\" &");
                        commandsToRun.emplace_back("systemctl reload-or-restart \"wpa_supplicant@" + interface + ".service\" &");
                        postCommandsToRun.emplace_back("systemctl reload-or-restart \"wpa_supplicant@" + interface + ".service\" &");
                    } else {
                        validConfig = false;
                    }
                }
                if (DHCPSERVER) {
                    content.append("DHCPServer=yes\n");
                }
                if (IPFORWARDING == 1) {
                    content.append("IPForward=yes\n");
                    ipForward = true;
                } else if (IPFORWARDING == 2) {
                    // systemd-networkd might not have masquerade support compiled in, we'll just do it manually
                    ipForward = true;
                    exec("/usr/sbin/nft add table nat");
                    exec("/usr/sbin/nft 'add chain nat postrouting { type nat hook postrouting priority 100 ; }'");
                    exec("/usr/sbin/nft add rule nat postrouting oif " + interface + " masquerade");
                }
                content.append(addressLines);
                // some of the FPP7 images don't support this setting.  They use older gcc
                // Pi Zero 2 W has issues with IgnoreCarrierLoss causing network visibility problems (Issue #2487)
#ifdef PLATFORM_PI
                if (!isPiZero2W()) {
                    content.append("IgnoreCarrierLoss=5s\n");
                }
#else
                content.append("IgnoreCarrierLoss=5s\n");
#endif
                content.append("\n");

                // Apply global gateway to static interfaces only
                if (!globalGateway.empty() && interfaceSettings["PROTO"] != "dhcp") {
                    content.append("[Route]\nGateway=").append(globalGateway).append("\n");
                    if (ROUTEMETRIC != 0) {
                        content.append("Metric=").append(std::to_string(ROUTEMETRIC)).append("\n");
                    }
                }
                content.append("\n");
                if (interfaceSettings["PROTO"] == "dhcp") {
                    content.append("[DHCPv4]\nClientIdentifier=mac\nUseDomains=true\n");
                    // Only use NTP from DHCP if explicitly enabled
                    std::string useNTPFromDHCP;
                    getRawSetting("UseNTPFromDHCP", useNTPFromDHCP);
                    if (useNTPFromDHCP != "1") {
                        content.append("UseNTP=no\n");
                    }
                } else if (ROUTEMETRIC != 0) {
                    content.append("[DHCPv4]\n");
                }
                if (ROUTEMETRIC != 0) {
                    content.append("RouteMetric=").append(interfaceSettings["ROUTEMETRIC"]).append("\n\n");
                    content.append("[IPv6AcceptRA]\nRouteMetric=").append(interfaceSettings["ROUTEMETRIC"]).append("\n\n");
                } else {
                    content.append("\n");
                }
                if (DHCPSERVER == 1) {
                    content.append("[DHCPServer]\nPoolOffset=").append(std::to_string(DHCPOFFSET)).append("\nPoolSize=").append(std::to_string(DHCPPOOLSIZE)).append("\n");
                    if (!dnsSettings["DNS1"].empty()) {
                        content.append("EmitDNS=yes\nDNS=").append(dnsSettings["DNS1"]).append("\n");
                    }
                    content.append("\n");
                    if (FileExists(FPP_MEDIA_DIR + "config/leases." + interface)) {
                        content.append(GetFileContents(FPP_MEDIA_DIR + "config/leases." + interface));
                    }
                    content.append("\n");
                }
                if (validConfig) {
                    filesNeeded["/etc/systemd/network/10-" + interface + ".network"] = content;
                }
            }
        }
    }

    // If tethering is explicitly enabled (==1) but no interface configs set it up,
    // configure it now (handles fresh installs with no /config directory)
    if (tetherEnabled == 1 && !hostapd) {
        filesNeeded["/etc/hostapd/hostapd.conf"] = CreateHostAPDConfig(tetherInterface);
        std::string content = "[Match]\nName=";
        content.append(tetherInterface).append("\nType=wlan\n\n"
                                               "[Network]\n"
                                               "DHCP=no\n"
                                               "Address=192.168.8.1/24\n"
                                               "DHCPServer=yes\n\n");
        content.append("[DHCPServer]\n"
                       "PoolOffset=10\n"
                       "PoolSize=100\n"
                       "EmitDNS=no\n\n");
        if (FileExists(FPP_MEDIA_DIR + "config/leases." + tetherInterface)) {
            content.append(GetFileContents(FPP_MEDIA_DIR + "config/leases." + tetherInterface));
        }
        content.append("\n");
        filesNeeded["/etc/systemd/network/10-" + tetherInterface + ".network"] = content;
        hostapd = true;
    }

    bool reloadApache = false;
    if (dhcpProxies.empty() && FileExists(dhcpProxyFile)) {
        unlink(dhcpProxyFile.c_str());
        reloadApache = true;
    } else if (!dhcpProxies.empty()) {
        PutFileContents(dhcpProxyFile, dhcpProxies);
        reloadApache = true;
    }

    // Configure ntpsec to ignore DHCP NTP servers unless explicitly enabled
    std::string ntpsecDefaults = "/etc/default/ntpsec";
    std::string useNTPFromDHCP;
    getRawSetting("UseNTPFromDHCP", useNTPFromDHCP);
    std::string ignoreDHCP = (useNTPFromDHCP == "1") ? "" : "yes";

    std::string ntpsecConfig = GetFileContents(ntpsecDefaults);
    if (!ntpsecConfig.empty()) {
        // Update the IGNORE_DHCP setting in /etc/default/ntpsec
        std::string newConfig = ntpsecConfig;
        bool needsRestart = false;

        size_t pos = newConfig.find("IGNORE_DHCP=");
        if (pos != std::string::npos) {
            size_t lineEnd = newConfig.find('\n', pos);
            std::string oldLine = newConfig.substr(pos, lineEnd - pos);
            std::string newLine = "IGNORE_DHCP=\"" + ignoreDHCP + "\"";
            newConfig.replace(pos, oldLine.length(), newLine);
            needsRestart = (newConfig != ntpsecConfig);
        }

        // Ensure -g flag is set in NTPD_OPTS to allow large time corrections on boot
        // This is critical for systems without RTC that may boot with wildly incorrect times
        pos = newConfig.find("NTPD_OPTS=");
        if (pos != std::string::npos) {
            size_t lineEnd = newConfig.find('\n', pos);
            std::string optsLine = newConfig.substr(pos, lineEnd - pos);
            // Check if -g flag is already present
            if (optsLine.find("-g") == std::string::npos) {
                // Add -g flag after NTPD_OPTS="
                size_t quotePos = optsLine.find('"');
                if (quotePos != std::string::npos) {
                    std::string newOptsLine = optsLine.substr(0, quotePos + 1) + "-g " + optsLine.substr(quotePos + 1);
                    newConfig.replace(pos, optsLine.length(), newOptsLine);
                    needsRestart = true;
                }
            }
        }

        if (needsRestart) {
            PutFileContents(ntpsecDefaults, newConfig);
            // Remove any DHCP-generated NTP config to force reload
            if (ignoreDHCP == "yes" && FileExists("/run/ntpsec/ntp.conf.dhcp")) {
                unlink("/run/ntpsec/ntp.conf.dhcp");
            }
            execbg("/usr/bin/systemctl reload-or-restart ntpsec.service &");
        }
    }

    bool changed = false;
    for (auto& ftc : filesToConsider) {
        if (filesNeeded.find(ftc) == filesNeeded.end()) {
            // No longer used/needed, remove it
            unlink(ftc.c_str());
            changed = true;
            if (startsWith(ftc, "/etc/wpa_supplicant/")) {
                std::string intf = ftc.substr(ftc.find("-") + 1);
                intf = intf.substr(0, intf.find("."));
                execbg("/usr/bin/systemctl disable wpa_supplicant@" + intf + ".service &");
            }
        }
    }
    changed |= compareAndCopyFiles(filesNeeded);
    if (changed) {
        bool localFullReload = false;
        printf("Need to restart/reload stuff.\n");
        for (auto& c : commandsToRun) {
            printf("    %s\n", c.c_str());
            exec(c);
        }
        if (fullReload) {
            exec("/usr/bin/systemctl daemon-reload");
        }
        execbg("/usr/bin/systemctl reload-or-restart systemd-networkd.service &");
        if (hostapd) {
            unblockWifi();
            execbg("/usr/bin/systemctl reload-or-restart hostapd.service &");
        } else {
            if (contains(execAndReturn("/usr/bin/systemctl is-enabled hostapd"), "enabled")) {
                execbg("/usr/bin/systemctl disable hostapd.service &");
                execbg("/usr/bin/systemctl reload-or-restart systemd-networkd.service &");
                localFullReload = true;
            }
            if (!contains(execAndReturn("/usr/bin/systemctl is-active hostapd"), "inactive")) {
                exec("/usr/bin/systemctl stop hostapd.service");
                localFullReload = true;
            }
            exec("rm -f /home/fpp/media/tmp/wifi-*.ascii");
        }
        if (fullReload && localFullReload) {
            exec("/usr/bin/systemctl daemon-reload");
            exec("/usr/bin/systemctl reload-or-restart systemd-networkd.service");
            for (auto& c : postCommandsToRun) {
                printf("    %s\n", c.c_str());
                exec(c);
            }
        }
    }

    printf("FPP - Setting max IGMP memberships\n");
    exec("/usr/sbin/sysctl -w net/core/rmem_max=393216 net/core/wmem_max=393216 net/ipv4/igmp_max_memberships=512  > /dev/null 2>&1");
    if (ipForward) {
        exec("/usr/sbin/sysctl net.ipv4.ip_forward=1");
    } else {
        exec("/usr/sbin/sysctl net.ipv4.ip_forward=0");
    }
    if (reloadApache && (!contains(execAndReturn("/usr/bin/systemctl is-active apache2"), "inactive"))) {
        exec("/usr/bin/systemctl reload-or-restart apache2");
    }
}

// Check if there are any network interfaces that could potentially receive NTP time
// Returns true if there's at least one interface with a "real" IP address that could reach NTP servers
// Excludes loopback, USB gadget, and tethering interfaces
static bool hasNetworkInterfaceForNTP() {
    // If EnableTethering is explicitly set to 1, user expects to use tethering
    // which means no external network - skip NTP wait
    int tetheringEnabled = getRawSettingInt("EnableTethering", 0);
    if (tetheringEnabled == 1) {
        printf("FPP - Tethering is enabled, skipping NTP time wait\n");
        return false;
    }

    struct ifaddrs* ifAddrStruct = NULL;
    struct ifaddrs* ifa = NULL;
    void* tmpAddrPtr = NULL;
    bool hasValidIP = false;

    if (getifaddrs(&ifAddrStruct) != 0) {
        // If we can't get interface info, assume we might have network
        return true;
    }

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }

        std::string nm = ifa->ifa_name;
        // Skip loopback and USB gadget interfaces (usb0, usb1, etc.)
        if (nm == "lo" || startsWith(nm, "usb")) {
            continue;
        }

        // Only check IPv4 addresses
        if (ifa->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            std::string addr = addressBuffer;

            // Skip tethering/USB gadget IP addresses
            // 192.168.6.2/192.168.7.2 = BeagleBone USB gadget
            // 192.168.8.1 = FPP tethering hotspot
            if (contains(addr, "192.168.6.2") ||
                contains(addr, "192.168.7.2") ||
                contains(addr, "192.168.8.1")) {
                continue;
            }

            // Found a valid IP that could potentially reach NTP
            hasValidIP = true;
            break;
        }
    }

    if (ifAddrStruct != NULL) {
        freeifaddrs(ifAddrStruct);
    }

    // If no valid IP found, check if any "real" interfaces exist with link up
    // An interface must have carrier (link) to potentially get DHCP
    if (!hasValidIP) {
        // Re-scan to check if interfaces exist with carrier
        if (getifaddrs(&ifAddrStruct) == 0) {
            std::string tetherInterface = FindTetherWIFIAdapater();
            for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
                std::string nm = ifa->ifa_name;
                // Skip loopback and USB gadget interfaces
                if (nm == "lo" || startsWith(nm, "usb")) {
                    continue;
                }
                // Skip the interface designated for tethering
                if (nm == tetherInterface) {
                    continue;
                }
                // Check for "real" network interface names
                if (startsWith(nm, "eth") || startsWith(nm, "wlan") ||
                    startsWith(nm, "en") || startsWith(nm, "wl") ||
                    startsWith(nm, "br") || startsWith(nm, "bond")) {
                    // Check carrier state via sysfs - most reliable across all drivers
                    // /sys/class/net/<iface>/carrier returns 1 if link, 0 if no link
                    std::string carrierPath = "/sys/class/net/" + nm + "/carrier";
                    std::string carrier = GetFileContents(carrierPath);
                    TrimWhiteSpace(carrier);
                    if (carrier == "1") {
                        printf("FPP - Interface %s has carrier, will wait for NTP\n", nm.c_str());
                        hasValidIP = true;
                        break;
                    }
                }
            }
            if (ifAddrStruct != NULL) {
                freeifaddrs(ifAddrStruct);
            }
        }
    }
    return hasValidIP;
}

void handleBootDelay() {
    int i = getRawSettingInt("bootDelay", -1);
    const std::string delayFile = FPP_MEDIA_DIR + "/tmp/boot_delay";
    const std::string skipFile = FPP_MEDIA_DIR + "/tmp/boot_delay_skip";

    // bootDelay=0 means no delay at all - clean up any flag file and return immediately
    if (i == 0) {
        unlink(delayFile.c_str());
        unlink(skipFile.c_str());
        return;
    }

    if (i > 0) {
        printf("FPP - Sleeping for %d seconds\n", i);
        // Create flag file with start time and duration for UI countdown
        time_t startTime = time(nullptr);
        std::string flagContent = std::to_string(startTime) + "," + std::to_string(i);
        PutFileContents(delayFile, flagContent);
        // Notify systemd we're starting the delay and extend timeout
        sd_notify(0, "STATUS=Boot delay in progress");
        sd_notifyf(0, "EXTEND_TIMEOUT_USEC=%llu", (unsigned long long)(i + 30) * 1000000);

        // Sleep in 100ms increments to allow UI to show countdown and respond to skip
        int remainingMs = i * 1000;
        int watchdogCounter = 0;
        while (remainingMs > 0) {
            // Check for skip request from UI every iteration
            if (FileExists(skipFile)) {
                printf("FPP - Boot delay skip requested by user\n");
                unlink(skipFile.c_str());
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            remainingMs -= 100;
            watchdogCounter++;

            // Notify systemd watchdog every 10 seconds (100 iterations)
            if (watchdogCounter >= 100) {
                sd_notify(0, "WATCHDOG=1");
                watchdogCounter = 0;
            }
        }
        // Remove flag files when delay completes
        unlink(delayFile.c_str());
        unlink(skipFile.c_str());
    } else if (i == -1) {
        // Auto mode: check if we have any network interfaces that could get NTP
        // If not, skip the time wait entirely and clean up flag file now

        // On a Pi5, it takes about 4.1s from when fppinit is called to when eth0
        // will report a link up. Since we want to wait for NTP if the network is
        // available, we need to wait
        const auto processor_count = std::thread::hardware_concurrency();
        if (processor_count > 2) {
            int count = 0;
            while (!hasNetworkInterfaceForNTP() && count < 50) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                count++;
            }
            if (!hasNetworkInterfaceForNTP()) {
                printf("FPP - No network interface found, skipping boot delay\n");
                unlink(delayFile.c_str());
                unlink(skipFile.c_str());
                return;
            }
        }
    }
}

// Minimal SNTP (RFC 4330) client: query an NTP server over UDP and step the
// system clock to its transmit timestamp (second precision). ntpsec on Debian
// 13 ships no one-shot client (ntpdig was dropped) and "ntpd -gq" takes ~20s to
// produce a step; this sets the clock in ~1s so boot never blocks on it. The
// ntpsec daemon refines to sub-millisecond afterwards. Must run as root
// (clock_settime needs CAP_SYS_TIME). Returns true if the clock was set.
static bool quickSntpSet(const std::string& host, int timeoutSecs) {
    struct addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    struct addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), "123", &hints, &res) != 0 || res == nullptr) {
        return false;
    }
    bool ok = false;
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock >= 0) {
        struct timeval tv { timeoutSecs, 0 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        unsigned char pkt[48] = { 0 };
        pkt[0] = 0x1b; // LI=0, VN=3, Mode=3 (client)
        if (sendto(sock, pkt, sizeof(pkt), 0, res->ai_addr, res->ai_addrlen) == (ssize_t)sizeof(pkt)
            && recv(sock, pkt, sizeof(pkt), 0) == (ssize_t)sizeof(pkt)) {
            int stratum = pkt[1];
            // Transmit timestamp: 32-bit seconds since 1900, big-endian, at offset 40.
            uint32_t ntpSecs = ((uint32_t)pkt[40] << 24) | ((uint32_t)pkt[41] << 16) | ((uint32_t)pkt[42] << 8) | (uint32_t)pkt[43];
            // stratum 0 == "kiss o' death"/invalid; 16+ == unsynchronized.
            // 2208988800 = seconds between the 1900 (NTP) and 1970 (Unix) epochs.
            if (stratum > 0 && stratum < 16 && ntpSecs > 2208988800U) {
                struct timespec ts { (time_t)(ntpSecs - 2208988800U), 0 };
                ok = (clock_settime(CLOCK_REALTIME, &ts) == 0);
            }
        }
        close(sock);
    }
    freeaddrinfo(res);
    return ok;
}

// Ensure the clock is sane before fppd starts - called AFTER waitForInterfacesUp
// so we know network is available. On RTC-less boards the clock boots stale (at
// the image build date), which would make schedules and logs wrong. Rather than
// block boot for ~20s waiting for the ntpsec daemon to step the clock, do a
// quick one-shot SNTP set now and continue; the daemon refines it afterwards.
void handleTimeSyncWait() {
    int i = getRawSettingInt("bootDelay", -1);
    if (i != -1) {
        // Only do time sync wait in auto mode
        return;
    }

    const std::string delayFile = FPP_MEDIA_DIR + "/tmp/boot_delay";
    const std::string skipFile = FPP_MEDIA_DIR + "/tmp/boot_delay_skip";

    // Check if there are any network interfaces that could get NTP time
    // If not, skip - no point trying NTP on a device with no network
    if (!hasNetworkInterfaceForNTP()) {
        printf("FPP - No network interface found, skipping NTP time set\n");
        unlink(delayFile.c_str());
        unlink(skipFile.c_str());
        return;
    }

    struct stat attr;
    stat("/etc/fpp/rfs_version", &attr);
    time_t fileTime = attr.st_ctime;
    time_t currentTime = time(nullptr);

    if (difftime(fileTime, currentTime) <= 0) {
        // Clock is already at or past the image build date - nothing to do.
        unlink(delayFile.c_str());
        unlink(skipFile.c_str());
        return;
    }

    // Clock is stale (no RTC). Do a fast SNTP set against the configured server
    // and move on -- do NOT block on full NTP convergence. Use the first
    // pool/server from ntp.conf so a site-local time server is honoured; fall
    // back to FPP's pool.
    std::string ntpHost = execAndReturn(
        "/usr/bin/awk '/^(pool|server)[[:space:]]/ { print $2; exit }' /etc/ntpsec/ntp.conf 2>/dev/null");
    TrimWhiteSpace(ntpHost);
    if (ntpHost.empty()) {
        ntpHost = "falconplayer.pool.ntp.org";
    }

    printf("FPP - System clock is stale; doing a quick SNTP set from %s\n", ntpHost.c_str());
    sd_notify(0, "STATUS=Setting system clock via SNTP");
    bool set = false;
    for (int attempt = 0; attempt < 2 && !set; ++attempt) {
        set = quickSntpSet(ntpHost, 2);
    }
    if (set) {
        currentTime = time(nullptr);
        struct tm tmNow;
        localtime_r(&currentTime, &tmNow);
        char buffer[26];
        strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", &tmNow);
        printf("FPP - Clock set via SNTP to %s; ntpsec will refine it in the background\n", buffer);
    } else {
        printf("FPP - Quick SNTP set failed (no time server reachable yet); continuing, ntpsec will sync when available\n");
    }
    unlink(delayFile.c_str());
    unlink(skipFile.c_str());
}

void checkWLANInterface() {
    if (contains(execAndReturn("/usr/bin/systemctl is-enabled wpa_supplicant@wlan0"), "enabled") && contains(execAndReturn("/usr/bin/systemctl is-active wpa_supplicant@wlan0"), "inactive")) {
        printf("Need to restart wpa_supplicant@wlan0\n");
        execbg("systemctl restart \"wpa_supplicant@wlan0.service\" &");
    }
}

// Build the spoken "I Have Found The Following I P Addresses, ..." string from
// the current non-tether IPv4 addresses. Returns "" if none are present yet.
static std::string buildIPAnnounceString() {
    std::string announce = "I Have Found The Following I P Addresses";
    bool found = false;
    struct ifaddrs* ifAddrStruct = NULL;
    struct ifaddrs* ifa = NULL;
    void* tmpAddrPtr = NULL;
    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        std::string nm = ifa->ifa_name;
        if (startsWith(nm, "usb") || startsWith(nm, "lo")) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            std::string addr = addressBuffer;
            if (!contains(addr, "192.168.6.2") && !contains(addr, "192.168.7.2") && !contains(addr, "192.168.8.1")) {
                printf("FPP - Found %s IP Address %s\n", ifa->ifa_name, addressBuffer);
                announce += ", " + std::string(addressBuffer);
                found = true;
            }
        }
    }
    if (ifAddrStruct != NULL) {
        freeifaddrs(ifAddrStruct);
    }
    return found ? announce : "";
}

bool waitForInterfacesUp(int timeOut) {
    int count = 0;
    // If no network interfaces have carrier/link, don't wait for IP address - likely no network available and no point waiting for DHCP/NTP
    while (!hasNetworkInterfaceForNTP()) {
        if (count >= (timeOut / 2)) { // spend half the timeOut waiting for interfaces to have link, then give up
            printf("FPP - No network interfaces with link detected after waiting for %d ms, skipping IP wait\n", timeOut * 200);
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ++count;
    }
    printf("FPP - Waited for %0.1f seconds for network interfaces to have link...\n", (((float)count) * 0.2));
    std::string announce;
    do {
        announce = buildIPAnnounceString();
        if (announce.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            ++count;
        }
    } while (announce.empty() && (count < timeOut));
    if (announce.empty()) {
        printf("FPP - Could not get a valid IP address\n");
        return false;
    }
    printf("FPP - Waited for %0.1f seconds for IP address\n", (((float)count) * 0.2f));
    return true;
}

// Announce the device's IP address(es) over audio with flite + pw-play. Invoked
// from fpp-announce-ip.service (ordered after fpp_postnetwork and the PipeWire
// services) rather than inline in postNetwork, so the CPU-heavy flite synthesis
// doesn't compete with the tail of postNetwork / fppd startup on single-core
// boards. Network is already up by the time the service runs.
void announceIPAddresses() {
    if (getRawSettingInt("disableIPAnnouncement", 0) || !FileExists("/usr/bin/flite")) {
        return;
    }
    std::string announce = buildIPAnnounceString();
    if (announce.empty()) {
        printf("FPP - announceIP: no usable IP address to announce\n");
        return;
    }
    // flite is an ALSA client, so playing directly (flite -t) would route
    // through the pipewire-alsa plugin + /root/.asoundrc -- the fragile ALSA
    // chain we're retiring. Instead render to a WAV and play it through PipeWire
    // natively with pw-play. pw-play needs the runtime env vars to find FPP's
    // PipeWire instance at /run/pipewire-fpp. /run/fppd is created by setupAudio.
    const std::string announceWav = "/run/fppd/ip-announce.wav";
    std::string fliteCmd =
        "/usr/bin/flite -voice awb -t \"" + announce + "\" -o " + announceWav +
        " && PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp"
        " /usr/bin/pw-play " + announceWav;
    exec(fliteCmd);
    unlink(announceWav.c_str()); // don't leave the rendered WAV behind in /run
}
static void disableWLANPowerManagement() {
    struct ifaddrs* ifAddrStruct = NULL;
    struct ifaddrs* ifa = NULL;
    void* tmpAddrPtr = NULL;
    getifaddrs(&ifAddrStruct);
    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        std::string nm = ifa->ifa_name;
        if (startsWith(nm, "wl")) {
            printf("FPP - Disabling power saving for %s\n", nm.c_str());
            exec("/usr/sbin/iw dev " + nm + " set power_save off 2>/dev/null > /dev/null");
        }
    }
}

void maybeEnableTethering() {
    int te = getRawSettingInt("EnableTethering", 0);
    if (te == 0) {
        bool found = false;
        struct ifaddrs* ifAddrStruct = NULL;
        struct ifaddrs* ifa = NULL;
        void* tmpAddrPtr = NULL;
        getifaddrs(&ifAddrStruct);
        for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) {
                continue;
            }
            std::string nm = ifa->ifa_name;
            if (startsWith(nm, "usb") || startsWith(nm, "lo")) {
                continue;
            }
            if (ifa->ifa_addr->sa_family == AF_INET) { // check it is IP4
                // is a valid IP4 Address
                tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                std::string addr = addressBuffer;
                if (!contains(addr, "192.168.6.2") && !contains(addr, "192.168.7.2") && !contains(addr, "192.168.8.1")) {
                    found = true;
                }
            }
        }
        if (ifAddrStruct != NULL) {
            freeifaddrs(ifAddrStruct);
        }
        if (!found) {
            // Before enabling tethering, check if any wired interface has carrier/link.
            // If a cable is connected, DHCP will likely succeed shortly - don't enable tethering.
            // This matches the original bash MaybeEnableTethering logic that checked ethtool link.
            bool hasWiredLink = false;
            for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net/")) {
                std::string dev = entry.path().filename();
                if (startsWith(dev, "eth") || startsWith(dev, "en") || startsWith(dev, "br") || startsWith(dev, "bond")) {
                    std::string carrierPath = "/sys/class/net/" + dev + "/carrier";
                    if (FileExists(carrierPath)) {
                        std::string carrier = GetFileContents(carrierPath);
                        TrimWhiteSpace(carrier);
                        if (carrier == "1") {
                            printf("FPP - Wired interface %s has link, not enabling tethering\n", dev.c_str());
                            hasWiredLink = true;
                            break;
                        }
                    }
                }
            }
            if (hasWiredLink) {
                // A wired interface has link - DHCP should complete soon.
                // Don't enable tethering; the routable.d/check_tether dispatcher
                // will clean up tethering if it was enabled from a previous boot.
                return;
            }
            // Check if the tether interface has a wifi client configuration
            std::string tetherInterface = FindTetherWIFIAdapater();
            std::string interfaceConfigFile = FPP_MEDIA_DIR + "/config/interface." + tetherInterface;
            if (FileExists(interfaceConfigFile)) {
                auto interfaceSettings = loadSettingsFile(interfaceConfigFile);
                if (!interfaceSettings["SSID"].empty()) {
                    // WiFi client config exists - give it time to connect and get DHCP
                    printf("FPP - %s has WiFi SSID configured, waiting up to 12s for connection and DHCP...\n", tetherInterface.c_str());
                    int waitCount = 0;
                    bool gotIP = false;
                    bool connected = false;
                    while (waitCount < 24 && !gotIP && !connected) { // 24 * 500ms = 12 seconds
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        // Check for IP address
                        struct ifaddrs* ifAddrCheck = NULL;
                        if (getifaddrs(&ifAddrCheck) == 0) {
                            for (struct ifaddrs* ifa = ifAddrCheck; ifa != NULL; ifa = ifa->ifa_next) {
                                if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                                    std::string ifname = ifa->ifa_name;
                                    if (ifname == tetherInterface) {
                                        void* tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
                                        char addressBuffer[INET_ADDRSTRLEN];
                                        inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                                        std::string addr = addressBuffer;
                                        if (!contains(addr, "169.254.")) { // Ignore link-local
                                            printf("FPP - %s got IP address %s\n", tetherInterface.c_str(), addr.c_str());
                                            gotIP = true;
                                            break;
                                        }
                                    }
                                }
                            }
                            freeifaddrs(ifAddrCheck);
                        }
                        // Also check if connected to AP (even without IP yet)
                        if (!gotIP && waitCount > 10) { // After 5 seconds, start checking connection status
                            std::string iwOutput = execAndReturn("/usr/sbin/iw " + tetherInterface + " link");
                            if (contains(iwOutput, "Connected to")) {
                                printf("FPP - %s connected to WiFi, keeping WiFi config active\n", tetherInterface.c_str());
                                connected = true;
                            }
                        }
                        if (gotIP || connected)
                            break;
                        waitCount++;
                    }
                    if (!gotIP && !connected) {
                        printf("FPP - %s no IP or connection after 12s, enabling tethering\n", tetherInterface.c_str());
                        te = 1;
                    }
                } else {
                    // Interface config exists but no SSID - enable tethering
                    te = 1;
                }
            } else {
                // did not find an ip address and no wifi client config exists
                te = 1;
            }
        }
    }
    std::string tetherInterface = FindTetherWIFIAdapater();
    if (te == 1) {
        if (!FileExists("/sys/class/net/" + tetherInterface)) {
            printf("No interface for tethering.\n");
            te = 2;
        }
    }
    if (te == 1) {
        std::string c = CreateHostAPDConfig(tetherInterface);
        PutFileContents("/etc/hostapd/hostapd.conf", c);

        // Remove wpa_supplicant config if it exists (switching from client to AP mode)
        std::string wpaConfig = "/etc/wpa_supplicant/wpa_supplicant-" + tetherInterface + ".conf";
        if (FileExists(wpaConfig)) {
            printf("FPP - Removing wpa_supplicant config for %s to enable tethering\n", tetherInterface.c_str());
            unlink(wpaConfig.c_str());
            exec("/usr/bin/systemctl stop wpa_supplicant@" + tetherInterface + ".service");
            exec("/usr/bin/systemctl disable wpa_supplicant@" + tetherInterface + ".service");
        }

        std::string content = "[Match]\nName=";
        content.append(tetherInterface).append("\nType=wlan\n\n"
                                               "[Network]\n"
                                               "DHCP=no\n"
                                               "Address=192.168.8.1/24\n"
                                               "DHCPServer=yes\n\n");

        content.append("[DHCPServer]\n"
                       "PoolOffset=10\n"
                       "PoolSize=100\n"
                       "EmitDNS=no\n\n");
        PutFileContents("/etc/systemd/network/10-" + tetherInterface + ".network", content);
        unblockWifi();
        exec("/usr/bin/systemctl reload-or-restart systemd-networkd.service");
        unblockWifi();
        exec("/usr/bin/systemctl reload-or-restart hostapd.service");
        exec("/usr/bin/systemctl enable hostapd.service");
    }
}
void detectNetworkModules() {
    std::string content;
    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net/")) {
        const std::string filename = entry.path().filename();
        std::string modName = "/sys/class/net/" + filename + "/device/driver/module";
        if (FileExists(modName)) {
            char buf[256];
            size_t l = readlink(modName.c_str(), buf, sizeof(buf));
            buf[l] = 0;
            std::string mod = buf;
            mod = mod.substr(mod.rfind("/") + 1);
            content += mod;
            content += "\n";
        }
    }
    if (!content.empty()) {
        PutFileContents("/etc/modules-load.d/fpp-network.conf", content);
        // PutFileContents("/home/fpp/media/config/fpp-network-modules.conf", content);
    }
}
void removeDummyInterface() {
    exec("ip link delete dummy0 > /dev/null 2>&1");
}
