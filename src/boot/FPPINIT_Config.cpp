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

#include <chrono>
#include "fpp-json.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#if __has_include(<gpiod.hpp>)
#include <gpiod.hpp>
#define HASGPIOD
#if __has_include(<gpiodcxx/chip.hpp>)
#define IS_GPIOD_CXX_V2
#endif
#endif

#include "FPPINIT.h"

void DetectCape() {
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

void checkSSHKeys() {
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
    } else {
        // No hardware RNG to seed from: ssh-keygen's getrandom() can block
        // waiting for the CRNG to seed on a freshly booted board with little
        // entropy. fppinit no longer waits for haveged on every boot (it's only
        // needed right here -- for first-boot key generation on a board without
        // an RNG), so start it on demand to seed the pool before generating keys.
        exec("/usr/bin/systemctl start haveged.service");
    }
    execbg("/usr/bin/ssh-keygen -q -N \"\" -t ecdsa -f /etc/ssh/ssh_host_ecdsa_key &");
    execbg("/usr/bin/ssh-keygen -q -N \"\" -t ed25519 -f /etc/ssh/ssh_host_ed25519_key &");
    execbg("/usr/bin/ssh-keygen -q -N \"\" -t rsa -b 2048 -f /etc/ssh/ssh_host_rsa_key &");
}

// Copies files/config from the /boot partition to /home/fpp/media
// On the Pi, /boot (or /boot/firmware) is vfat and thus the
// user could stick a default config on there
void handleBootPartition() {
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

void checkHostName() {
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

void runScripts(const std::string tp, bool userBefore) {
    std::string pfx = "FPPDIR=/opt/fpp SRCDIR=/opt/fpp/src ";

    if (userBefore && FileExists(FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh")) {
        printf("FPP - Running UserCallbackHook.sh %s\n", tp.c_str());
        execbg(pfx + FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh " + tp);
    }
    for (const auto& entry : std::filesystem::directory_iterator(FPP_MEDIA_DIR + "/plugins")) {
        if (entry.is_directory()) {
            std::string filename = entry.path().filename();
            std::string cmd = FPP_MEDIA_DIR + "/plugins/" + filename + "/scripts/" + tp + ".sh";
            if (FileExists(cmd)) {
                printf("FPP - Running plugin %s script for %s\n", filename.c_str(), tp.c_str());
                execbg(pfx + cmd);
            }
        }
    }
    if (!userBefore && FileExists(FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh")) {
        printf("FPP - Running UserCallbackHook.sh %s\n", tp.c_str());
        execbg(pfx + FPP_MEDIA_DIR + "/scripts/UserCallbackHook.sh " + tp);
    }
}

void checkFSTAB() {
    std::string cont = GetFileContents("/etc/fstab");
    if (cont.find("home/fpp/media") == std::string::npos) {
        cont += "\n";
        cont += "#####################################\n";
        cont += "#/dev/sda1     /home/fpp/media  auto    defaults,noatime,nodiratime,exec,nofail,flush,uid=1000,gid=1000  0  0\n";
        cont += "#####################################\n";
        PutFileContents("/etc/fstab", cont);
    }
}

void createDirectories() {
    static std::vector<std::string> DIRS = { "", "config", "effects", "logs", "music", "playlists", "scripts", "sequences",
                                             "upload", "videos", "plugins", "plugindata", "exim4", "images", "cache",
                                             "backups", "tmp", "virtualdisplay_assets" };
    printf("FPP - Checking for required directories\n");
    uid_t fppUid = 0;
    gid_t fppGid = 0;
    bool haveFppUser = GetUserIds("fpp", &fppUid, &fppGid);
    for (auto& d : DIRS) {
        std::string dir = FPP_MEDIA_DIR + "/" + d;
        if (!DirectoryExists(dir)) {
            printf("    Creating directory %s\n", dir.c_str());
            mkdir(dir.c_str(), 0775);
            if (haveFppUser) {
                chown(dir.c_str(), fppUid, fppGid);
            }
        }
    }
}

void setupApache() {
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

// Fan thermal trip settings.  The web UI (PrintFanThermalSettings in
// www/common.php) creates settings named FanTrip_<zone type stripped to
// alphanumerics>_<trip index> holding degrees C for thermal zones that have a
// fan cooling device bound to them (the Pi5 case/active-cooler fan, cape fans
// wired into a thermal zone via device tree).  Trip points reset to their
// device tree defaults on every boot, so applyThermalSettings() runs during
// boot; it is also invoked directly ("fppinit applyThermal") when one of the
// settings is changed in the UI.
//
// The first pass each boot records the untouched device tree trip values into
// a snapshot in media/tmp (cleared on every boot) BEFORE applying any
// settings, so resetThermalSettings() ("fppinit resetThermal", the UI's
// "Reset to Defaults" button) can restore the hardware defaults without a
// reboot.
static const std::string FAN_TRIP_DEFAULTS_FILE = FPP_MEDIA_DIR + "/tmp/fan_thermal_defaults.json";
static const std::string FAN_PROBE_FILE = FPP_MEDIA_DIR + "/tmp/fan_probe.json";

static std::string fanTripKey(const std::string& ztype, int trip) {
    std::string key;
    for (char c : ztype) {
        if (isalnum((unsigned char) c)) {
            key += c;
        }
    }
    key += "_";
    key += std::to_string(trip);
    return key;
}

// Walk every "active" trip point of every thermal zone, calling
// cb(tripTempFile, "<sanitized zone type>_<trip index>").  Passive/critical
// trips belong to the kernel's throttle/shutdown logic and are never visited.
static void forEachActiveTripPoint(const std::function<void(const std::string&, const std::string&)>& cb) {
    for (int zn = 0; zn < 32; zn++) {
        std::string base = "/sys/class/thermal/thermal_zone" + std::to_string(zn);
        if (!FileExists(base + "/type")) {
            break;
        }
        std::string ztype = GetFileContents(base + "/type");
        TrimWhiteSpace(ztype);
        for (int t = 0; t < 16; t++) {
            std::string tripFile = base + "/trip_point_" + std::to_string(t) + "_temp";
            if (!FileExists(tripFile)) {
                break;
            }
            std::string tripType = GetFileContents(base + "/trip_point_" + std::to_string(t) + "_type");
            TrimWhiteSpace(tripType);
            if (tripType != "active") {
                continue;
            }
            cb(tripFile, fanTripKey(ztype, t));
        }
    }
}

// captureDefaults must ONLY be set from the boot paths, where sysfs still holds
// the untouched device tree values.  Capturing during an on-demand apply (the
// UI changing a setting) could record an already-customized temperature as the
// "default" if the snapshot had been lost, which would make a later reset
// restore the user's value instead of the hardware one.
void applyThermalSettings(bool captureDefaults) {
    if (FileExists("/etc/fpp/container")) {
        return;
    }
    Json::Value defaults;
    if (FileExists(FAN_TRIP_DEFAULTS_FILE)) {
        LoadJsonFromString(GetFileContents(FAN_TRIP_DEFAULTS_FILE), defaults, JsonRoot::Object);
    }
    bool defaultsChanged = false;
    forEachActiveTripPoint([&](const std::string& tripFile, const std::string& key) {
        if (captureDefaults && !JsonHas(defaults, key)) {
            std::string cur = GetFileContents(tripFile);
            TrimWhiteSpace(cur);
            if (!cur.empty()) {
                defaults[key] = atoi(cur.c_str());
                defaultsChanged = true;
            }
        }
        int temp = getRawSettingInt("FanTrip_" + key, -1);
        if (temp > 0) {
            printf("FPP - Setting fan trip point %s to %dC\n", key.c_str(), temp);
            PutFileContents(tripFile, std::to_string(temp * 1000));
        }
    });
    if (defaultsChanged) {
        PutFileContents(FAN_TRIP_DEFAULTS_FILE, SaveJsonToString(defaults));
    }
}

void resetThermalSettings() {
    if (FileExists("/etc/fpp/container")) {
        return;
    }
    // Remove all FanTrip_ settings (same rewrite approach as setRawSetting)
    std::string content = GetFileContents(FPP_MEDIA_DIR + "/settings");
    std::vector<std::string> lines = split(content, '\n');
    content = "";
    for (auto& line : lines) {
        if (!line.empty() && !line.starts_with("FanTrip_")) {
            content += line;
            content += "\n";
        }
    }
    PutFileContents(FPP_MEDIA_DIR + "/settings", content);

    // Restore the device tree default temperatures captured at boot.  If the
    // snapshot is somehow gone (media/tmp cleared by hand mid-run), the
    // settings removed above still restore the defaults on the next boot, so
    // warn rather than fail.
    Json::Value defaults;
    if (FileExists(FAN_TRIP_DEFAULTS_FILE) && LoadJsonFromString(GetFileContents(FAN_TRIP_DEFAULTS_FILE), defaults, JsonRoot::Object)) {
        forEachActiveTripPoint([&](const std::string& tripFile, const std::string& key) {
            if (JsonHas(defaults, key)) {
                printf("FPP - Restoring fan trip point %s to %dmC\n", key.c_str(), defaults[key].asInt());
                PutFileContents(tripFile, std::to_string(defaults[key].asInt()));
            }
        });
    } else {
        printf("FPP - No fan trip point defaults snapshot; settings cleared, defaults restore on next boot\n");
    }
}

// A pwm-fan hwmon node always exposes fanN_input (the tachometer) even when no
// fan -- or a dead fan -- is wired to it: it simply reads 0 RPM.  That is
// indistinguishable from a healthy fan the thermal governor has merely turned
// off, so the reading is worse than useless (it looks like a stalled fan).  To
// tell "off" from "absent/dead", force each fan on and read its tachometer.
//
// Lowering a trip point does NOT reliably wake the (poll-disabled) thermal
// governor on the Pi5/K16Pro kernels, and driving the cooling-device cur_state
// ramps only gradually, so drive the hwmon PWM directly (pwm1_enable=1 +
// pwm1=max).  A healthy fan spins up to thousands of RPM within ~150ms; an
// absent/dead one stays 0.  The pre-probe PWM state is restored afterwards (the
// governor reasserts control on the next thermal event).
//
// The result is written to media/tmp/fan_probe.json (wiped every boot), keyed by
// "<hwmon name>:<fan index>", so Sensors::DetectFanSensors() in fppd can suppress
// the RPM display for a fan whose tachometer can neither be controlled nor read.
// This runs during the boot "start" phase, before fppd caches its sensor list.
void probeFanPresence() {
    if (FileExists("/etc/fpp/container")) {
        return;
    }
    Json::Value result;
    for (int h = 0; h < 32; h++) {
        std::string hwmon = "/sys/class/hwmon/hwmon" + std::to_string(h);
        if (!DirectoryExists(hwmon)) {
            break;
        }
        std::vector<int> fans;
        for (int f = 1; f <= 8; f++) {
            if (FileExists(hwmon + "/fan" + std::to_string(f) + "_input")) {
                fans.push_back(f);
            }
        }
        if (fans.empty()) {
            continue;
        }
        std::string name = GetFileContents(hwmon + "/name");
        TrimWhiteSpace(name);
        if (name.empty()) {
            name = "hwmon" + std::to_string(h);
        }

        // Force the fan on via the hwmon PWM interface, remembering the current
        // state so it can be restored.  pwm1 drives every fan on a pwm-fan node.
        std::string pwmFile = hwmon + "/pwm1";
        std::string enFile = hwmon + "/pwm1_enable";
        bool forced = FileExists(pwmFile);
        std::string origPwm, origEn;
        if (forced) {
            origPwm = GetFileContents(pwmFile);
            TrimWhiteSpace(origPwm);
            if (FileExists(enFile)) {
                origEn = GetFileContents(enFile);
                TrimWhiteSpace(origEn);
                PutFileContents(enFile, "1");
            }
            PutFileContents(pwmFile, "255");
        }

        // Sample each tachometer until it reads non-zero (healthy fans spin up
        // within ~150ms); cap at ~0.7s so an absent/dead fan can't stall boot.
        std::vector<int> rpm(fans.size(), 0);
        for (int s = 0; s < 7; s++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            bool anyZero = false;
            for (size_t fi = 0; fi < fans.size(); fi++) {
                if (rpm[fi] == 0) {
                    std::string r = GetFileContents(hwmon + "/fan" + std::to_string(fans[fi]) + "_input");
                    TrimWhiteSpace(r);
                    rpm[fi] = atoi(r.c_str());
                    if (rpm[fi] == 0) {
                        anyZero = true;
                    }
                }
            }
            if (!anyZero) {
                break;
            }
        }

        // Restore the pre-probe PWM state.
        if (forced) {
            PutFileContents(pwmFile, origPwm);
            if (!origEn.empty()) {
                PutFileContents(enFile, origEn);
            }
        }

        for (size_t fi = 0; fi < fans.size(); fi++) {
            std::string key = name + ":" + std::to_string(fans[fi]);
            Json::Value e;
            e["rpm"] = rpm[fi];
            e["present"] = rpm[fi] > 0;
            result[key] = e;
            printf("FPP - Fan probe %s: %d RPM (%s)\n", key.c_str(), rpm[fi],
                   rpm[fi] > 0 ? "present" : "no tach signal, RPM display suppressed");
        }
    }
    if (result.size() > 0) {
        PutFileContents(FAN_PROBE_FILE, SaveJsonToString(result));
    }
}

void setFileOwnership() {
    exec("/usr/bin/chown -R fpp:fpp " + FPP_MEDIA_DIR);
}

bool checkUnpartitionedSpace() {
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
void resizeRootFS() {
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
void setupTimezone() {
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

void cleanupChromiumFiles() {
    exec("/usr/bin/rm -rf /home/fpp/.config/chromium/Singleton* 2>/dev/null > /dev/null");
}

void setupKiosk(bool force) {
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

// Run "apt-get <args>" synchronously and return true only if it exited 0.
static bool runAptGet(const std::vector<std::string>& args) {
    pid_t pid = fork();
    if (pid == -1) {
        printf("Error: Failed to fork process\n");
        return false;
    } else if (pid == 0) {
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>("apt-get"));
        for (const auto& a : args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);
        execvp("apt-get", argv.data());
        // If execvp fails, exit the child directly (don't fall back into the
        // parent's control flow).
        printf("Error: Failed to execute apt-get\n");
        _exit(127);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// Extract a package name from a userpackages.json entry. Supports both the
// legacy schema (a bare string) and the ownership schema
// ({"package": "name", "requestedBy": [...]} written by the PHP package
// helpers). Returns "" when the entry carries no usable package name.
static std::string packageNameFromJson(const Json::Value& item) {
    if (item.isString()) {
        return item.asString();
    }
    if (item.isObject() && item.isMember("package") && item["package"].isString()) {
        return item["package"].asString();
    }
    return "";
}

// Returns true if there is nothing to do or every package installed; false only
// when an apt failure occurred that is worth retrying on the next boot. The
// DPkg::Lock::Timeout option lets apt wait for a concurrent install at boot
// instead of failing immediately.
bool installPackagesFromJson(const std::string& filePath) {
    std::ifstream file(filePath, std::ifstream::binary);
    if (!file) {
        // No user package list -> nothing to install, don't keep retrying.
        printf("No user package list at %s, nothing to install\n", filePath.c_str());
        return true;
    }

    Json::Value root;
    Json::CharReaderBuilder reader;
    std::string errs;

    if (!Json::parseFromStream(reader, file, &root, &errs)) {
        // Malformed config won't fix itself on retry; consume the trigger.
        printf("Error: Failed to parse JSON - %s\n", errs.c_str());
        return true;
    }

    if (!root.isArray()) {
        printf("Error: JSON is not an array\n");
        return true;
    }

    bool anyPackages = false;
    for (const auto& item : root) {
        if (!packageNameFromJson(item).empty()) {
            anyPackages = true;
            break;
        }
    }
    if (!anyPackages) {
        return true;
    }

    // Non-Debian platforms (Fedora, MacOS, ...) have no apt-get; there is
    // nothing we can replay. Consume the trigger rather than retrying forever.
    if (!FileExists("/usr/bin/apt-get")) {
        printf("apt-get not available on this platform; skipping user package install\n");
        return true;
    }

    // Refresh the package lists first. A freshly flashed OS may ship stale/empty
    // lists, and this can run at boot before the network is fully up, so retry a
    // few times before giving up.
    bool updated = false;
    for (int i = 1; i <= 3; i++) {
        if (runAptGet({ "-o", "DPkg::Lock::Timeout=60", "update" })) {
            updated = true;
            break;
        }
        printf("Warning: apt-get update failed (attempt %d), retrying...\n", i);
        sleep(5);
    }
    if (!updated) {
        printf("Error: apt-get update never succeeded - will retry user packages on next boot\n");
        return false;
    }

    bool allOk = true;
    for (const auto& item : root) {
        std::string pkg = packageNameFromJson(item);
        if (!pkg.empty()) {
            printf("Installing: %s\n", pkg.c_str());
            if (!runAptGet({ "-o", "DPkg::Lock::Timeout=60", "install", "-y", pkg })) {
                printf("Warning: Package installation failed for %s\n", pkg.c_str());
                allOk = false;
            }
        }
    }
    return allOk;
}

// A plugin is considered "installed" if its directory contains a pluginInfo.json
// manifest (same rule the web layer's GetInstalledPlugins() uses). Used to decide
// whether the post-FPPOS-upgrade "plugins must be reinstalled" warning applies.
static bool anyPluginsInstalled() {
    std::string dir = FPP_MEDIA_DIR + "/plugins";
    if (!DirectoryExists(dir)) {
        return false;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_directory() && FileExists(entry.path().string() + "/pluginInfo.json")) {
            return true;
        }
    }
    return false;
}

// FPP 10 changed the default MultiSync send method from Multicast to
// Unicast-to-known-remotes for fresh installs (see www/settings.json's
// MultiSyncUnicast "default": 1). An FPPOS reflash replaces /opt/fpp
// (settings.json included) but preserves /home/fpp/media/settings from the
// prior install untouched -- so an existing multisync user who never
// explicitly recorded a send method (multicast was implicit/default before
// unicast existed) would otherwise silently flip to unicast the moment this
// new code boots. Pin them to their historical multicast behavior instead.
// (Fresh installs and normal in-place `fpp upgrade`s are handled separately,
// by upgrade/122/upgrade.sh via the version-gated upgrade_config path.)
static void migrateMultiSyncDefaultToMulticast() {
    if (getRawSettingInt("MultiSyncEnabled", 0) != 1) {
        return;
    }

    // No send method recorded at all -> the user was relying on the pre-FPP-10
    // implicit multicast behavior.  Make it explicit.
    if (getRawSettingInt("MultiSyncMulticast", 0) != 1 &&
        getRawSettingInt("MultiSyncBroadcast", 0) != 1 &&
        getRawSettingInt("MultiSyncUnicast", 0) != 1) {
        setRawSetting("MultiSyncMulticast", "1");
    }

    // Pin MultiSyncUnicast off whenever the key is simply ABSENT, independent of
    // what the other two are set to.  This must not be folded into the branch
    // above: a user who had explicitly chosen multicast or broadcast skips that
    // branch entirely, so the key stayed absent -- and an absent key is not
    // neutral.  It resolves differently depending on who is asking:
    //
    //   www/common.php IfSettingEqualPrint() -> settings.json "default": 1
    //   src/settings.cpp LoadSettingsInfo()  -> settings.json "default": 1
    //   getRawSettingInt() (right here)      -> 0, the file is read directly
    //
    // So an upgraded box would show "Send MultiSync to ALL KNOWN remotes via
    // Unicast" ticked in the UI *and* have fppd genuinely unicast to every
    // remote, on top of whatever method the user actually chose -- while this
    // migration, reading raw, saw nothing wrong.  Writing the key makes all
    // three readers agree.
    //
    // Only absence is corrected: an explicit 0 or 1 is the user's own choice.
    std::string existing;
    if (!getRawSetting("MultiSyncUnicast", existing)) {
        setRawSetting("MultiSyncUnicast", "0");
    }
}

// Config-state migrations that must survive an FPPOS reflash, gated on the
// same /fppos_upgraded marker as checkInstallPackages() (touched by
// upgradeOS-part2.sh, which is always sourced from the target image being
// flashed -- see the note in upgradeOS-part1.sh -- so this runs with current
// code regardless of how old the box being upgraded was). Unlike
// checkInstallPackages(), this does NOT unlink /fppos_upgraded itself: package
// installs may still be retrying (e.g. no network yet), and each migration
// here is written to be safely idempotent, so simply re-running on a later
// boot is harmless. Add future OS-upgrade-surviving config migrations here.
void checkConfigMigrations() {
    if (FileExists("/fppos_upgraded")) {
        migrateMultiSyncDefaultToMulticast();
    }
}

void checkInstallPackages() {
    if (FileExists("/fppos_upgraded")) {
        // An FPPOS reflash replaces /opt/fpp (new fppd, headers, plugin API
        // version) while /home/fpp/media -- including the plugin clones -- is
        // preserved. Native (.so) plugins built against the old API will be
        // rejected at dlopen, and any rootfs artifacts a plugin's fpp_install.sh
        // dropped are gone. If plugins are present, set a persisted flag so fppd
        // can raise a (non-dismissible) warning prompting the user to Reinstall
        // All. The Plugin Manager clears the flag once a reinstall succeeds. Set
        // independent of the apt result below; a retry next boot just rewrites the
        // same value.
        if (anyPluginsInstalled()) {
            setRawSetting("pluginReinstallNeededAfterOS", "1");
        }

        printf("Installing User Packages\n");
        // Only consume the upgrade marker once the packages actually installed.
        // If the install can't complete (e.g. no network yet at boot), leave
        // /fppos_upgraded in place so it is retried on the next boot rather than
        // silently never reinstalling the user's packages.
        if (installPackagesFromJson("/home/fpp/media/config/userpackages.json")) {
            unlink("/fppos_upgraded");
        } else {
            printf("User package install incomplete - leaving /fppos_upgraded set to retry next boot\n");
        }
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

#ifdef PLATFORM_PI
// Shrink an oversized vc4-KMS CMA pool on boards that cannot afford it.
//
// The image ships "dtoverlay=vc4-kms-v3d,cma-256" for every Pi (FPP_Install.sh
// adds it to cap the Pi4's 512MB firmware default -- issue #2679). But the
// overlay's own README documents cma-256, and everything above cma-128, as
// "needs 1GB", and the firmware does NOT clamp it: a 512MB Pi Zero 2 W really
// comes up with CmaTotal 262144 kB. That is half the board handed to a GPU pool
// on a headless controller, and it is enough to break a source rebuild -- it
// leaves ~163MB usable, well under what cc1plus needs.
//
// So cap at cma-128, the largest size the overlay does not gate behind 1GB.
// FPP's real DMA needs are far below that: the DPI envelope declared below is
// 1920x997 RGB888, about 7.6MB.
//
// Only ever REDUCE: a deliberate smaller override (cma-64) is left alone, since
// raising it would defeat the purpose. Rewrites `content` in place and sets
// `changed` so the caller's existing reboot handles it; comparing before writing
// keeps it idempotent, so it fires once and never loops.
// MemTotal counts the CMA reservation, so it is the board's usable total, not
// its marketing size: a 512MB board reports ~425-500MB, a 1GB board ~900MB+.
// 700MB separates them with room on both sides.
static constexpr long CMA_ONE_GB_BOARD_KB = 700000;
static constexpr int CMA_MAX_UNDER_1GB = 128;

// Pure half: no file I/O, so it can be lifted out between the markers below and
// exercised directly against sample config.txt text. Edits `content` in place
// and returns true if it changed anything.
// --- BEGIN capVC4CMALine ---
static bool capVC4CMALine(std::string& content, long memKB) {
    if (memKB <= 0 || memKB >= CMA_ONE_GB_BOARD_KB) {
        return false;
    }
    // Match the overlay line itself, never a commented-out or indented copy.
    // Covers both the generic overlay and the -pi5 variant.
    size_t vidx = content.find("dtoverlay=vc4-kms-v3d");
    while (vidx != std::string::npos && vidx != 0 && content[vidx - 1] != '\n') {
        vidx = content.find("dtoverlay=vc4-kms-v3d", vidx + 1);
    }
    if (vidx == std::string::npos) {
        return false;
    }
    size_t eol = content.find("\n", vidx);
    if (eol == std::string::npos) {
        eol = content.size();
    }
    std::string line = content.substr(vidx, eol - vidx);

    size_t cidx = line.find("cma-");
    if (cidx == std::string::npos) {
        return false;
    }
    size_t dstart = cidx + strlen("cma-");
    size_t dend = dstart;
    while (dend < line.size() && isdigit(static_cast<unsigned char>(line[dend]))) {
        dend++;
    }
    if (dend == dstart) {
        // "cma-size" (a raw byte count) or some other spelling -- leave it be.
        return false;
    }
    int current = static_cast<int>(strtol(line.c_str() + dstart, nullptr, 10));
    if (current <= CMA_MAX_UNDER_1GB) {
        return false;
    }
    line.replace(cidx, dend - cidx, "cma-" + std::to_string(CMA_MAX_UNDER_1GB));
    printf("FPP - %ldkB board: capping vc4 CMA at %dMB (was %dMB)\n",
           memKB, CMA_MAX_UNDER_1GB, current);
    content.replace(vidx, eol - vidx, line);
    return true;
}
// --- END capVC4CMALine ---

static void capVC4CMAForBoard(std::string& content, bool& changed) {
    std::string meminfo = GetFileContents("/proc/meminfo");
    size_t midx = meminfo.find("MemTotal:");
    if (midx == std::string::npos) {
        return;
    }
    long memKB = strtol(meminfo.c_str() + midx + strlen("MemTotal:"), nullptr, 10);
    if (capVC4CMALine(content, memKB)) {
        PutFileContents("/boot/firmware/config.txt", content);
        changed = true;
    }
}
#endif

void setupChannelOutputs() {
#ifdef PLATFORM_PI
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
            bool migrated = false;
            for (int x = 0; x < v["channelOutputs"].size(); x++) {
                if (v["channelOutputs"][x]["type"].asString() == "RPIWS281X") {
                    // The RPIWS281X driver has been removed.  DPI drives the same pins without
                    // conflicting with the onboard audio, so migrate the config to DPIPixels.
                    // Capes we don't have a mapping for keep their subType; DPIPixels resolves
                    // the matching string config at runtime.
                    std::string subType = v["channelOutputs"][x]["subType"].asString();
                    v["channelOutputs"][x]["type"] = "DPIPixels";
                    if (subType == "PiHat") {
                        v["channelOutputs"][x]["subType"] = "PiHat-DPIPixels";
                    } else if (subType == "rPi-MFC") {
                        v["channelOutputs"][x]["subType"] = "rPi-MFC-DPIPixels";
                    } else if (subType == "rPi-28D") {
                        // The rPi-28D's third output was ws2801 over SPI which DPI cannot drive.
                        // The 4 output variant drives CN1's clock and data pins as WS281x instead.
                        v["channelOutputs"][x]["subType"] = "rPi-28D-DPIPixels-4";
                        for (int o = 0; o < v["channelOutputs"][x]["outputs"].size(); o++) {
                            if (v["channelOutputs"][x]["outputs"][o]["protocol"].asString() == "ws2801") {
                                v["channelOutputs"][x]["outputs"][o]["protocol"] = "ws2811";
                            }
                        }
                    }
                    migrated = true;
                }
                if (v["channelOutputs"][x]["type"].asString() == "DPIPixels" && v["channelOutputs"][x]["enabled"].asInt() == 1) {
                    hasDPI = true;
                }
            }
            if (migrated) {
                PutFileContents("/home/fpp/media/config/co-pixelStrings.json", SaveJsonToString(v));
            }
        }
    }
    bool changed = false;
    // Only the RGBMatrix output still needs the onboard audio out of the way.  Pixel strings
    // are driven by DPI now, which coexists with snd_bcm2835.
    if (hasRPIMatrix) {
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
    capVC4CMAForBoard(content, changed);
    size_t idx = content.find("dtoverlay=vc4-kms-dpi-fpp");
    std::string origLine = "";
    if (idx != std::string::npos) {
        size_t idx2 = content.find("\n", idx);
        origLine = content.substr(idx, idx2 - idx);
    }
    if (hasDPI) {
        // The DPI overlay only bootstraps the 1920-wide, 38.4MHz pipeline; its
        // vertical resolution is just the boot/default mode.  DPIPixelsOutput
        // overrides the vertical timing at runtime (sizing vactive to the longest
        // string and setting the frame rate purely via the vertical blanking, no
        // reboot).  Declare the tallest envelope we support - vactive=997 (the old
        // 20fps mode, the lowest rate / longest string) - so every runtime mode is
        // a subset of what the overlay advertises.
        std::string width = "1920";
        std::string height = "997";
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

// Drive a GPIO line, looked up by the name the kernel exposes for it
// (gpio-line-names in the device tree), to the given value.  Uses libgpiod
// directly rather than the gpioset/gpioinfo tools whose command line and
// output format changed incompatibly between libgpiod 1.x and 2.x.
//
// Like gpioset, the line is released as soon as the value has been written:
// the pin keeps the level the driver last wrote to it.
static bool setNamedGPIO(const std::string& pin, int val) {
#ifdef HASGPIOD
    bool found = false;
    for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
        if (!entry.path().filename().string().starts_with("gpiochip")) {
            continue;
        }
        std::string path = entry.path().string();
        try {
#ifdef IS_GPIOD_CXX_V2
            gpiod::chip chip(path);
            int offset = chip.get_line_offset_from_name(pin);
            if (offset < 0) {
                continue;
            }
            gpiod::line_settings settings;
            settings.set_direction(gpiod::line::direction::OUTPUT);
            settings.set_output_value(val ? gpiod::line::value::ACTIVE : gpiod::line::value::INACTIVE);
            gpiod::line_request request = chip.prepare_request()
                                              .set_consumer("FPPINIT")
                                              .add_line_settings(offset, settings)
                                              .do_request();
            request.release();
#else
            gpiod::chip chip(path, gpiod::chip::OPEN_BY_PATH);
            gpiod::line line = chip.find_line(pin);
            if (!line) {
                continue;
            }
            int offset = line.offset();
            gpiod::line_request request;
            request.consumer = "FPPINIT";
            request.request_type = gpiod::line_request::DIRECTION_OUTPUT;
            line.request(request, val);
            line.release();
#endif
            printf("FPP - Set GPIO %s (%s line %d) to %d\n", pin.c_str(), path.c_str(), offset, val);
            found = true;
        } catch (const std::exception& ex) {
            printf("FPP - Could not set GPIO %s on %s: %s\n", pin.c_str(), path.c_str(), ex.what());
        }
    }
    if (!found) {
        printf("FPP - Could not find a GPIO line named %s\n", pin.c_str());
    }
    return found;
#else
    printf("FPP - Cannot set GPIO %s, no libgpiod support\n", pin.c_str());
    return false;
#endif
}

void handleRebootActions() {
    if (FileExists("/home/fpp/media/tmp/cape-info.json")) {
        Json::Value v;
        if (LoadJsonFromString(GetFileContents("/home/fpp/media/tmp/cape-info.json"), v)) {
            if (v.isMember("rebootActions")) {
                for (const auto& action : v["rebootActions"]) {
                    if (action["type"].asString() == "gpio") {
                        std::string pin = action["pin"].asString();
                        if (pin.empty()) {
                            continue;
                        }
                        int val = 0;
                        if (pin[0] == '+') {
                            pin = pin.substr(1);
                            val = 1;
                        }
                        setNamedGPIO(pin, val);
                    }
                }
            }
        }
    }
}
