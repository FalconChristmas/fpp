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
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

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

void checkFSTAB() {
    std::string cont = GetFileContents("/etc/fstab");
    if (cont.find("home/fpp/media") == std::string::npos) {
        cont += "\n";
        cont += "#####################################\n";
        cont += "#/dev/sda1     /home/fpp/media  auto    defaults,noatime,nodiratime,exec,nofail,flush,uid=500,gid=500  0  0\n";
        cont += "#####################################\n";
        PutFileContents("/etc/fstab", cont);
    }
}

void createDirectories() {
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
        if (item.isString()) {
            anyPackages = true;
            break;
        }
    }
    if (!anyPackages) {
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
        if (item.isString()) {
            printf("Installing: %s\n", item.asString().c_str());
            if (!runAptGet({ "-o", "DPkg::Lock::Timeout=60", "install", "-y", item.asString() })) {
                printf("Warning: Package installation failed for %s\n", item.asString().c_str());
                allOk = false;
            }
        }
    }
    return allOk;
}

void checkInstallPackages() {
    if (FileExists("/fppos_upgraded")) {
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

void setupChannelOutputs() {
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

void handleRebootActions() {
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
