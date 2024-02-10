/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2024 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the CC-BY-ND as described in the
 * included LICENSE.CC-BY-ND file.  This file may be modified for
 * personal use, but modified copies MAY NOT be redistributed in any form.
 */

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <array>
#include <dirent.h>
#include <filesystem>
#include <iomanip>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <thread>
#include <unistd.h>

#include "common_mini.h"
#include "non-gpl/CapeUtils/CapeUtils.h"

static const std::string FPP_MEDIA_DIR = "/home/fpp/media";

int remove_recursive(const char* const path, bool removeThis = true) {
    DIR* const directory = opendir(path);
    if (directory) {
        struct dirent* entry;
        while ((entry = readdir(directory))) {
            if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name)) {
                continue;
            }
            char filename[strlen(path) + strlen(entry->d_name) + 2];
            sprintf(filename, "%s/%s", path, entry->d_name);
            if (entry->d_type == DT_DIR) {
                if (remove_recursive(filename)) {
                    closedir(directory);
                    return -1;
                }
            } else {
                if (remove(filename)) {
                    closedir(directory);
                    return -1;
                }
            }
        }
        if (closedir(directory)) {
            return -1;
        }
    }
    if (removeThis) {
        return remove(path);
    }
    return 0;
}

static void DetectCape() {
#ifdef CAPEDETECT
    remove_recursive("/home/fpp/media/tmp/", false);
    CapeUtils::INSTANCE.initCape(false);
#endif
}

static void exec(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        printf(buffer.data());
    }
}
static std::string execAndReturn(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    std::string ret;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        ret += std::string(buffer.data());
    }
    return ret;
}

static void modprobe(const char* mod) {
    std::string cmd = std::string("/usr/sbin/modprobe ") + mod;
    exec(cmd);
}

static void checkSSHKeys() {
    for (const auto& entry : std::filesystem::directory_iterator("/etc/ssh")) {
        if (entry.is_regular_file()) {
            const auto filename = entry.path().filename();
            if (contains(filename, "ssh_host")) {
                return;
            }
        }
    }
    printf("      - Renerating SSH keys\n");
    exec("ssh-keygen -A");
}

// Copies files/config from the /boot partition to /home/fpp/media
// On the Pi, /boot (or /boot/firmware) is vfat and thus the
// user could stick a default config on there
static void handleBootPartition() {
    std::string bootDir = "/boot";
    if (DirectoryExists("/boot/firmware")) {
        bootDir = "/boot/firmware";
    }
    if (FileExists(bootDir + "/fpp_boot.sh")) {
        std::string cmd = "/bin/bash " + bootDir + "/fpp_boot.sh";
        exec(cmd);
    }
    if (DirectoryExists(bootDir + "/fpp")) {
        if (!FileExists(bootDir + "/fpp/copy_done")) {
            std::string cmd = "/usr/bin/cp -a " + bootDir + "/fpp/* " + FPP_MEDIA_DIR;
            PutFileContents(bootDir + "/fpp/copy_done", "1");
        }
    }
}

static void checkHostName() {
    std::string hn;
    if (FileExists("/.dockerenv")) {
        std::string CID = execAndReturn("head -1 /proc/1/cgroup | sed -e \"s/.*docker-//\" | cut -c1-12");
        hn = execAndReturn("hotname");
        TrimWhiteSpace(hn);
        setRawSetting("HostName", hn);
        setRawSetting("HostDescription", "Docker ID: " + CID);
    } else if (getRawSetting("HostName", hn)) {
        char hostname[256];
        int result = gethostname(hostname, 256);
        if (result == 0 && hn != hostname) {
            // need to reset the hostname
            printf("Changing hostname from %s to %s\n", hostname, hn.c_str());
            std::string cmd = "/usr/bin/hostname " + hn;
            exec(cmd);

            PutFileContents("/etc/hostname", hn + "\n");

            std::string hosts = GetFileContents("/etc/hosts");
            std::vector<std::string> lines = split(hosts, '\n');
            hosts = "";
            bool found = false;
            for (auto& line : lines) {
                if (line.starts_with("127.0.1.1")) {
                    hosts += "127.0.1.1\t";
                    hosts += hn;
                    found = true;
                } else {
                    hosts += line;
                }
                hosts += "\n";
            }
            if (!found) {
                hosts += "127.0.1.1\t";
                hosts += hn;
            }
            PutFileContents("/etc/hosts", hosts);
            exec("systemctl restart avahi-daemon");
        }
    }
}

static void runScripts(const std::string tp, bool userBefore = true) {
    if (userBefore && FileExists(FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh")) {
        exec("/bin/bash " + FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh " + tp);
    }
    for (const auto& entry : std::filesystem::directory_iterator(FPP_MEDIA_DIR + "/plugins")) {
        if (entry.is_directory()) {
            std::string filename = entry.path().filename();
            std::string cmd = FPP_MEDIA_DIR + "/plugins/" + filename + "/scripts/" + tp + ".sh";
            if (FileExists(cmd)) {
                printf("Running %s\n", cmd.c_str());
                exec("/bin/bash " + cmd);
            }
        }
    }
    if (!userBefore && FileExists(FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh")) {
        exec("/bin/bash " + FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh " + tp);
    }
}

static void checkFSTAB() {
    std::string cont = GetFileContents("/etc/fstab");
    if (cont.find("home/fpp/media") == std::string::npos) {
        cont += "\n";
        cont += "#####################################\n";
        cont += "#/dev/sda1     /home/fpp/media  auto    defaults,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500  0  0\n";
        cont += "#####################################\n";
        PutFileContents("/etc/fstab", cont);
    }
}

static void createDirectories() {
    static std::vector<std::string> DIRS = { "", "config", "effects", "logs", "music", "playlists", "scripts", "sequences",
                                             "upload", "videos", "plugins", "plugindata", "exim4", "images", "cache",
                                             "backups", "tmp" };
    printf("FPP - Checking for required directories\n");
    struct passwd* pwd = getpwnam("fpp");
    for (auto& d : DIRS) {
        std::string dir = FPP_MEDIA_DIR + "/" + d;
        if (!DirectoryExists(dir)) {
            printf("    Creating directory %s\n", dir.c_str());
            mkdir(dir.c_str(), 0775);
            if (pwd) {
                chown(dir.c_str(), pwd->pw_uid, pwd->pw_gid);
            }
        }
    }
}

static void setupApache() {
    static const std::string HTACCESSS = FPP_MEDIA_DIR + "/config/.htaccess";
    static const std::string HTPWD = FPP_MEDIA_DIR + "/config/.htpasswd";
    if (!FileExists(HTACCESSS)) {
        std::string content = "Allow from All\nSatisfy Any\nSetEnvIf Host ^ LOCAL_PROTECT=0\n";
        PutFileContents(HTACCESSS, content);
    } else {
        std::string content = GetFileContents(HTACCESSS);
        if (content.find("php_value") != std::string::npos) {
            printf("    Modifying .htaccessfile\n");
            std::vector<std::string> lines = split(content, '\n');
            content = "";
            for (auto& line : lines) {
                if (line.find("php_value") == std::string::npos) {
                    content += line;
                }
                content += "\n";
            }
            PutFileContents(HTACCESSS, content);
        }
    }
    if (!FileExists(HTPWD)) {
        PutFileContents(HTPWD, "");
    }
    if (!FileExists("/opt/fpp/www/proxy/.htaccess")) {
        printf("Creating proxy .htaccess link\n");
        if (!FileExists("/home/fpp/media/config/proxies")) {
            PutFileContents("/home/fpp/media/config/proxies", "");
        }
        symlink("/home/fpp/media/config/proxies", "/opt/fpp/www/proxy/.htaccess");
    }
}

void handleBootActions() {
    std::string v;
    if (getRawSetting("BootActions", v) && !v.empty()) {
        exec("/opt/fpp/scripts/handle_boot_actions");
    }
}

void configureBBB() {
#ifdef PLATFORM_BBB
    if (FileExists("/dev/mmcblk1")) {
        // full size beagle, check the bootloader
        int fd = open("/dev/mmcblk1", O_RDONLY);
        lseek(fd, 393488, SEEK_SET);
        uint8_t buf[25];
        read(fd, buf, 23);
        close(fd);
        buf[23] = 0;
        std::string uboot = (const char*)buf;
        if (uboot != "U-Boot 2022.04-gbaca7b4") {
            exec("/opt/fpp/bin.bbb/bootloader/install.sh");
            setRawSetting("rebootFlag", "1");
        }
    } else {
        // its a pocketbeagle
        // bug in kernel/device tree on pcoketbeagle where in P2-36 is not
        // properly set to be AIN so sensors won't read properly
        exec("i2cset -y -f 0 0x24 9 5");
    }

    // Beagle LEDS
    std::string led;
    if (getRawSetting("BBBLedPWR", led) && !led.empty()) {
        if (led == "0") {
            led = "0x00";
        } else {
            led = "0x38";
        }
        exec("i2cset -f -y 0 0x24 0x0b 0x6e");
        exec("i2cset -f -y 0 0x24 0x13 " + led);
        exec("i2cset -f -y 0 0x24 0x0b 0x6e");
        exec("i2cset -f -y 0 0x24 0x13 " + led);
    }
    if (getRawSetting("BBBLeds0", led) && !led.empty()) {
        PutFileContents("/sys/class/leds/beaglebone:green:usr0/trigger", led);
    }
    if (getRawSetting("BBBLeds1", led) && !led.empty()) {
        PutFileContents("/sys/class/leds/beaglebone:green:usr1/trigger", led);
    }
    if (getRawSetting("BBBLeds2", led) && !led.empty()) {
        PutFileContents("/sys/class/leds/beaglebone:green:usr2/trigger", led);
    }
    if (getRawSetting("BBBLeds3", led) && !led.empty()) {
        PutFileContents("/sys/class/leds/beaglebone:green:usr3/trigger", led);
    }
#endif
}
static std::string FindTetherWIFIAdapater() {
    std::string ret;
    if (!getRawSetting("TetherInterface", ret) || ret.empty()) {
        // see if we can find a non-associated interface
        for (const auto& entry : std::filesystem::directory_iterator("/sys/class/net/")) {
            std::string dev = entry.path().filename();
            if (startsWith(dev, "wl")) {
                std::string output = execAndReturn("iwconfig " + dev);
                if (contains(output, "Not-Associated")) {
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
#if __GNUC__ > 11
    content.append("reassociation_deadline=3000\npmk_r1_push=1\nft_over_ds=0\nbss_transition=1\n");
#endif
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
    }

    return content;
}
static void setupNetwork() {
    if (FileExists("/usr/sbin/rfkill")) {
        exec("/usr/sbin/rfkill unblock wifi");
        exec("/usr/sbin/rfkill unblock all");
    }
    auto dnsSettings = loadSettingsFile(FPP_MEDIA_DIR + "/config/dns");
    std::string dns = dnsSettings["DNS1"];
    if (dnsSettings["DNS2"] != "") {
        dns += ",";
        dns += dnsSettings["DNS2"];
    }
    std::string WifiRegulatoryDomain = "US";
    getRawSetting("WifiRegulatoryDomain", WifiRegulatoryDomain);
    exec("iw reg set " + WifiRegulatoryDomain);

    int tetherEnabled = getRawSettingInt("EnableTethering", 0);
    std::string tetherInterface = FindTetherWIFIAdapater();

    bool ipForward = false;
    bool hostapd = false;
    std::map<std::string, std::string> filesNeeded;
    std::list<std::string> filesToConsider;
    std::list<std::string> commandsToRun;
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
    for (const auto& entry : std::filesystem::directory_iterator(FPP_MEDIA_DIR + "/config")) {
        std::string dev = entry.path().filename();
        if (startsWith(dev, "interface.")) {
            auto interfaceSettings = loadSettingsFile(FPP_MEDIA_DIR + "/config/" + dev);
            std::string interface = dev.substr(10);

            std::string content = "[Match]\nName=";
            std::string addressLines;

            int DHCPSERVER = getIntFromMap(interfaceSettings, "DHCPSERVER", 0);
            int DHCPOFFSET = getIntFromMap(interfaceSettings, "DHCPOFFSET", 100);
            int DHCPPOOLSIZE = getIntFromMap(interfaceSettings, "DHCPPOOLSIZE", 50);
            int ROUTEMETRIC = getIntFromMap(interfaceSettings, "ROUTEMETRIC", 0);
            int IPFORWARDING = getIntFromMap(interfaceSettings, "IPFORWARDING", 0);

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
                    wpa.append(WifiRegulatoryDomain).append("\n\nnetwork={\n  ssid=").append(interfaceSettings["SSID"]);
                    if (!interfaceSettings["PSK"].empty()) {
                        wpa.append("\n  psk=").append(interfaceSettings["PSK"]);
                    }
                    wpa.append("\n  key_mgmt=WPA-PSK\n  scan_ssid=1\n  priority=100\n}\n\n");
                    if (!interfaceSettings["BACKUPSSID"].empty() && interfaceSettings["BACKUPSSID"] != "\"\"") {
                        wpa.append("\nnetwork={\n  ssid=").append(interfaceSettings["BACKUPSSID"]);
                        if (!interfaceSettings["BACKUPPSK"].empty()) {
                            wpa.append("\n  psk=").append(interfaceSettings["BACKUPPSK"]);
                        }
                        wpa.append("\n  key_mgmt=WPA-PSK\n  scan_ssid=1\n  priority=100\n}\n\n");
                    }
                    filesNeeded["/etc/wpa_supplicant/wpa_supplicant-" + interface + ".conf"] = wpa;
                    commandsToRun.emplace_back("systemctl enable \"wpa_supplicant@" + interface + ".service\"");
                    commandsToRun.emplace_back("systemctl reload-or-restart \"wpa_supplicant@" + interface + ".service\"");
                }
            }
            if (DHCPSERVER) {
                content.append("DHCPServer=yes\n");
            }
            if (IPFORWARDING == 1) {
                content.append("IPForward=yes\n");
                ipForward = true;
            } else if (IPFORWARDING == 2) {
                // systemd-networkd might not have masqarate support compiled in, we'll just do it manually
                ipForward = true;
                exec("modprobe iptable_nat");
                exec("nft add table nat");
                exec("nft 'add chain nat postrouting { type nat hook postrouting priority 100 ; }'");
                exec("nft add rule nat postrouting oif " + interface + " masquerade");
            }
            content.append(addressLines);
#if __GNUC__ > 11
            // some of the FPP7 images don't support this setting.  They use older gcc
            content.append("IgnoreCarrierLoss=5s\n");
#endif
            content.append("\n");

            if (!interfaceSettings["GATEWAY"].empty()) {
                content.append("[Route]\nGateway=").append(interfaceSettings["GATEWAY"]).append("\n");
                if (ROUTEMETRIC != 0) {
                    content.append("Metric=").append(interfaceSettings["ROUTEMETRIC"]).append("\n");
                }
            }
            content.append("\n");
            if (interfaceSettings["PROTO"] == "dhcp") {
                content.append("[DHCPv4]\nClientIdentifier=mac\n");
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

                if (!FileExists(FPP_MEDIA_DIR + "/config/proxies")) {
                    // DHCPServer is on, lets make sure proxies are enabled
                    // so they can be found via the proxies page
                    std::string c2 = "RewriteEngine on\nRewriteBase /proxy/\n\nRewriteRule ^(.*)/(.*)$  http://$1/$2  [P,L]\nRewriteRule ^(.*)$  $1/  [R,L]\n";
                    PutFileContents(FPP_MEDIA_DIR + "/config/proxies", c2);
                }
            }
            filesNeeded["/etc/systemd/network/10-" + interface + ".network"] = content;
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
                exec("systemctl disable wpa_supplicant@" + intf + ".service");
            }
        }
    }
    changed |= compareAndCopyFiles(filesNeeded);
    if (changed) {
        printf("Need to restart/reload stuff\n");
        for (auto& c : commandsToRun) {
            exec(c);
        }
        exec("systemctl reload-or-restart systemd-networkd.service");
        if (hostapd) {
            exec("systemctl reload-or-restart hostapd.service");
        } else {
            if (contains(execAndReturn("systemctl is-enabled hostapd"), "enabled")) {
                exec("systemctl disable hostapd.service");
            }
            if (!contains(execAndReturn("systemctl is-active hostapd"), "inactive")) {
                exec("systemctl stop hostapd.service");
            }
        }
    }

    printf("FPP - Setting max IGMP memberships\n");
    exec("sysctl net/ipv4/igmp_max_memberships=512 > /dev/null 2>&1");
    if (ipForward) {
        exec("sysctl net.ipv4.ip_forward=1");
    } else {
        exec("sysctl net.ipv4.ip_forward=0");
    }
}

static void configureScreenBlanking() {
    PutFileContents("/sys/class/graphics/fbcon/cursor_blink", "0");
    int i = getRawSettingInt("screensaver", 0);
    if (i) {
        int to = getRawSettingInt("screensaverTimeout", 0);
        printf("FPP - Screen blanking set to %d minute(s)", to);
        if (to == 0) {
            exec("TERM=linux setterm --clear all --blank 1  >> /dev/tty0");
            exec("dd if=/dev/zero of=/dev/fb0 > /dev/null 2>&1");
        } else {
            exec("TERM=linux setterm --clear all --blank " + std::to_string(to) + "  >> /dev/tty0");
        }
    }
}
static void setFileOwnership() {
    exec("chown -R fpp:fpp " + FPP_MEDIA_DIR);
}
static void checkUnpartitionedSpace() {
    if (!FileExists("/etc/fpp/desktop")) {
        std::string sourceDev = execAndReturn("findmnt -n -o SOURCE " + FPP_MEDIA_DIR);
        TrimWhiteSpace(sourceDev);
        if (sourceDev.empty()) {
            sourceDev = execAndReturn("findmnt -n -o SOURCE /");
            TrimWhiteSpace(sourceDev);
        }
        if (!sourceDev.empty()) {
            sourceDev = sourceDev.substr(5);
            std::string osd;
            getRawSetting("storageDevice", osd);
            if (osd != sourceDev) {
                setRawSetting("storageDevice", osd);
            }
        }
        std::string fs = "0";
        if (FileExists("/dev/mmcblk0")) {
            fs = execAndReturn("sfdisk -F /dev/mmcblk0 | tail -n 1");
            TrimWhiteSpace(fs);
            auto splits = split(fs, ' ');
            fs = splits.back();
            if (endsWith(fs, "G")) {
                fs = fs.substr(0, fs.size() - 1);
            } else {
                fs = "0";
            }
        }
        std::string oldfs;
        getRawSetting("UnpartitionedSpace", oldfs);
        if (oldfs != "fs") {
            setRawSetting("UnpartitionedSpace", fs);
        }
    }
}

int main(int argc, char* argv[]) {
    std::string action = "start";
    if (argc > 1) {
        action = argv[1];
    }
    printf("------------------------------\nRunning FPPINIT %s\n", action.c_str());
    if (action == "start") {
        createDirectories();
        DetectCape();
        int reboot = getRawSettingInt("rebootFlag", 0);
        if (reboot) {
            printf("FPP - Clearing reboot flags\n");
            setRawSetting("rebootFlag", "0");
        }
        checkSSHKeys();
        handleBootPartition();
        checkHostName();
        checkFSTAB();
        setupApache();
        handleBootActions();
        runScripts("boot", true);
        int skipNetwork = FileExists("/etc/fpp/desktop") || getRawSettingInt("SkipNetworkReset", 0);
        if (!skipNetwork) {
            setupNetwork();
        }
        configureBBB();
        configureScreenBlanking();
#ifdef PLATFORM_PI
        exec("/opt/fpp/scripts/runFunction setupChannelOutputs");
#endif
        checkUnpartitionedSpace();
        setFileOwnership();
    } else if (action == "postNetwork") {
        // for now...
        exec("/opt/fpp/scripts/fpp_boot");
        setFileOwnership();
    } else if (action == "runPreStartScripts") {
        runScripts("preStart", true);
    } else if (action == "runPostStartScripts") {
        runScripts("postStart", false);
    } else if (action == "runPreStopScripts") {
        runScripts("preStop", true);
    } else if (action == "runPostStopScripts") {
        runScripts("postStop", false);
    }
    return 0;
}