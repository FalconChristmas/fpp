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

#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <array>
#include <dirent.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "common_mini.h"
#include <arpa/inet.h>
#include <ifaddrs.h>

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

#ifdef PLATFORM_BB64
constexpr std::string SD_CARD_DEVICE = "/dev/mmcblk1";
#else
constexpr std::string SD_CARD_DEVICE = "/dev/mmcblk0";
#endif

#ifdef PLATFORM_PI
constexpr std::string I2C_DEV = "/dev/i2c-1";
#else
constexpr std::string I2C_DEV = "/dev/i2c-2";
#endif

static const std::string FPP_MEDIA_DIR = "/home/fpp/media";

void teeOutput(const std::string& log) {
    std::string cmd = "tee -a ";
    cmd += log;
    mkdir("/home/fpp/media/", 0775);
    mkdir("/home/fpp/media/logs", 0775);

    FILE* f = popen(cmd.c_str(), "w");
    if (dup2(fileno(f), STDOUT_FILENO) < 0) {
        printf("Couldn't redirect output to log file\n");
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
}

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
static int execbg(const std::string& cmd) {
    return system(cmd.c_str());
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

static bool LoadJsonFromString(const std::string& str, Json::Value& root) {
    Json::CharReaderBuilder builder;
    Json::CharReader* reader = builder.newCharReader();
    std::string errors;
    builder["collectComments"] = false;
    bool success = reader->parse(str.c_str(), str.c_str() + str.size(), &root, &errors);
    delete reader;
    if (!success) {
        Json::Value empty;
        root = empty;
        return false;
    }
    return true;
}
std::string SaveJsonToString(const Json::Value& root) {
    Json::StreamWriterBuilder wbuilder;
    wbuilder["indentation"] = "\t";

    std::string result = Json::writeString(wbuilder, root);

    return result;
}

#ifdef PLATFORM_PI
inline bool isPi5() {
    return startsWith(GetFileContents("/proc/device-tree/model"), "Raspberry Pi 5") || startsWith(GetFileContents("/proc/device-tree/model"), "Raspberry Pi Compute Module 5");
}
#endif

static void modprobe(const char* mod) {
    std::string cmd = std::string("/usr/sbin/modprobe ") + mod;
    exec(cmd);
}

static void DetectCape() {
    if (!FileExists("/.dockerenv")) {
#ifdef CAPEDETECT
        int count = 0;
#ifdef PLATFORM_PI
        modprobe("i2c-dev");
#endif
        while (!FileExists(I2C_DEV) && count < 500) {
            printf("FPP - Waiting for %s to appear for Cape/Hat detection %d\n", I2C_DEV.c_str(), count);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            count++;
        }
        printf("FPP - Checking for Cape/Hat\n");
        exec("/opt/fpp/src/fppcapedetect -no-set-permissions");
#endif
    }
    PutFileContents(FPP_MEDIA_DIR + "/tmp/cape_detect_done", "1");
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
    printf("      - Regenerating SSH keys\n");
    if (FileExists("/dev/hwrng")) {
        // if the hwrng exists, use it to seed the urandom generator
        // with some random data
        exec("dd if=/dev/hwrng of=/dev/urandom count=1 bs=4096 status=none");
    }
    execbg("/usr/bin/ssh-keygen -q -N \"\" -t ecdsa -f /etc/ssh/ssh_host_ecdsa_key &");
    execbg("/usr/bin/ssh-keygen -q -N \"\" -t ed25519 -f /etc/ssh/ssh_host_ed25519_key &");
    execbg("/usr/bin/ssh-keygen -q -N \"\" -t rsa -b 2048 -f /etc/ssh/ssh_host_rsa_key &");
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
            exec(cmd);
            PutFileContents(bootDir + "/fpp/copy_done", "1");
        }
    }
}

static void checkHostName() {
    std::string hn;
    if (FileExists("/.dockerenv")) {
        std::string CID = execAndReturn("/usr/bin/head -1 /proc/1/cgroup | sed -e \"s/.*docker-//\" | cut -c1-12");
        hn = execAndReturn("/usr/bin/hostname");
        TrimWhiteSpace(hn);
        TrimWhiteSpace(CID);
        setRawSetting("HostName", hn);
        setRawSetting("HostDescription", "Docker ID: " + CID);
    } else {
        getRawSetting("HostName", hn);
        // By default on installation, hostname is fpp, but it does not appear in the settings file
        if (hn == "") {
            hn = "fpp";
        }
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
            execbg("/usr/bin/systemctl restart avahi-daemon &");
        }
    }
}

static void runScripts(const std::string tp, bool userBefore = true) {
    std::string pfx = "FPPDIR=/opt/fpp SRCDIR=/opt/fpp/src ";

    if (userBefore && FileExists(FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh")) {
        execbg(pfx + FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh " + tp);
    }
    for (const auto& entry : std::filesystem::directory_iterator(FPP_MEDIA_DIR + "/plugins")) {
        if (entry.is_directory()) {
            std::string filename = entry.path().filename();
            std::string cmd = FPP_MEDIA_DIR + "/plugins/" + filename + "/scripts/" + tp + ".sh";
            if (FileExists(cmd)) {
                execbg(pfx + cmd);
            }
        }
    }
    if (!userBefore && FileExists(FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh")) {
        execbg(pfx + FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh " + tp);
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
                                             "backups", "tmp", "virtualdisplay_assets" };
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
    static const std::string UIPASSCONF = FPP_MEDIA_DIR + "/config/ui-password-config.conf";
    static const std::string HTPWD = FPP_MEDIA_DIR + "/config/.htpasswd";
    if (!FileExists(UIPASSCONF)) {
        std::string content = "<RequireAny>\n  Require all granted\n</RequireAny>\n\nSetEnvIf Host ^ LOCAL_PROTECT=0\n";
        PutFileContents(UIPASSCONF, content);
    } /* else {
        std::string content = GetFileContents(UIPASSCONF);
        if (content.find("php_value") != std::string::npos) {
            printf("    Modifying .htaccess file\n");
            std::vector<std::string> lines = split(content, '\n');
            content = "";
            for (auto& line : lines) {
                if (line.find("php_value") == std::string::npos) {
                    content += line;
                }
                content += "\n";
            }
            PutFileContents(UIPASSCONF, content);
        }
    } */
    if (!FileExists(HTPWD)) {
        PutFileContents(HTPWD, "");
    }
    /*     if (!FileExists("/opt/fpp/www/proxy/.htaccess")) {
            printf("Creating proxy .htaccess link\n");
            if (!FileExists("/home/fpp/media/config/proxies")) {
                PutFileContents("/home/fpp/media/config/proxies", "");
            }
            symlink("/home/fpp/media/config/proxies", "/opt/fpp/www/proxy/.htaccess");
        } */
}

void handleBootActions() {
    std::string v;
    if (getRawSetting("BootActions", v) && !v.empty()) {
        exec("/opt/fpp/scripts/handle_boot_actions");
    }
}

inline const std::string& mapBBBLedValue(const std::string& v) {
#ifdef PLATFORM_BB64
    if (v == "cpu") {
        static const std::string activity = "activity";
        return activity;
    }
#endif
    return v;
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
        if (uboot != "U-Boot 2022.04-gc6f4cf7") {
            exec("/opt/fpp/bin.bbb/bootloader/install.sh");
            setRawSetting("rebootFlag", "1");
        }
    } else {
        // its a pocketbeagle
        // bug in kernel/device tree on pcoketbeagle where in P2-36 is not
        // properly set to be AIN so sensors won't read properly
        exec("/usr/sbin/i2cset -y -f 0 0x24 9 5");
    }

    // Beagle LEDS
    std::string pled;
    if (getRawSetting("BBBLedPWR", pled) && !pled.empty()) {
        if (pled == "0") {
            pled = "0x00";
        } else {
            pled = "0x38";
        }
        exec("/usr/sbin/i2cset -f -y 0 0x24 0x0b 0x6e");
        exec("/usr/sbin/i2cset -f -y 0 0x24 0x13 " + pled);
        exec("/usr/sbin/i2cset -f -y 0 0x24 0x0b 0x6e");
        exec("/usr/sbin/i2cset -f -y 0 0x24 0x13 " + pled);
    }
#endif
#if defined(PLATFORM_BBB) || defined(PLATFORM_BB64)
    std::string led;
    int offset = FileExists("/sys/class/leds/beaglebone:green:usr0/trigger") ? 0 : 1;
    if (getRawSetting("BBBLeds0", led) && !led.empty()) {
        PutFileContents("/sys/class/leds/beaglebone:green:usr" + std::to_string(offset) + "/trigger", mapBBBLedValue(led));
    } else {
        PutFileContents("/sys/class/leds/beaglebone:green:usr" + std::to_string(offset) + "/trigger", "heartbeat");
    }
    if (getRawSetting("BBBLeds1", led) && !led.empty()) {
        PutFileContents("/sys/class/leds/beaglebone:green:usr" + std::to_string(1 + offset) + "/trigger", mapBBBLedValue(led));
    } else {
        PutFileContents("/sys/class/leds/beaglebone:green:usr" + std::to_string(1 + offset) + "/trigger", "mmc0");
    }
    if (getRawSetting("BBBLeds2", led) && !led.empty()) {
        PutFileContents("/sys/class/leds/beaglebone:green:usr" + std::to_string(2 + offset) + "/trigger", mapBBBLedValue(led));
    } else {
        PutFileContents("/sys/class/leds/beaglebone:green:usr" + std::to_string(2 + offset) + "/trigger", mapBBBLedValue("cpu"));
    }
    if (getRawSetting("BBBLeds3", led) && !led.empty()) {
        PutFileContents("/sys/class/leds/beaglebone:green:usr" + std::to_string(3 + offset) + "/trigger", led);
    } else {
        PutFileContents("/sys/class/leds/beaglebone:green:usr" + std::to_string(3 + offset) + "/trigger", "mmc1");
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
                std::string output = execAndReturn("/usr/sbin/iw " + dev + " link");
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

static void setupNetwork(bool fullReload = false) {
    unblockWifi();
    auto dnsSettings = loadSettingsFile(FPP_MEDIA_DIR + "/config/dns");
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
                    if (!contains(execAndReturn("/usr/bin/systemctl is-active wpa_supplicant@" + interface), "inactive")) {
                        commandsToRun.emplace_back("systemctl reload-or-restart \"wpa_supplicant@" + interface + ".service\" &");
                    }
                    if (!contains(execAndReturn("/usr/bin/systemctl is-enabled wpa_supplicant@" + interface), "enabled")) {
                        commandsToRun.emplace_back("systemctl enable \"wpa_supplicant@" + interface + ".service\" &");
                        commandsToRun.emplace_back("systemctl daemon-reload");
                    }
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
            content.append("IgnoreCarrierLoss=5s\n");
            content.append("\n");

            if (!interfaceSettings["GATEWAY"].empty()) {
                content.append("[Route]\nGateway=").append(interfaceSettings["GATEWAY"]).append("\n");
                if (ROUTEMETRIC != 0) {
                    content.append("Metric=").append(interfaceSettings["ROUTEMETRIC"]).append("\n");
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
    bool reloadApache = false;
    if (dhcpProxies.empty() && FileExists(dhcpProxyFile)) {
        unlink(dhcpProxyFile.c_str());
        reloadApache = true;
    } else if (!dhcpProxies.empty()) {
        PutFileContents(dhcpProxyFile, dhcpProxies);
        reloadApache = true;
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
static void setFileOwnership() {
    exec("/usr/bin/chown -R fpp:fpp " + FPP_MEDIA_DIR);
}

static bool checkUnpartitionedSpace() {
    bool ret = false;
    if (!FileExists("/etc/fpp/desktop")) {
        std::string sourceDev = execAndReturn("/usr/bin/findmnt -n -o SOURCE " + FPP_MEDIA_DIR);
        TrimWhiteSpace(sourceDev);
        if (sourceDev.empty()) {
            sourceDev = execAndReturn("/usr/bin/findmnt -n -o SOURCE /");
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
        if (FileExists(SD_CARD_DEVICE)) {
            fs = execAndReturn("/usr/sbin/sfdisk -F " + SD_CARD_DEVICE + " | tail -n 1");
            TrimWhiteSpace(fs);
            auto splits = split(fs, ' ');
            fs = splits.back();
            if (endsWith(fs, "G")) {
                fs = fs.substr(0, fs.size() - 1);
            } else {
                fs = "0";
            }
        }
        if (FileExists("/boot/firmware/fpp_expand_rootfs") || FileExists("/boot/fpp_expand_rootfs")) {
            fs = "0";
            std::string rootPart = execAndReturn("/usr/bin/findmnt -n -o SOURCE /");
            TrimWhiteSpace(rootPart);
            if (startsWith(rootPart, SD_CARD_DEVICE)) {
                std::string lastPartNum = execAndReturn("/usr/sbin/parted " + SD_CARD_DEVICE + " -ms unit s p | tail -n 1 | cut -f 1 -d:");
                TrimWhiteSpace(lastPartNum);
                std::string startPos = execAndReturn("/usr/sbin/parted " + SD_CARD_DEVICE + " -ms unit s p | grep \"^" + lastPartNum + "\" | cut -f 2 -d: | sed 's/[^0-9]//g'");
                TrimWhiteSpace(startPos);
                std::string fdiskInstructions = "p\nd\n" + lastPartNum + "\nn\np\n" + lastPartNum + "\n" + startPos + "\n\np\nw\n";
                PutFileContents("/tmp/fdisk.cmds", fdiskInstructions);
                exec("/usr/sbin/fdisk " + SD_CARD_DEVICE + " < /tmp/fdisk.cmds");
                unlink("/tmp/fdisk.cmds");
                exec("systemctl enable fpp-expand-rootfs.service");
                setRawSetting("rebootFlag", "1");
                ret = true;
            }
            unlink("/boot/firmware/fpp_expand_rootfs");
            unlink("/boot/fpp_expand_rootfs");
        }
        std::string oldfs;
        getRawSetting("UnpartitionedSpace", oldfs);
        if (oldfs != fs) {
            setRawSetting("UnpartitionedSpace", fs);
        }
    }
    return ret;
}
static void resizeRootFS() {
    std::string rootPart = execAndReturn("/usr/bin/findmnt -n -o SOURCE /");
    TrimWhiteSpace(rootPart);
    exec("/usr/sbin/resize2fs " + rootPart);
    exec("systemctl disable fpp-expand-rootfs.service");
}
static void setupTimezone() {
    std::string s;
    getRawSetting("TimeZone", s);
    TrimWhiteSpace(s);
    if (!s.empty()) {
        std::string c = GetFileContents("/etc/timezone");
        TrimWhiteSpace(c);
        if (c != s) {
            printf("Resetting timezone from %s to %s\n", c.c_str(), s.c_str());
            exec("/usr/bin/timedatectl set-timezone " + s);
            execbg("/usr/sbin/dpkg-reconfigure -f noninteractive tzdata &");
        }
    }
}

static void handleBootDelay() {
    int i = getRawSettingInt("bootDelay", -1);
    if (i > 0) {
        printf("FPP - Sleeping for %d seconds\n", i);
        std::this_thread::sleep_for(std::chrono::seconds(i));
    } else if (i == -1) {
        const auto processor_count = std::thread::hardware_concurrency();
        if (processor_count > 2) {
            // super fast Pi, we need a minimal delay for devices to be found
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }

        struct stat attr;
        stat("/etc/fpp/rfs_version", &attr);
        struct tm tmFile, tmNow;
        localtime_r(&(attr.st_ctime), &tmFile);
        time_t t = time(nullptr);
        localtime_r(&(attr.st_ctime), &tmNow);

        time_t t1 = mktime(&tmFile);
        time_t t2 = mktime(&tmNow);
        double diffSecs = difftime(t1, t2);
        if (diffSecs > 0) {
            char buffer[26];
            strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", &tmFile);
            printf("FPP - FPP - Waiting until system date is at least %s or 15s\n", buffer);
        }

        int count = 0;
        while (diffSecs > 0 && count < 150) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            t = time(nullptr);
            localtime_r(&(attr.st_ctime), &tmNow);
            t2 = mktime(&tmNow);
            diffSecs = difftime(t1, t2);
            count++;
        }
    }
}
void cleanupChromiumFiles() {
    exec("/usr/bin/rm -rf /home/fpp/.config/chromium/Singleton* 2>/dev/null > /dev/null");
}

static void checkWLANInterface() {
    if (contains(execAndReturn("/usr/bin/systemctl is-enabled wpa_supplicant@wlan0"), "enabled") && contains(execAndReturn("/usr/bin/systemctl is-active wpa_supplicant@wlan0"), "inactive")) {
        printf("Need to restart wpa_supplicant@wlan0\n");
        execbg("systemctl restart \"wpa_supplicant@wlan0.service\" &");
    }
}

static bool waitForInterfacesUp(bool flite, int timeOut) {
    bool found = false;
    int count = 0;
    std::string announce;
    do {
        announce = "I Have Found The Following I P Addresses";
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
        if (!found) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            ++count;
        }
    } while (!found && (count < timeOut));
    if (!found) {
        printf("FPP - Could not get a valid IP address\n");
        return false;
    } else {
        printf("FPP - Waited for %d seconds for IP address\n", (count / 5));
        if (!getRawSettingInt("disableIPAnnouncement", 0) && FileExists("/usr/bin/flite") && flite) {
            execbg("/usr/bin/flite -voice awb -t \"" + announce + "\" &");
        }
        return true;
    }
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

static void maybeEnableTethering() {
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
            // did not find an ip address
            te = 1;
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
    }
}
static void detectNetworkModules() {
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
static void checkPi5Wifi() {
#ifdef PLATFORM_PI
    if (isPi5()) {
        // Pi5 does not have external wifi adapters, make sure we have them disabled
        if (FileExists("/etc/modprobe.d/blacklist-native-wifi.conf")) {
            unlink("/etc/modprobe.d/blacklist-native-wifi.conf");
        }
        std::string v;
        getRawSetting("wifiDrivers", v);
        if (v != "Kernel") {
            setRawSetting("wifiDrivers", "Kernel");
        }
    }
#endif
}
void removeDummyInterface() {
    exec("ip link delete dummy0 > /dev/null 2>&1");
}

static void setupAudio() {
    if (!FileExists("/root/.libao")) {
        PutFileContents("/root/.libao", "dev=default");
    }
    std::string aplay = execAndReturn("/usr/bin/aplay -l 2>&1");
    std::vector<std::string> lines = split(aplay, '\n');
    std::map<std::string, std::string> cards;
    std::map<std::string, bool> hdmiStatus;
    bool hasNonHDMI = false;
    for (auto& l : lines) {
        if (l.starts_with("card ")) {
            std::string k = l.substr(0, 6);
            std::string v = l.substr(8);
            int idx = v.find(' ');
            v = v.substr(0, idx);
            cards[k] = v;
            hasNonHDMI |= !l.contains("vc4hdmi");
        }
    }
    int hdmiIdx = 0;
    for (int x = 0; x < 4; x++) {
        std::string cstr = "/sys/class/drm/card" + std::to_string(x) + "-HDMI-A-1/status";
        if (FileExists(cstr)) {
            for (int p = 1; p < 5; p++) {
                std::string cstr = "/sys/class/drm/card" + std::to_string(x) + "-HDMI-A-" + std::to_string(p) + "/status";
                std::string c = GetFileContents(cstr);
                std::string k = "vc4hdmi" + std::to_string(hdmiIdx);
                hdmiStatus[k] = c.contains("connected") && !c.contains("disconnected");
                hdmiIdx++;
            }
        }
    }
    if (!hasNonHDMI || contains(aplay, "no soundcards")) {
        printf("FPP - No Soundcard Detected, loading snd-dummy\n");
        modprobe("snd-dummy");
    }
    int card = getRawSettingInt("AudioOutput", 0);
    std::string cstr = "card " + std::to_string(card);
    bool found = false;
    int count = 0;

    if (cards[cstr].starts_with("vc4hdmi") && !hdmiStatus[cards[cstr]]) {
        // nothing connected to the HDMI port so it's definitely not going to work
        // flip to the dummy output
        if (!cards.empty() && !cards["card 0"].starts_with("vc4hdmi")) {
            printf("FPP - Could not find audio device %d, attempting device 0.\n", card);
            card = 0;
        } else {
            card = cards.size();
            found = true;
            setRawSetting("AudioOutput", std::to_string(card));
        }
    }
    while (!found && count < 50) {
        std::string amixer = execAndReturn("/usr/bin/amixer -c " + std::to_string(card) + " cset numid=3 1  2>&1");
        if (contains(amixer, "Invalid ")) {
            ++count;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
            found = true;
        }
    }
    if (!found) {
        printf("FPP - Could not find audio device %d, defaulting to device 0.\n", card);
        CopyFileContents("/opt/fpp/etc/asoundrc.plain", "/root/.asoundrc");
        setRawSetting("AudioOutput", "0");
    } else {
        printf("FPP - Waited for %d seconds for audio device\n", (count / 5));
    }
    int v = getRawSettingInt("volume", -1);
    if (v == -1) {
        setRawSetting("volume", "70");
        v = 70;
    }
    std::string origAudio0CardType;
    getRawSetting("AudioCard0Type", origAudio0CardType);
    std::string audioCardType = "unknown";
    std::string aplayl = execAndReturn("/usr/bin/aplay -l | grep 'card " + std::to_string(card) + "'");
#ifdef PLATFORM_PI
    // Pi needs a volume adjustment, in reality a lot of sound cards do, but we
    // don't want to put in a lot of special cases here so only handle the Pi
    if (card == 0) {
        if (contains(aplayl, "[bcm2")) {
            v = (v / 2) + 50;
            audioCardType = "bcm2";
        }
    }
#endif
    if (audioCardType != origAudio0CardType) {
        setRawSetting("AudioCard0Type", audioCardType);
    }
    std::string cardType = aplayl.substr(aplayl.find("[") + 1);
    cardType = cardType.substr(0, cardType.find("]"));
    printf("FPP - Found sound card of type %s\n", cardType.c_str());
    std::string asoundrc;
    if (FileExists("/home/fpp/media/tmp/asoundrc")) {
        if (contains(cardType, "pcm510")) {
            printf("FPP - Using Cape Provided asoundrc\n");
            asoundrc = GetFileContents("/home/fpp/media/tmp/asoundrc");
        } else {
            asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.dmix");
        }
    }
    if (asoundrc.empty()) {
        if (contains(cardType, "vc4-hdmi")) {
            replaceAll(cardType, "-", "");
            asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.hdmi");
        } else if (contains(cardType, "bcm2")) {
            asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.plain");
        } else {
            asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.dmix");
        }
    }
    int bufSize = getRawSettingInt("AudioBufferSize", 3072);
    int perSize = getRawSettingInt("AudioPeriodSize", 1024);
    replaceAll(asoundrc, "CARDTYPE", cardType);
    replaceAll(asoundrc, "BUFFERSIZE", std::to_string(bufSize));
    replaceAll(asoundrc, "PERIODSIZE", std::to_string(perSize));
    for (int x = 0; x < 10; x++) {
        if (x != card) {
            replaceAll(asoundrc, "card " + std::to_string(x), "card " + std::to_string(card));
        }
    }
    int rate = getRawSettingInt("AudioFormat", 0);
    switch (rate) {
    case 1:
    case 2:
    case 3:
        // replaceAll(asoundrc, "rate 44100", "rate 44100");
        break;
    case 4:
    case 5:
    case 6:
        replaceAll(asoundrc, "rate 44100", "rate 48000");
        break;
    case 7:
    case 8:
    case 9:
        replaceAll(asoundrc, "rate 44100", "rate 96000");
        break;
    default:
        break;
    }
    PutFileContents("/root/.asoundrc", asoundrc);
    std::string mixers = execAndReturn("/usr/bin/amixer -c " + std::to_string(card) + " scontrols | cut -f2 -d\"'\"");
    if (mixers.empty()) {
        // for some sound cards, the mixer devices won't show up
        // until something is played.  Play one second of silence
        exec("/usr/bin/aplay -d 1 /opt/fpp/media/silence_5sec.wav >> /dev/null 2>&1  &");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        mixers = execAndReturn("/usr/bin/amixer -c " + std::to_string(card) + " scontrols | cut -f2 -d\"'\"");
    }
    TrimWhiteSpace(mixers);
    std::string mixer;
    getRawSetting("AudioMixerDevice", mixer);
    std::string origMixer = mixer;
    if (mixer.empty() || !contains(mixers, mixer)) {
        mixer = mixers;
        if (mixer.find("\n") != std::string::npos) {
            mixer = mixer.substr(0, mixer.find("\n"));
        }
    }
    if (mixer != origMixer) {
        printf("FPP - Setting mixer device to %s\n", mixer.c_str());
        setRawSetting("AudioMixerDevice", mixer);
    }
    exec("/usr/bin/amixer -c " + std::to_string(card) + " set " + mixer + " " + std::to_string(v) + "% > /dev/null 2>&1");
    setRawSetting("AudioCardType", cardType);
}
void detectFalconHardware() {
#ifdef PLATFORM_PI
    printf("FPP - Checking for Falcon hardware on SPI port\n");
    int fnd = system("/opt/fpp/src/fppd -H");
    setRawSetting("FalconHardwareDetected", fnd ? "1" : "0");
#endif
}

void setupKiosk(bool force = false) {
    int km = getRawSettingInt("Kiosk", 0);
    if (km || force) {
        std::string url = "http://localhost/";
        getRawSetting("KioskUrl", url);

        // FPP UI doesn't delete the setting, it makes it ""
        if (url == "\"\"") {
            url = "http://localhost/";
        }

        std::string value = "{\"RestoreOnStartup\": 4,\"RestoreOnStartupURLs\": [\"" + url + "\"]}";
        mkdir("/etc/chromium/", 0775);
        mkdir("/etc/chromium/policies", 0775);
        mkdir("/etc/chromium/policies/managed", 0775);
        PutFileContents("/etc/chromium/policies/managed/policy.json", value);
    }
}
void checkInstallKiosk() {
    int km = getRawSettingInt("Kiosk", 0);
    if (FileExists("/fpp_kiosk")) {
        km = true;
    }

    if (km && !FileExists("/etc/fpp/kiosk")) {
        std::string s = execAndReturn("/usr/bin/systemctl is-enabled fpp-install-kiosk");
        TrimWhiteSpace(s);
        if (s == "disabled") {
            exec("/usr/bin/systemctl enable fpp-install-kiosk");
            exec("/usr/sbin/reboot");
        }
    }
}
void installKiosk() {
    int km = getRawSettingInt("Kiosk", 0);
    if (FileExists("/fpp_kiosk")) {
        unlink("/fpp_kiosk");
        km = true;
    }
    if (km && !FileExists("/etc/fpp/kiosk")) {
        // need to re-install kiosk mode
        exec("/opt/fpp/SD/FPP_Kiosk.sh");
        if (FileExists("/etc/fpp/kiosk")) {
            exec("/usr/sbin/reboot");
        }
        setupKiosk(true);
    }
}

void installPackagesFromJson(const std::string& filePath) {
    std::ifstream file(filePath, std::ifstream::binary);
    if (!file) {
        printf("Error: Could not open %s\n", filePath.c_str());
        return;
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;

    if (!Json::parseFromStream(reader, file, &root, &errs)) {
        printf("Error: Failed to parse JSON - %s\n", errs.c_str());
        return;
    }

    if (!root.isArray()) {
        printf("Error: JSON is not an array\n");
        return;
    }

    for (const auto& item : root) {
        if (item.isString()) {
            printf("Installing: %s\n", item.asString().c_str());
            pid_t pid = fork();
            if (pid == -1) {
                printf("Error: Failed to fork process\n");
                return;
            } else if (pid == 0) {
                execlp("apt-get", "apt-get", "install", "-y", item.asString().c_str(), nullptr);
                // If execlp fails
                printf("Error: Failed to execute apt-get for %s\n", item.asString().c_str());
                return;
            } else {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                    printf("Warning: Package installation failed for %s\n", item.asString().c_str());
                }
            }
        }
    }
}

void checkInstallPackages() {
    if (FileExists("/fppos_upgraded")) {
        unlink("/fppos_upgraded");
        printf("Installing User Packages\n");
        installPackagesFromJson("/home/fpp/media/config/userpackages.json");
    }
}
void startZRAMSwap() {
    if (!FileExists("/dev/zram0") && FileExists("/usr/sbin/zramswap")) {
        execbg("/usr/sbin/zramswap start 2>/dev/null > /dev/null &");
    }
}

static void setupChannelOutputs() {
#ifdef PLATFORM_PI
    bool hasRPI = false;
    bool hasDPI = false;
    bool hasRPIMatrix = false;
    Json::Value v;
    if (FileExists("/home/fpp/media/config/channeloutputs.json")) {
        if (LoadJsonFromString(GetFileContents("/home/fpp/media/config/channeloutputs.json"), v)) {
            for (int x = 0; x < v["channelOutputs"].size(); x++) {
                if (v["channelOutputs"][x]["subType"].asString() == "RGBMatrix" && v["channelOutputs"][x]["enabled"].asInt() == 1) {
                    hasRPIMatrix = true;
                }
            }
        }
    }
    if (FileExists("/home/fpp/media/config/co-pixelStrings.json")) {
        if (LoadJsonFromString(GetFileContents("/home/fpp/media/config/co-pixelStrings.json"), v)) {
            for (int x = 0; x < v["channelOutputs"].size(); x++) {
                if (v["channelOutputs"][x]["type"].asString() == "RPIWS281X" && v["channelOutputs"][x]["enabled"].asInt() == 1) {
                    hasRPI = true;
                    if (isPi5() && v["channelOutputs"][x]["subType"].asString() == "PiHat") {
                        // Pi5 does not support RPIWS281X.  If using standard PiHat, then flip to DPIPixels,
                        v["channelOutputs"][x]["type"] = "DPIPixels";
                        v["channelOutputs"][x]["subType"] = "PiHat-DPIPixels";
                        std::string newPixels = SaveJsonToString(v);
                        bool b = PutFileContents("/home/fpp/media/config/co-pixelStrings.json", newPixels);
                        hasRPI = false;
                        hasDPI = true;
                    }
                }
                if (v["channelOutputs"][x]["type"].asString() == "DPIPixels" && v["channelOutputs"][x]["enabled"].asInt() == 1) {
                    hasDPI = true;
                }
            }
        }
    }
    bool changed = false;
    if (hasRPI || hasRPIMatrix) {
        if (!FileExists("/etc/modprobe.d/blacklist-bcm2835.conf")) {
            PutFileContents("/etc/modprobe.d/blacklist-bcm2835.conf", "blacklist snd_bcm2835");
            changed = true;
        }
        // Preemptively load the USB sound driver so it will detect before snd-dummy is created
        // and get the default card0 slot
        exec("modprobe snd_usb_audio");

        // load the dummy sound driver so things that expect a sound device to be there will still work
        exec("modprobe snd-dummy");
        exec("modprobe snd-seq-dummy");
    } else {
        if (FileExists("/etc/modprobe.d/blacklist-bcm2835.conf")) {
            unlink("/etc/modprobe.d/blacklist-bcm2835.conf");
        }
        exec("modprobe snd-bcm2835");
        exec("modprobe snd_usb_audio");
    }
    std::string content = GetFileContents("/boot/firmware/config.txt");
    size_t idx = content.find("dtoverlay=vc4-kms-dpi-fpp-");
    std::string origLine = "";
    if (idx != std::string::npos) {
        size_t idx2 = content.find("\n", idx);
        origLine = content.substr(idx, idx2 - idx);
    }
    if (hasDPI) {
        int fps = getRawSettingInt("DPI_FPS", 40);
        std::string model = GetFileContents("/sys/firmware/devicetree/base/model");
        std::string type = "pi3";
        std::string width = "362";
        std::string height = fps == 40 ? "162" : "324";
        if (contains(model, "Pi 4") || contains(model, "Pi 5")) {
            type = "pi4";
            width = "1084";
        }
        std::string line = "dtoverlay=vc4-kms-dpi-fpp-" + type + ",rgb888,hactive=" + width + ",hfp=0,hsync=1,hbp=0,vactive=" + height + ",vfp=1,vsync=1,vbp=1";
        if (line != origLine) {
            if (origLine != "") {
                content.replace(idx, origLine.size(), line);
            } else {
                content.append(line).append("\n");
            }
            PutFileContents("/boot/firmware/config.txt", content);
            changed = true;
        }
    } else if (origLine != "") {
        size_t idx2 = content.find("\n", idx);
        content.erase(idx, idx2 - idx);
        PutFileContents("/boot/firmware/config.txt", content);
        changed = true;
    }
    if (changed) {
        printf("\n\nRebooting to load new settings.\n\n");
        exec("/usr/sbin/reboot");
    }
#endif
}

static void handleRebootActions() {
    if (FileExists("/home/fpp/media/tmp/cape-info.json")) {
        Json::Value v;
        if (LoadJsonFromString(GetFileContents("/home/fpp/media/tmp/cape-info.json"), v)) {
            if (v.isMember("rebootActions")) {
                for (const auto& action : v["rebootActions"]) {
                    if (action["type"].asString() == "gpio") {
                        std::string pin = action["pin"].asString();
                        int val = 0;
                        if (pin[0] == '+') {
                            pin = pin.substr(1);
                            val = 1;
                        }
                        std::string ret = execAndReturn("gpioinfo | grep -e gpioch -e " + pin);
                        auto lines = split(ret, '\n');
                        int curChip = 0;
                        for (const auto& line : lines) {
                            if (line.find("gpiochip") != std::string::npos) {
                                curChip = atoi(line.substr(line.find("gpiochip") + 8).c_str());
                            } else if (line.find("\"" + pin + "\"") != std::string::npos) {
                                std::string l = line.substr(line.find("line ") + 5);
                                TrimWhiteSpace(l);
                                int lineNum = atoi(l.c_str());
                                std::string cmd = "gpioset " + std::to_string(curChip) + " " + std::to_string(lineNum) + "=" + std::to_string(val);
                                printf("FPP - Toggling GPIO via: %s\n", cmd.c_str());
                                exec(cmd);
                            }
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::string networkSetupMut = FPP_MEDIA_DIR + "/tmp/networkSetup";
    std::string action = "start";
    if (argc > 1) {
        action = argv[1];
    }
    FILE* f = fopen((FPP_MEDIA_DIR + "/logs/fpp_init.log").c_str(), "a+");
    int sleepCount = 0;
    while (!f && sleepCount < 30) {
        printf("Could not open log\n");
        sleepCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        f = fopen((FPP_MEDIA_DIR + "/logs/fpp_init.log").c_str(), "a+");
    }
    if (f) {
        fclose(f);
        if (action == "start") {
            teeOutput(FPP_MEDIA_DIR + "/logs/fpp_init.log");
        } else if (action == "postNetwork") {
            teeOutput(FPP_MEDIA_DIR + "/logs/fpp_boot.log");
        }
    } else {
        printf("Could not too output to log.  Directory might not be writable or maybe full.\n");
    }
    printf("------------------------------\nRunning FPPINIT %s", action.c_str());
    for (int x = 2; x < argc; x++) {
        printf(" %s", argv[x]);
    }
    printf("\n");
    if (action == "start") {
        if (!FileExists("/.dockerenv")) {
#ifdef CAPEDETECT
            printf("FPP - Cleaning tmp\n");
            remove_recursive("/home/fpp/media/tmp/", false);
#endif
        }
        createDirectories();
        printf("FPP - Directories created\n");
        bool needReboot = checkUnpartitionedSpace();
        checkSSHKeys();
        handleBootPartition();
        checkPi5Wifi();
        checkHostName();
        checkFSTAB();
        setupApache();
        DetectCape();
        setupTimezone();
        int reboot = getRawSettingInt("rebootFlag", 0);
        if (reboot && !needReboot) {
            printf("FPP - Clearing reboot flags\n");
            setRawSetting("rebootFlag", "0");
        }
        handleBootActions();
        runScripts("boot", true);
        int skipNetwork = FileExists("/etc/fpp/desktop") || getRawSettingInt("SkipNetworkReset", 0);
        if (!skipNetwork) {
            setupNetwork();
        }
        configureBBB();
        setupChannelOutputs();
        setupKiosk();
        printf("Setting file ownership\n");
        setFileOwnership();
        PutFileContents(FPP_MEDIA_DIR + "/tmp/cape_detect_done", "1");
        checkInstallKiosk();
    } else if (action == "postNetwork") {
        removeDummyInterface();
        handleBootDelay();
        checkWLANInterface();
        // turn off blinking cursor
        PutFileContents("/sys/class/graphics/fbcon/cursor_blink", "0");
        cleanupChromiumFiles();
        setupAudio();
        removeDummyInterface();
        waitForInterfacesUp(true, 100); // call to flite requires audio, so do audio before this
        if (!FileExists("/etc/fpp/desktop")) {
            maybeEnableTethering();
            detectNetworkModules();
        }
        setupTimezone(); // this may not have worked in the init phase, try again
        detectFalconHardware();
        setFileOwnership();
        checkInstallPackages();
        startZRAMSwap();
    } else if (action == "bootPre") {
        int restart = getRawSettingInt("restartFlag", 0);
        if (restart) {
            printf("FPP - Clearing restart flags\n");
            setRawSetting("restartFlag", "0");
        }
        setupChannelOutputs();
        runScripts("preStart", true);
    } else if (action == "bootPost") {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        runScripts("postStart", false);
    } else if (action == "runPreStartScripts") {
        runScripts("preStart", true);
    } else if (action == "runPostStartScripts") {
        runScripts("postStart", false);
    } else if (action == "runPreStopScripts") {
        runScripts("preStop", true);
    } else if (action == "runPostStopScripts") {
        runScripts("postStop", false);
    } else if (action == "setupAudio") {
        setupAudio();
    } else if (action == "configureBBB") {
        configureBBB();
    } else if (action == "installKiosk") {
        installKiosk();
    } else if (action == "setupNetwork") {
        PutFileContents(networkSetupMut, "1");
        setupNetwork(true);
        waitForInterfacesUp(false, 70);
        detectNetworkModules();
        maybeEnableTethering();
        unlink(networkSetupMut.c_str());
    } else if (action == "checkForTether") {
        if (!FileExists(networkSetupMut)) {
            std::string a = execAndReturn("systemctl is-active fpp_postnetwork");
            TrimWhiteSpace(a);
            if (a.starts_with("active") && argc >= 3) {
                std::string iface = argv[2];
                if (iface.starts_with("wlan")) {
                    waitForInterfacesUp(false, 20);
                    maybeEnableTethering();
                    detectNetworkModules();
                }
            }
        }
    } else if (action == "maybeRemoveTether") {
        int te = getRawSettingInt("EnableTethering", 0);
        if (!FileExists(networkSetupMut) && (te != 1)) {
            std::string e = execAndReturn("systemctl is-enabled hostapd");
            std::string a = execAndReturn("systemctl is-active hostapd");
            TrimWhiteSpace(e);
            TrimWhiteSpace(a);
            if (a.starts_with("active") || e.starts_with("enabled")) {
                std::string iface = argv[2];
                if (FindTetherWIFIAdapater() != iface && !iface.starts_with("usb") && !iface.starts_with("lo") && waitForInterfacesUp(false, 10)) {
                    exec("rm -f /etc/systemd/network/10-" + FindTetherWIFIAdapater() + ".network");
                    exec("rm -f /home/fpp/media/tmp/wifi-*.ascii");
                    exec("systemctl stop hostapd.service");
                    exec("systemctl disable hostapd.service");
                    exec("systemctl reload-or-restart systemd-networkd.service");
                }
            }
        }
    } else if (action == "detectNetworkModules") {
        detectNetworkModules();
    } else if (action == "reboot") {
        handleRebootActions();
    } else if (action == "resizeRootFS") {
        resizeRootFS();
    }
    printf("------------------------------\n");
    return 0;
}
