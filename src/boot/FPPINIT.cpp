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
#include <algorithm>
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
#include <algorithm>
#include <regex>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <cctype>

#include "common_mini.h"
#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <systemd/sd-daemon.h>
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
    struct PipeCloser {
        void operator()(FILE* f) const {
            if (f)
                pclose(f);
        }
    };
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
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
    struct PipeCloser {
        void operator()(FILE* f) const {
            if (f)
                pclose(f);
        }
    };
    std::unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
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
inline bool isPiZero2W() {
    return contains(GetFileContents("/proc/device-tree/model"), "Raspberry Pi Zero 2 W");
}
#endif

static void modprobe(const char* mod) {
    std::string cmd = std::string("/usr/sbin/modprobe ") + mod;
    exec(cmd);
}

static void DetectCape() {
    if (!FileExists("/etc/fpp/container")) {
#ifdef CAPEDETECT
        int count = 0;
#ifdef PLATFORM_PI
        modprobe("i2c-dev");
#endif
        if (!FileExists(I2C_DEV)) {
            printf("FPP - Waiting up to 3s for %s to appear for Cape/Hat detection\n", I2C_DEV.c_str());
        }
        while (!FileExists(I2C_DEV) && count < 600) {
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
    int keyCount = 0;
    int pubCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator("/etc/ssh")) {
        if (entry.is_regular_file()) {
            const auto filename = entry.path().filename();
            if (contains(filename, "ssh_host")) {
                if (filename.extension() == ".pub") {
                    pubCount++;
                } else {
                    keyCount++;
                }
            }
        }
    }
    // make sure we have at least two private and public keys (really should be three, but some platforms don't support ed25519, so we'll just check for two)
    if (keyCount >= 2 && pubCount >= 2) {
        return;
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
    if (FileExists("/etc/fpp/container")) {
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
        if (uboot != "U-Boot 2022.04-g5509547") {
            printf("Installing new bootloader\n");
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
    // --no-reload is important: a bare "systemctl disable" performs an implicit
    // daemon-reload, which on a single-core SBC (e.g. BeagleBone) takes ~11s and
    // freezes systemd's entire job queue mid-boot -- stalling dbus, networkd, and
    // fppinit. The removed symlink takes effect next boot regardless, so there's
    // no reason to reload now.
    exec("systemctl disable --no-reload fpp-expand-rootfs.service");
}
static void setupTimezone() {
    std::string s;
    getRawSetting("TimeZone", s);
    TrimWhiteSpace(s);
    if (!s.empty()) {
        // Debian 13 (trixie) no longer ships /etc/timezone -- the timezone is
        // tracked solely by the /etc/localtime symlink. Reading the old file
        // therefore always returned empty, so this used to run the expensive
        // `timedatectl set-timezone` (a dbus round-trip to systemd-timedated
        // that blocks ~15s on a single-core SBC) plus a backgrounded
        // `dpkg-reconfigure tzdata` on EVERY boot, and twice (fppinit start +
        // postNetwork). Derive the current zone by reading the /etc/localtime
        // symlink target (a single syscall, no fork) and only reconfigure when
        // it actually differs. The target looks like
        // ".../usr/share/zoneinfo/America/New_York"; the zone is everything
        // after "zoneinfo/".
        std::string c;
        std::error_code ec;
        std::filesystem::path tzTarget = std::filesystem::read_symlink("/etc/localtime", ec);
        if (!ec) {
            std::string t = tzTarget.string();
            size_t pos = t.find("zoneinfo/");
            if (pos != std::string::npos) {
                c = t.substr(pos + 9); // strlen("zoneinfo/")
            }
        }
        TrimWhiteSpace(c);
        if (c != s) {
            printf("Resetting timezone from %s to %s\n", c.c_str(), s.c_str());
            exec("/usr/bin/timedatectl set-timezone " + s);
            execbg("/usr/sbin/dpkg-reconfigure -f noninteractive tzdata &");
        }
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

static void handleBootDelay() {
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
static void handleTimeSyncWait() {
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
    bool found = false;
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
        printf("FPP - Waited for %0.1f seconds for IP address\n", (((float)count) * 0.2f));
        if (!getRawSettingInt("disableIPAnnouncement", 0) && FileExists("/usr/bin/flite") && flite) {
            // flite is an ALSA client, so playing directly (flite -t) would route
            // through the pipewire-alsa plugin + /root/.asoundrc -- the fragile
            // ALSA chain we're retiring. Instead render to a WAV and play it
            // through PipeWire natively with pw-play. pw-play needs the runtime
            // env vars to find FPP's PipeWire instance at /run/pipewire-fpp.
            // /run/fppd is created earlier by setupAudio.
            const std::string announceWav = "/run/fppd/ip-announce.wav";
            std::string fliteCmd =
                "/usr/bin/flite -voice awb -t \"" + announce + "\" -o " + announceWav +
                " && PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp XDG_RUNTIME_DIR=/run/pipewire-fpp"
                " /usr/bin/pw-play " + announceWav + " &";
            execbg(fliteCmd);
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
void removeDummyInterface() {
    exec("ip link delete dummy0 > /dev/null 2>&1");
}

// Resolve ALSA card number to stable card ID (e.g., 3 -> "S3", 0 -> "ICUSBAUDIO7D")
// Reads /proc/asound/cards: " 3 [S3             ]: USB-Audio - ..."
// Falls back to the card number as string if not found.
// Read the USB product name for an ALSA card from sysfs (e.g. "USB Sound Device").
// Falls back to the ALSA card type name if sysfs is unavailable.
static std::string getAlsaCardProductName(int cardNum, const std::string& fallback) {
    std::string devicePath = "/sys/class/sound/card" + std::to_string(cardNum) + "/device";
    char resolved[PATH_MAX];
    if (realpath(devicePath.c_str(), resolved)) {
        // The ALSA device sysfs node points to a USB interface.
        // The USB product string is in the parent USB device node.
        std::string parentDir = resolved;
        auto pos = parentDir.rfind('/');
        if (pos != std::string::npos) {
            std::string productFile = parentDir.substr(0, pos) + "/product";
            std::string product = GetFileContents(productFile);
            TrimWhiteSpace(product);
            if (!product.empty()) {
                return product;
            }
        }
    }
    return fallback;
}

static std::string getAlsaCardId(int cardNum) {
    std::string cardsContent = GetFileContents("/proc/asound/cards");
    if (!cardsContent.empty()) {
        std::istringstream iss(cardsContent);
        std::string line;
        while (std::getline(iss, line)) {
            // Match: " 3 [S3             ]: ..."
            auto bracket = line.find('[');
            auto closeBracket = line.find(']');
            if (bracket != std::string::npos && closeBracket != std::string::npos && closeBracket > bracket) {
                std::string numStr = line.substr(0, bracket);
                TrimWhiteSpace(numStr);
                try {
                    int num = std::stoi(numStr);
                    if (num == cardNum) {
                        std::string cardId = line.substr(bracket + 1, closeBracket - bracket - 1);
                        TrimWhiteSpace(cardId);
                        if (!cardId.empty()) {
                            return cardId;
                        }
                    }
                } catch (...) {}
            }
        }
    }
    return std::to_string(cardNum);
}

// Returns true if every ALSA card referenced by the PipeWire audio-groups JSON
// is currently present (by stable card ID, as listed in /proc/asound/cards).
// Used to decide whether the cached PipeWire group config is still valid for the
// current hardware without forking the (expensive) regeneration PHP. A removed
// or swapped card -> some referenced cardId is absent -> returns false so the
// regen runs and strips/re-resolves it. The regen resolves by stable card ID
// (not card number), so a card that's still present but renumbered keeps a valid
// config -- which is why presence-by-ID is a sufficient check.
static bool pipewireConfigCardsPresent(const std::string& jsonPath) {
    if (!FileExists(jsonPath)) {
        return false;
    }
    Json::Value root;
    if (!LoadJsonFromString(GetFileContents(jsonPath), root)) {
        return false;
    }
    std::vector<std::string> present;
    std::istringstream iss(GetFileContents("/proc/asound/cards"));
    std::string line;
    while (std::getline(iss, line)) {
        auto b = line.find('[');
        auto e = line.find(']');
        if (b != std::string::npos && e != std::string::npos && e > b) {
            std::string id = line.substr(b + 1, e - b - 1);
            TrimWhiteSpace(id);
            if (!id.empty()) {
                present.push_back(id);
            }
        }
    }
    for (const auto& grp : root["groups"]) {
        for (const auto& mbr : grp["members"]) {
            std::string cid = mbr.get("cardId", "").asString();
            if (!cid.empty() && std::find(present.begin(), present.end(), cid) == present.end()) {
                return false;
            }
        }
    }
    return true;
}

static void setupAudio() {
    if (!FileExists("/root/.libao")) {
        PutFileContents("/root/.libao", "dev=default");
    }
    std::string mediaBackend = "pipewire-simple";
    getRawSetting("MediaBackend", mediaBackend);
    std::string mediaBackendLower = mediaBackend;
    std::transform(mediaBackendLower.begin(), mediaBackendLower.end(), mediaBackendLower.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    // The ALSA ("Hardware Direct") backend has been retired -- FPP10 always uses
    // a PipeWire backend. Migrate any device still set to "alsa" to the default
    // pipewire-simple and persist it, so we never take the (being-removed) ALSA
    // path and every other MediaBackend reader sees the new value. Done here, the
    // earliest audio entry point, so the rest of setupAudio runs as PipeWire.
    if (mediaBackendLower == "alsa") {
        printf("FPP - MediaBackend 'alsa' is retired; migrating to 'pipewire-simple'\n");
        setRawSetting("MediaBackend", "pipewire-simple");
        mediaBackend = "pipewire-simple";
        mediaBackendLower = "pipewire-simple";
    }
    bool usePipeWireBackend = (mediaBackendLower == "pipewire" || mediaBackendLower == "pipewire-simple");
    bool runningInDocker = FileExists("/.dockerenv");
    const std::string audioEnvPath = "/run/fppd/fpp-audio.env";
    printf("FPP - Audio backend: %s\n", usePipeWireBackend ? "PipeWire" : "ALSA");
    std::string aplay = execAndReturn("/usr/bin/aplay -l 2>&1");
    std::vector<std::string> lines = split(aplay, '\n');
    std::map<std::string, std::string> cards;
    std::map<std::string, std::string> cardLines; // full aplay line per "card N"
    std::map<std::string, bool> hdmiStatus;
    bool hasNonHDMI = false;
    auto lineHasHDMI = [](const std::string& l) {
        std::string lc = l;
        std::transform(lc.begin(), lc.end(), lc.begin(), ::tolower);
        return lc.find("hdmi") != std::string::npos;
    };
    for (auto& l : lines) {
        if (l.starts_with("card ")) {
            std::string k = l.substr(0, 6);
            std::string v = l.substr(8);
            int idx = v.find(' ');
            v = v.substr(0, idx);
            cards[k] = v;
            cardLines[k] = l;
            hasNonHDMI |= !lineHasHDMI(l);
        }
    }
    int hdmiIdx = 0;
    bool anyHDMIConnected = false;
    for (int x = 0; x < 4; x++) {
        std::string cstr = "/sys/class/drm/card" + std::to_string(x) + "-HDMI-A-1/status";
        if (FileExists(cstr)) {
            for (int p = 1; p < 5; p++) {
                std::string cstr = "/sys/class/drm/card" + std::to_string(x) + "-HDMI-A-" + std::to_string(p) + "/status";
                std::string c = GetFileContents(cstr);
                std::string k = "vc4hdmi" + std::to_string(hdmiIdx);
                bool conn = c.contains("connected") && !c.contains("disconnected");
                hdmiStatus[k] = conn;
                anyHDMIConnected |= conn;
                hdmiIdx++;
            }
        }
    }
    // True when the only audio device is the synthetic snd-dummy (no real,
    // non-HDMI sound card present) -- the common case on a BeagleBone with no
    // audio cape. Used later to skip the expensive PipeWire device-enumeration
    // and group-regeneration dance, which only matters for real/hot-pluggable
    // hardware.
    bool noRealSoundcard = (!hasNonHDMI || contains(aplay, "no soundcards"));
    if (noRealSoundcard) {
        printf("FPP - No Soundcard Detected, loading snd-dummy\n");
        modprobe("snd-dummy");
    }
    int card = getRawSettingInt("AudioOutput", 0);
    std::string cstr = "card " + std::to_string(card);
    bool found = false;
    int count = 0;

    // Treat the selected card as unusable if it's any HDMI card (vc4hdmi OR
    // legacy bcm2835 HDMI) with no display connected. Playing to an
    // unconnected HDMI audio device panics the kernel on some Pis.
    auto cardIsDeadHDMI = [&](const std::string& k) {
        if (!lineHasHDMI(cardLines[k])) return false;
        if (cards[k].starts_with("vc4hdmi")) {
            return !hdmiStatus[cards[k]];
        }
        // legacy bcm2835 HDMI shares the physical port; if any HDMI is
        // connected, assume this device may work, otherwise treat as dead.
        return !anyHDMIConnected;
    };

    if (cardIsDeadHDMI(cstr)) {
        // Walk cards in index order and pick the first non-HDMI, or HDMI with
        // display connected. Falls through to the snd-dummy case otherwise.
        int fallback = -1;
        for (const auto& [k, _] : cards) {
            if (!cardIsDeadHDMI(k)) {
                fallback = std::stoi(k.substr(5));
                break;
            }
        }
        if (fallback >= 0 && fallback != card) {
            printf("FPP - Audio device %d has no HDMI connected, switching to card %d (%s)\n",
                   card, fallback, cards["card " + std::to_string(fallback)].c_str());
            card = fallback;
            setRawSetting("AudioOutput", std::to_string(card));
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
        // Point ALSA's default at the chosen card so root-context playback
        // (e.g. flite from fppinit) doesn't land on a dead HDMI device.
        std::string arc = GetFileContents("/opt/fpp/etc/asoundrc.plain");
        std::string cardLine = "card " + std::to_string(card);
        size_t pos = 0;
        while ((pos = arc.find("card 0", pos)) != std::string::npos) {
            arc.replace(pos, 6, cardLine);
            pos += cardLine.size();
        }
        PutFileContents("/root/.asoundrc", arc);
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
    // Pi headphone jack needs a volume adjustment, in reality a lot of sound cards do, but we
    // don't want to put in a lot of special cases here so only handle the Pi
    if (card == 0) {
        if (contains(aplayl, "[bcm2") && !contains(aplayl, "-i2s")) {
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
    if (usePipeWireBackend) {
        if (!FileExists("/etc/pipewire/client.conf") && FileExists("/usr/share/pipewire/client.conf")) {
            CopyFileContents("/usr/share/pipewire/client.conf", "/etc/pipewire/client.conf");
        }
        asoundrc = GetFileContents("/opt/fpp/etc/asoundrc.pipewire");
    } else {
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
    int pipewireSampleRate = 44100;
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
        pipewireSampleRate = 48000;
        break;
    case 7:
    case 8:
    case 9:
        replaceAll(asoundrc, "rate 44100", "rate 96000");
        pipewireSampleRate = 96000;
        break;
    default:
        break;
    }
    PutFileContents("/root/.asoundrc", asoundrc);
    const std::string pipewireSinkConfPath = "/etc/pipewire/pipewire.conf.d/95-fpp-alsa-sink.conf";
    if (usePipeWireBackend) {
        exec("/bin/mkdir -p /etc/pipewire/pipewire.conf.d");
        // Create FPP ALSA adapter nodes for ALL playback-capable cards present
        // at boot.  This ensures consistent naming (fpp_alsa_{cardIdNorm}) and
        // prevents WirePlumber from creating confusingly-named duplicates.
        // USB devices plugged in after boot get adapters created at Apply time
        // by GeneratePipeWireGroupsConfig() in pipewire.php.
        std::string arecordAll = execAndReturn("/usr/bin/arecord -l 2>/dev/null");
        std::ostringstream pipewireSink;
        pipewireSink << "context.objects = [\n";
        for (const auto& [key, cardName] : cards) {
            int cardNum = std::stoi(key.substr(5)); // "card N" -> N
            std::string cId = getAlsaCardId(cardNum);
            if (cId.empty()) cId = cardName;
            // Normalise card ID for node name: lowercase, non-alnum → underscore
            std::string cidNorm = cId;
            std::transform(cidNorm.begin(), cidNorm.end(), cidNorm.begin(), ::tolower);
            for (auto& ch : cidNorm) {
                if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') ch = '_';
            }

            // Probe ALSA HW params — if this fails the device can't be opened
            // (e.g. HDMI with nothing connected → error 524/ENOMEDIUM).
            // Also skip cards that only support IEC958/passthrough formats —
            // PipeWire's adapter can't negotiate those as normal PCM sinks.
            std::string hwParams = execAndReturn("timeout 2 /usr/bin/aplay -D hw:" + cId + " --dump-hw-params /dev/zero 2>&1 | head -40");
            if (!contains(hwParams, "HW Params")) {
                printf("FPP - PipeWire: skipping card %d (%s) — device cannot be opened\n",
                       cardNum, cId.c_str());
                continue;
            }
            // Verify card supports at least one standard PCM format
            bool hasPcmFormat = contains(hwParams, "S16_LE") || contains(hwParams, "S24_LE")
                             || contains(hwParams, "S24_3LE") || contains(hwParams, "S32_LE");
            if (!hasPcmFormat) {
                printf("FPP - PipeWire: skipping card %d (%s) — no standard PCM format (IEC958 only?)\n",
                       cardNum, cId.c_str());
                continue;
            }

            std::string productName = getAlsaCardProductName(cardNum, cardName);
            std::string desc = productName + " (" + cId + ")";

            // USB audio cards need extra headroom and use different channel-detection logic.
            bool isUsbCard = cardLines.count(key) && cardLines[key].find("USB Audio") != std::string::npos;

            // Detect playback channels from ALSA HW params.
            // Formats: "CHANNELS: 8" (fixed), "CHANNELS[2]: 2 8" (discrete
            // list), "CHANNELS: [1 8]" (continuous range).
            //
            // A continuous range like "[1 8]" means the driver accepts any
            // count in that range — it does NOT imply that many physical
            // outputs.  The Pi onboard analog (bcm2835) reports "[1 8]"
            // because it has 8 hardware mixer subdevices, but it is a
            // 2-channel stereo jack.  Opening it as 8ch feeds interleaved
            // 8-channel data to a stereo DAC → garbled, high-pitched noise.
            // So for non-USB cards with a range we default to stereo;
            // only fixed/discrete declarations use the largest value.
            int maxChannels = 2;
            std::smatch chMatch;
            if (std::regex_search(hwParams, chMatch, std::regex(R"(CHANNELS\[?\d*\]?:\s+(.+))"))) {
                std::string chLine = chMatch[1].str();
                bool isRange = chLine.find('[') != std::string::npos;
                std::regex numRe(R"(\d+)");
                std::sregex_iterator it(chLine.begin(), chLine.end(), numRe);
                std::sregex_iterator end;
                std::vector<int> nums;
                for (; it != end; ++it) {
                    nums.push_back(std::stoi((*it)[0].str()));
                }
                if (isRange && nums.size() >= 2) {
                    int lo = nums.front();
                    int hi = nums.back();
                    if (isUsbCard) {
                        // USB audio: a range [lo hi] covers actual multi-channel
                        // altsets — use the maximum.
                        maxChannels = hi;
                    } else {
                        // Non-USB (e.g. bcm2835 analog): range is misleading,
                        // default to stereo clamped into the range.
                        maxChannels = std::min(std::max(2, lo), hi);
                    }
                } else {
                    // Fixed or discrete list: take the largest declared count.
                    for (int ch : nums) {
                        if (ch > maxChannels) maxChannels = ch;
                    }
                }
            }
            // For USB audio also read /proc/asound/cardN/stream0 which lists
            // every playback altset with its exact channel count.  This catches
            // cases where aplay reports a range or probes only one altset.
            if (isUsbCard) {
                std::string stream0 = GetFileContents("/proc/asound/card" + std::to_string(cardNum) + "/stream0");
                if (!stream0.empty()) {
                    // Only look in the Playback section (stop before Capture:)
                    size_t capturePos = stream0.find("\nCapture:");
                    std::string playSection = (capturePos != std::string::npos) ? stream0.substr(0, capturePos) : stream0;
                    std::regex chRe(R"(\bChannels:\s+(\d+))");
                    std::sregex_iterator it(playSection.begin(), playSection.end(), chRe);
                    std::sregex_iterator end;
                    for (; it != end; ++it) {
                        int ch = std::stoi((*it)[1].str());
                        if (ch > maxChannels) maxChannels = ch;
                    }
                }
            }
            if (maxChannels > 8) maxChannels = 8; // cap at 7.1

            // Detect best audio format from ALSA HW params
            // FORMAT line examples: "FORMAT: S16_LE S24_3LE", "FORMAT: S16_LE S24_LE S32_LE"
            // Priority: S32 > S24 > S16.  ALSA uses _ (S24_3LE), PipeWire drops it (S24LE).
            std::string audioFormat = "S16LE"; // safe default all cards support
            std::smatch fmtMatch;
            if (std::regex_search(hwParams, fmtMatch, std::regex(R"(FORMAT[^:]*:\s+(.+))"))) {
                std::string fmtLine = fmtMatch[1].str();
                if (fmtLine.find("S32_LE") != std::string::npos) {
                    audioFormat = "S32LE";
                } else if (fmtLine.find("S24_LE") != std::string::npos) {
                    audioFormat = "S24_32LE"; // 24-bit in 32-bit container
                } else if (fmtLine.find("S24_3LE") != std::string::npos) {
                    audioFormat = "S24LE"; // packed 24-bit (3 bytes)
                }
            }

            // Channel position arrays matching PipeWire convention
            static const char* positionArrays[] = {
                nullptr,
                "[ MONO ]",
                "[ FL FR ]",
                "[ FL FR FC ]",
                "[ FL FR RL RR ]",
                "[ FL FR FC RL RR ]",
                "[ FL FR FC LFE RL RR ]",
                "[ FL FR FC LFE RL RR RC ]",
                "[ FL FR FC LFE RL RR SL SR ]"
            };
            const char* posStr = (maxChannels >= 1 && maxChannels <= 8) ? positionArrays[maxChannels] : "[ FL FR ]";

            // USB audio cards need extra headroom: their independent oscillators
            // drift relative to the PipeWire graph driver clock, causing resyncs.
            int headroom = isUsbCard ? 4096 : 256;

            printf("FPP - PipeWire: creating adapter fpp_alsa_%s for card %d (%s) [%dch %s]%s\n",
                   cidNorm.c_str(), cardNum, cId.c_str(), maxChannels, audioFormat.c_str(),
                   isUsbCard ? " (USB, headroom=4096)" : "");
            pipewireSink << "  { factory = adapter\n"
                         << "    args = {\n"
                         << "      factory.name = api.alsa.pcm.sink\n"
                         << "      node.name = \"fpp_alsa_" << cidNorm << "\"\n"
                         << "      node.description = \"" << desc << "\"\n"
                         << "      media.class = \"Audio/Sink\"\n"
                         << "      api.alsa.path = \"hw:" << cId << "\"\n"
                         << "      api.alsa.period-size = " << perSize << "\n"
                         << "      api.alsa.headroom = " << headroom << "\n"
                         << "      audio.format = \"" << audioFormat << "\"\n"
                         << "      audio.rate = " << pipewireSampleRate << "\n"
                         << "      audio.channels = " << maxChannels << "\n"
                         << "      audio.position = " << posStr << "\n"
                         << "    }\n"
                         << "  }\n";
            // If this card has capture capability, also create an Audio/Source node.
            if (arecordAll.find("card " + std::to_string(cardNum) + ":") != std::string::npos) {
                // Detect capture channel count from ALSA HW params
                int capChannels = 2; // safe default — most USB cards need at least 2
                std::string capParams = execAndReturn("timeout 2 /usr/bin/arecord -D hw:" + cId + " --dump-hw-params /dev/zero 2>&1 | head -40");
                if (contains(capParams, "HW Params")) {
                    std::smatch capChMatch;
                    if (std::regex_search(capParams, capChMatch, std::regex(R"(CHANNELS\[?\d*\]?:\s+(.+))"))) {
                        std::string capChLine = capChMatch[1].str();
                        // Find the smallest number (minimum channels)
                        std::regex capNumRe(R"(\d+)");
                        std::sregex_iterator capIt(capChLine.begin(), capChLine.end(), capNumRe);
                        std::sregex_iterator capEnd;
                        int minCap = 99;
                        for (; capIt != capEnd; ++capIt) {
                            int c = std::stoi((*capIt)[0].str());
                            if (c > 0 && c < minCap) minCap = c;
                        }
                        if (minCap > 0 && minCap < 99) capChannels = minCap;
                    }
                }
                printf("FPP - PipeWire: card %d (%s) has capture [%dch], creating source node\n",
                       cardNum, cId.c_str(), capChannels);
                pipewireSink << "  { factory = adapter\n"
                             << "    args = {\n"
                             << "      factory.name = api.alsa.pcm.source\n"
                             << "      node.name = \"fpp_alsain_" << cidNorm << "\"\n"
                             << "      node.description = \"" << desc << "\"\n"
                             << "      node.nick = \"" << cId << "\"\n"
                             << "      media.class = \"Audio/Source\"\n"
                             << "      api.alsa.path = \"hw:" << cId << "\"\n"
                             << "      audio.format = \"" << audioFormat << "\"\n"
                             << "      audio.rate = 44100\n"
                             << "      audio.channels = " << capChannels << "\n"
                             << "    }\n"
                             << "  }\n";
            }
        }
        pipewireSink << "]\n";
        PutFileContents(pipewireSinkConfPath, pipewireSink.str());
    } else if (FileExists(pipewireSinkConfPath)) {
        unlink(pipewireSinkConfPath.c_str());
    }
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

    if (!runningInDocker) {
        mkdir("/run/fppd", 0755);
        std::ostringstream audioEnv;
        audioEnv << "SDL_AUDIODRIVER=" << (usePipeWireBackend ? "pulse" : "alsa") << "\n";
        // Keep OpenAL on the same backend as the rest of FPP audio.
        // In hardware (ALSA) mode this prevents OpenAL from defaulting to
        // PulseAudio and aborting during teardown if Pulse is unavailable.
        audioEnv << "ALSOFT_DRIVERS=" << (usePipeWireBackend ? "pulse" : "alsa") << "\n";
        if (usePipeWireBackend) {
            audioEnv << "PIPEWIRE_RUNTIME_DIR=/run/pipewire-fpp\n"
                     << "XDG_RUNTIME_DIR=/run/pipewire-fpp\n"
                     << "PIPEWIRE_CONFIG_DIR=/etc/pipewire\n"
                     << "PULSE_RUNTIME_PATH=/run/pipewire-fpp/pulse\n";
        }
        PutFileContents(audioEnvPath, audioEnv.str());
    } else if (FileExists(audioEnvPath)) {
        unlink(audioEnvPath.c_str());
    }

    // --- Audio Output Groups Configuration ---
    // On first PipeWire setup, create default output and input group configs
    // that mirror the legacy ALSA configuration: one output group with the
    // configured sound card, and one input group with fppd_stream_1 routed
    // to it.  This ensures audio works immediately when switching to PipeWire.
    const std::string groupsJsonPath = FPP_MEDIA_DIR + "/config/pipewire-audio-groups.json";
    const std::string simpleGroupsJsonPath = FPP_MEDIA_DIR + "/config/pipewire-audio-groups-simple.json";
    const std::string igJsonPath = FPP_MEDIA_DIR + "/config/pipewire-input-groups.json";

    // Simple PipeWire mode: the active config lives in pipewire-audio-groups-simple.json
    // and PipeWireSinkName is written by ApplyPipeWireSimpleConfig().  If that file is
    // missing (e.g. OS upgrade, fresh flash, or settings migrated without an explicit
    // UI save) synthesise it now from the current AudioOutput/VideoOutput settings so
    // PipeWireSinkName is populated before fppd starts.
    // Regenerate the simple groups config when it's missing OR when it
    // references a sound card that's no longer present. The latter is the
    // card-changed case (e.g. a USB card added to a previously dummy-only board,
    // or removed): the stale config would otherwise leave an unresolved device,
    // producing a "# WARNING:" conf that (a) is wrong and (b) defeats the
    // skip-regen fast path, forcing the full regen+restart dance on every boot.
    // Regenerating here re-points it at the current AudioOutput card.
    if (usePipeWireBackend && !runningInDocker && mediaBackendLower == "pipewire-simple"
        && (!FileExists(simpleGroupsJsonPath) || !pipewireConfigCardsPresent(simpleGroupsJsonPath))) {
        printf("FPP - pipewire-simple: simple groups config missing or references an absent card; regenerating from AudioOutput setting...\n");
        system("/usr/bin/php /opt/fpp/scripts/apply_pipewire_simple_config");
    }

    if (usePipeWireBackend && !runningInDocker && !FileExists(groupsJsonPath)) {
        std::string defCardId = getAlsaCardId(card);
        printf("FPP - First PipeWire setup: creating default output group (card %s) and input group\n", defCardId.c_str());

        // Default output group: one group with the legacy audio device
        Json::Value ogRoot;
        Json::Value group;
        group["id"] = 1;
        group["name"] = "Main Output";
        group["enabled"] = true;
        Json::Value member;
        member["cardId"] = defCardId;
        member["channels"] = 2;
        member["delayMs"] = 0.0;
        group["members"] = Json::Value(Json::arrayValue);
        group["members"].append(member);
        ogRoot["groups"] = Json::Value(Json::arrayValue);
        ogRoot["groups"].append(group);

        PutFileContents(groupsJsonPath, SaveJsonToString(ogRoot));

        // Default input group: fppd_stream_1 routed to output group 1
        Json::Value igRoot;
        Json::Value ig;
        ig["id"] = 1;
        ig["name"] = "Mix Bus 1";
        ig["enabled"] = true;
        ig["channels"] = 2;
        ig["volume"] = 100;
        Json::Value igMember;
        igMember["type"] = "fppd_stream";
        igMember["sourceId"] = "fppd_stream_1";
        igMember["name"] = "Media Playback";
        igMember["volume"] = 100;
        igMember["mute"] = false;
        ig["members"] = Json::Value(Json::arrayValue);
        ig["members"].append(igMember);
        ig["outputs"] = Json::Value(Json::arrayValue);
        ig["outputs"].append(1);
        igRoot["inputGroups"] = Json::Value(Json::arrayValue);
        igRoot["inputGroups"].append(ig);

        PutFileContents(igJsonPath, SaveJsonToString(igRoot));
    }

    // Restore cached combine-stream and input group configs so group sinks
    // survive reboot.  These are initially copied as-is so PipeWire starts
    // with the right module layout.  After PipeWire is up and WirePlumber
    // has enumerated devices, a regeneration script re-resolves card→sink
    // mappings to handle card-number changes.
    const std::string groupsConfCache = FPP_MEDIA_DIR + "/config/pipewire-audio-groups.conf";
    const std::string groupsConfDest = "/etc/pipewire/pipewire.conf.d/97-fpp-audio-groups.conf";
    const std::string igConfCache = FPP_MEDIA_DIR + "/config/pipewire-input-groups.conf";
    const std::string igConfDest = "/etc/pipewire/pipewire.conf.d/96-fpp-input-groups.conf";
    bool hasGroupsConfig = false;
    // Track whether the effective PipeWire config actually changes from what it
    // already loaded at boot. PipeWire (fpp-pipewire.service) starts during boot
    // with the persisted /etc/pipewire configs, so a restart in postNetwork is
    // only needed when something actually differs -- otherwise the costly
    // pipewire-pulse/WirePlumber/session-bus restart cascade buys nothing. On a
    // no-soundcard board the config is identical every boot, so this skips it.
    bool audioConfigChanged = false;
    if (usePipeWireBackend && !runningInDocker && FileExists(groupsConfCache)) {
        if (!FileExists(groupsConfDest) || GetFileContents(groupsConfCache) != GetFileContents(groupsConfDest)) {
            printf("FPP - Restoring PipeWire audio output groups config\n");
            exec("/bin/cp " + groupsConfCache + " " + groupsConfDest);
            audioConfigChanged = true;
        }
        hasGroupsConfig = true;
    } else if (usePipeWireBackend && !runningInDocker && FileExists(groupsJsonPath)) {
        // JSON exists but no cached .conf yet (e.g. first-time default creation).
        // The regeneration script will generate the .conf from the JSON.
        hasGroupsConfig = true;
        audioConfigChanged = true;
    } else if (usePipeWireBackend && !runningInDocker && FileExists(groupsConfDest)) {
        // JSON config was deleted but stale conf remains — clean up
        if (!FileExists(groupsJsonPath)) {
            unlink(groupsConfDest.c_str());
            audioConfigChanged = true;
        }
    }
    if (usePipeWireBackend && !runningInDocker && FileExists(igConfCache)) {
        if (!FileExists(igConfDest) || GetFileContents(igConfCache) != GetFileContents(igConfDest)) {
            printf("FPP - Restoring PipeWire input groups config\n");
            exec("/bin/cp " + igConfCache + " " + igConfDest);
            audioConfigChanged = true;
        }
    }

    // --- AES67 cleanup ---
    // AES67 is now managed by AES67Manager in fppd (GStreamer-based).
    // Remove any leftover PipeWire RTP module configs from the legacy Python approach.
    unlink("/etc/pipewire/pipewire.conf.d/96-fpp-aes67-rtp.conf");
    unlink("/etc/pipewire/pipewire.conf.d/96-fpp-aes67-sap.conf");
    unlink("/etc/ptp4l-fpp.conf");
    // Kill any leftover legacy daemons
    exec("pkill -f fpp_aes67_sap 2>/dev/null || true");
    exec("pkill -f 'ptp4l.*ptp4l-fpp' 2>/dev/null || true");

    // PipeWire is already running (started at boot with the persisted configs),
    // so only restart it when the config actually changed. A restart is
    // synchronous and triggers an expensive pipewire-pulse/WirePlumber/
    // session-bus cascade, so skipping it on an unchanged boot (always the case
    // on a no-soundcard board) saves ~10-15s on a single-core SBC.
    if (usePipeWireBackend && !runningInDocker) {
        // Validate/regenerate the audio group config from the JSON, and use its
        // exit code (2 == it changed/created the .conf) to decide whether a
        // restart is needed. Run it whenever there's a groups config -- NOT gated
        // on the dest already existing (chicken-and-egg: the regen is what creates
        // it).
        //
        // No real sound card: the synthetic snd-dummy can't go missing or shift
        // cards, so the cached .conf is permanently valid. Run WITHOUT --force so
        // the regen fast-exits (no pw-dump, no rewrite) when the cache is already
        // clean -- avoiding a full regenerate + PipeWire restart on every boot.
        // (--force is non-deterministic and reports "changed" every run, so it
        // would restart PipeWire every boot.) With a real card we keep --force so
        // a removed/shifted device is stripped before PipeWire opens it (PipeWire
        // crashes fatally on a missing ALSA device).
        if (hasGroupsConfig) {
            // Decide whether the cached PipeWire group config is still valid in
            // C++ (cheap file reads) so we only fork the regeneration PHP -- which
            // costs ~3s of php/common.php startup, ~6s under boot contention --
            // when something actually changed. This covers BOTH no-soundcard and
            // real-card boards (e.g. BBB capes with onboard PCM5012A): skip the
            // PHP when the cached conf is complete (no unresolved-device warning),
            // matches what PipeWire loaded at boot (dest), and every sound card it
            // references is still present. The regen only runs on a genuine change
            // (first boot, settings edit, or a card added/removed), which is rare.
            bool needRegen = true;
            if (FileExists(groupsConfCache) && FileExists(groupsConfDest)) {
                std::string cached = GetFileContents(groupsConfCache);
                const std::string& activeJson =
                    (mediaBackendLower == "pipewire-simple") ? simpleGroupsJsonPath : groupsJsonPath;
                if (cached.find("# WARNING:") == std::string::npos
                    && cached == GetFileContents(groupsConfDest)
                    && pipewireConfigCardsPresent(activeJson)) {
                    needRegen = false;
                    printf("FPP - PipeWire audio config valid, loaded, and all referenced cards present; skipping regeneration\n");
                }
            }
            if (needRegen) {
                std::string regenCmd = "/usr/bin/php /opt/fpp/scripts/regenerate_pipewire_groups";
                if (!noRealSoundcard) {
                    regenCmd += " --force";
                }
                printf("FPP - Validating PipeWire audio group config against current hardware...\n");
                int rc = system(regenCmd.c_str());
                if (WEXITSTATUS(rc) == 2) {
                    audioConfigChanged = true;
                }
            }
        }

        if (!audioConfigChanged) {
            // Config matches what PipeWire loaded at boot -- restarting would
            // change nothing (and the volume-restore below only exists to undo a
            // restart's reset of WirePlumber state, so it's unneeded too).
            printf("FPP - PipeWire audio config unchanged since boot; skipping restart\n");
        } else if (hasGroupsConfig && noRealSoundcard) {
            // Config changed but there's no real sound card: a restart is needed
            // to load it, but there are no USB cards to enumerate and no
            // card-number shifts to resolve, so skip the enumerate/regen dance.
            exec("/usr/bin/systemctl restart fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service");
            printf("FPP - No real sound card present; skipping PipeWire device enumeration/regeneration\n");
        } else {
            exec("/usr/bin/systemctl restart fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service");
        }

        // Wait for WirePlumber to enumerate ALSA devices before regenerating
        // configs.  WirePlumber needs a moment to discover USB sound cards
        // and create their PipeWire sink nodes.
        if (audioConfigChanged && hasGroupsConfig && !noRealSoundcard) {
            printf("FPP - Waiting for WirePlumber to enumerate devices...\n");
            std::this_thread::sleep_for(std::chrono::seconds(3));

            // Regenerate output/input group configs using current device state.
            // This fixes card-number shifts after reboot and resolves any
            // previously unresolved members.  Exit code 2 = config changed.
            printf("FPP - Regenerating PipeWire audio group configs...\n");
            int regenResult = system("/usr/bin/php /opt/fpp/scripts/regenerate_pipewire_groups --force");
            regenResult = WEXITSTATUS(regenResult);
            if (regenResult == 2) {
                // Config was changed — restart PipeWire to pick up the new config
                printf("FPP - PipeWire group config changed, restarting PipeWire...\n");
                exec("/usr/bin/systemctl restart fpp-pipewire.service fpp-wireplumber.service fpp-pipewire-pulse.service");
                // Wait for WirePlumber & combine-stream/filter-chain nodes to come up
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }

            // Restore per-group and per-member volume levels from the JSON config.
            // WirePlumber may restore stale volume state on startup.
            printf("FPP - Restoring PipeWire audio group volumes...\n");
            system("/usr/bin/php /opt/fpp/scripts/restore_pipewire_volumes");
        }
    } else if (!runningInDocker) {
        exec("/usr/bin/systemctl stop fpp-pipewire-pulse.service fpp-wireplumber.service fpp-pipewire.service");
    }

    // AES67 is now initialized by AES67Manager in fppd after PipeWire is running.
    // No external daemons to start here.
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
    if (!FileExists("/usr/sbin/zramswap")) {
        return;
    }
    // Gate on whether zram is already ACTIVE as swap, not on whether the device
    // node exists: kernels built with CONFIG_ZRAM=y (e.g. the BeagleBone) always
    // present /dev/zram0 even when nothing has configured it, so the old
    // FileExists("/dev/zram0") check made this a permanent no-op and zram swap
    // never came up. (On the Pi the systemd-zram-generator/rpi-swap path may
    // already have set it up, in which case /proc/swaps shows it and we skip.)
    std::string swaps = GetFileContents("/proc/swaps");
    if (swaps.find("zram") == std::string::npos) {
        execbg("/usr/sbin/zramswap start 2>/dev/null > /dev/null &");
    }
}
void startDiskSwap() {
    // The image ships a disk swap partition (e.g. mmcblk0p2) marked "noauto" in
    // fstab so systemd does NOT activate it during boot: swapping to eMMC/SD
    // wears the flash, and waiting for the swap device to be tagged by udev
    // would otherwise gate swap.target on the slow single-core coldplug. zram is
    // the primary swap; this larger disk swap is only useful for the occasional
    // heavy job (e.g. compiling FPP). Bring it up here, late in postNetwork, off
    // the boot critical path. swapon is harmless/idempotent if already active.
    exec("/usr/bin/awk '$3 == \"swap\" && $1 !~ /zram/ { print $1 }' /etc/fstab 2>/dev/null "
         "| while read -r dev; do /sbin/swapon \"$dev\" 2>/dev/null || true; done");
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
    size_t idx = content.find("dtoverlay=vc4-kms-dpi-fpp");
    std::string origLine = "";
    if (idx != std::string::npos) {
        size_t idx2 = content.find("\n", idx);
        origLine = content.substr(idx, idx2 - idx);
    }
    if (hasDPI) {
        int fps = getRawSettingInt("DPI_FPS", 40);
        std::string model = GetFileContents("/sys/firmware/devicetree/base/model");
        std::string width = "1920";
        std::string height = fps == 40 ? "497" : "997";
        std::string line = "dtoverlay=vc4-kms-dpi-fpp,rgb888,hactive=" + width + ",hfp=0,hsync=1,hbp=0,vactive=" + height + ",vfp=1,vsync=1,vbp=1";
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
        if (!FileExists("/etc/fpp/container")) {
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

        if (!FileExists("/etc/fpp/container")) {
            // Create boot delay flag file early if boot delay is configured
            // so UI can show warning immediately when Apache starts
            int bootDelaySetting = getRawSettingInt("bootDelay", -1);
            if (bootDelaySetting != 0) {
                // Store start time and duration/mode for UI countdown
                time_t startTime = time(nullptr);
                if (bootDelaySetting > 0) {
                    std::string flagContent = std::to_string(startTime) + "," + std::to_string(bootDelaySetting);
                    PutFileContents(FPP_MEDIA_DIR + "/tmp/boot_delay", flagContent);
                } else if (bootDelaySetting == -1) {
                    std::string flagContent = std::to_string(startTime) + ",auto";
                    PutFileContents(FPP_MEDIA_DIR + "/tmp/boot_delay", flagContent);
                }
            }
        } else {
            // Ensure no boot delay flag file exists. No delay in docker.
            unlink((FPP_MEDIA_DIR + "/tmp/boot_delay").c_str());
        }

        // Notify systemd that initialization is complete
        sd_notify(0, "READY=1\nSTATUS=FPP initialization complete");
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
        // Time sync wait happens AFTER interfaces are up so NTP has a chance to sync
        handleTimeSyncWait();
        if (!FileExists("/etc/fpp/desktop")) {
            maybeEnableTethering();
            detectNetworkModules();
        }
        setupTimezone(); // this may not have worked in the init phase, try again
        setFileOwnership();
        checkInstallPackages();
        startZRAMSwap();
        startDiskSwap();
        // Notify systemd that post-network setup is complete
        sd_notify(0, "READY=1\nSTATUS=FPP post-network setup complete");
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
