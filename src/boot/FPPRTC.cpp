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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include <array>
#include <iomanip>
#include <thread>

#include "common_mini.h"

static int getSettingInt() {
    if (!FileExists("/home/fpp/media/settings")) {
        return 0;
    }
    std::string c = GetFileContents("/home/fpp/media/settings");
    size_t idx = c.find("piRTC = ");
    if (idx != std::string::npos) {
        uint8_t ch = c[idx + 9] - '0';
        uint8_t ch2 = c[idx + 10];
        if (ch2 != '\"' && ch2 != '\n' && ch2 != '\r') {
            ch *= 10;
            ch += ch2 - '0';
        }
        return ch;
    }
    return 0;
}

static int getBus() {
#if defined(PLATFORM_BBB) || defined(PLATFORM_BB64)
    return 2;
#elif defined(PLATFORM_PI)
    return 1;
#else
    return 0;
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
static std::string hwclock(const char* action, const char* device) {
    std::string cmd = std::string("/usr/sbin/hwclock ") + action + " -f " + device + " 2>&1";
    return execAndReturn(cmd);
}
static void addI2CDevice(const char* dev, int bus) {
    char buf[256];
    snprintf(buf, sizeof(buf), "/sys/bus/i2c/devices/i2c-%d/new_device", bus);
    FILE* fd = fopen(buf, "w");
    if (fd != nullptr) {
        fwrite(dev, strlen(dev), 1, fd);
        fclose(fd);
    }
}
static void removeI2CDevice(const std::string& dev, int bus) {
    if (!dev.empty()) {
        printf("Failed to setup RTC, removing device\n");
        char buf[256];
        snprintf(buf, sizeof(buf), "/sys/bus/i2c/devices/i2c-%d/delete_device", bus);
        FILE* fd = fopen(buf, "w");
        if (fd != nullptr) {
            fwrite(dev.c_str(), dev.size(), 1, fd);
            fclose(fd);
        }
    }
}

static bool setupRTC(std::string& devAd, bool newDev) {
    int ledType = getSettingInt();

    int bus = getBus();
    switch (ledType) {
    case 1:
        printf("FPP - Configuring RTC, Setting to rasClock/%d\n", bus);
        modprobe("rtc-pcf2127");
        devAd = "0x51";
        if (newDev) {
            addI2CDevice("pcf2127 0x51", bus);
        }
        break;
    case 2:
        printf("FPP - Configuring RTC, Setting to DS1307/%d\n", bus);
        modprobe("rtc-ds1307");
        devAd = "0x68";
        if (newDev) {
            addI2CDevice("ds1307 0x68", bus);
        }
        break;
    case 3:
        printf("FPP - Configuring RTC, Setting to mcp7941x/%d\n", bus);
        // modprobe("");
        devAd = "0x6f";
        if (newDev) {
            addI2CDevice("mcp7941x 0x6f", bus);
        }
        break;
    case 4:
        printf("FPP - Configuring RTC, Setting to pcf8523/%d\n", bus);
        modprobe("rtc-pcf8523");
        devAd = "0x68";
        if (newDev) {
            addI2CDevice("pcf8523 0x68", bus);
        }
        break;
    case 5:
        printf("FPP - Configuring RTC, Setting to pcf85363/%d\n", bus);
        modprobe("rtc-pcf85363");
        devAd = "0x51";
        if (newDev) {
            addI2CDevice("pcf85363 0x51", bus);
        }
        break;
    default:
        printf("FPP - Configuring RTC, None Selected");
        return false;
    }
    return true;
}
static std::string getRTCDev() {
    std::string rtc = "/dev/rtc0";

#ifdef PLATFORM_BBB
    if (FileExists("/sys/class/rtc/rtc0/name")) {
        std::string drv = GetFileContents("/sys/class/rtc/rtc0/name");
        if (contains(drv, "omap_rtc")) {
            // built in OMAP RTC, (not battery backed, kind of useless)
            rtc = "/dev/rtc1";
        } else if (contains("drv", " 0-00")) {
            // Beaglebone Green Gateway has a ds1307 on i2c0
            // but no battery (by default) so cape would
            // be rtc1 and that's what we should use
            rtc = "/dev/rtc1";
        }
    }
#elif defined(PLATFORM_BB64)
    if (FileExists("/sys/class/rtc/rtc1/name")) {
        std::string drv = GetFileContents("/sys/class/rtc/rtc1/name");
        if (!contains(drv, "rtc-ti-k3")) {
            rtc = "/dev/rtc1";
        }
    }
#elif defined(PLATFORM_PI)
    if (FileExists("/sys/class/rtc/rtc0/name")) {
        std::string drv = GetFileContents("/sys/class/rtc/rtc0/name");
        if (contains(drv, "soc:rpi_rtc")) {
            // Raspberry Pi 5 RTC, we are configuring the cape clock which would be rtc1
            rtc = "/dev/rtc1";
        }
    }
#endif
    return rtc;
}

int main(int argc, char* argv[]) {
    std::string rtc = getRTCDev();
    std::string dev;
    if (!setupRTC(dev, !FileExists(rtc))) {
        return 0;
    }
    int count = 0;
    while (!FileExists(rtc) && count < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (FileExists(rtc)) {
        std::string action = "get";
        if (argc > 1) {
            action = argv[1];
        }
        if (action == "set") {
            hwclock("-w", rtc.c_str());
        } else {
            std::string ret = hwclock("-s", rtc.c_str());
            // printf("%s\n", ret.c_str());
            if (contains(ret, "Remote I/O error")) {
                removeI2CDevice(dev, getBus());
            } else {
#if defined(PLATFORM_BBB) || defined(PLATFORMBB64)
                // set the built in rtc to the same time as read from the RTC
                if (rtc != "/dev/rtc0") {
                    hwclock("-w", rtc.c_str());
                }
#endif
                exec("cp /opt/fpp/etc/update-RTC /etc/cron.daily/");
            }
        }
    } else if (!dev.empty()) {
        removeI2CDevice(dev, getBus());
    }
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::stringstream sstr;
    sstr << std::put_time(&tm, "%a %b %d %H:%M:%S %Z %Y");
    std::string str = sstr.str();
    printf("%s\n", str.c_str());
    return 0;
}