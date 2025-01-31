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

#include "BBBUtils.h"
#include <sys/mman.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include "../common.h"

static BeagleBoneType BBB_TYPE = Unknown;
BeagleBoneType getBeagleBoneType() {
    if (BBB_TYPE == Unknown) {
        FILE* file = fopen("/proc/device-tree/model", "r");
        if (file) {
            char buf[256];
            fgets(buf, 256, file);
            fclose(file);
            if (strcmp(buf, "SanCloud BeagleBone Enhanced") == 0) {
                BBB_TYPE = SanCloudEnhanced;
            } else if (strcmp(&buf[10], "PocketBeagle") == 0) {
                BBB_TYPE = PocketBeagle;
            } else if (strcmp(buf, "BeagleBoard.org PocketBeagle2") == 0) {
                BBB_TYPE = PocketBeagle;
            } else if (strcmp(buf, "BeagleBoard.org BeaglePlay") == 0) {
                BBB_TYPE = BeaglePlay;
            } else if (strcmp(&buf[10], "BeagleBone Black Wireless") == 0) {
                BBB_TYPE = BlackWireless;
            } else if (strcmp(&buf[10], "BeagleBone Green Wireless") == 0) {
                BBB_TYPE = GreenWireless;
            } else if (strcmp(&buf[10], "BeagleBone Green") == 0) {
                BBB_TYPE = Green;
            } else {
                BBB_TYPE = Black;
            }
        } else {
            BBB_TYPE = Black;
        }
    }
    return BBB_TYPE;
}

static std::vector<BBBPinCapabilities> BBB_PINS;

// 1MHz frequence
// #define HZ 1000000.0f
#define HZ 10000.0f

// static uint32_t bbGPIOMap[] = { 3, 0, 1, 2 };
static uint32_t bbGPIOMap[] = { 0, 0, 0, 0 };
static uint32_t bbGPIOStart[] = { 0, 0, 0, 0 };
static const char* bbbPWMDeviceName = "/sys/class/pwm/pwmchip";
static FILE* bbbPWMDutyFiles[] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

static volatile bool registersMemMapped = false;

static int getBBBPWMChipNum(int pwm) {
    static const char* bbbPWMChipNums[] = { "48300200", "48302200", "48304200" };
    char buf[256];
    for (int x = 0; x < 10; x++) {
        snprintf(buf, sizeof(buf), "/sys/bus/platform/drivers/ehrpwm/%s.pwm/pwm/pwmchip%d", bbbPWMChipNums[pwm], x);
        if (access(buf, F_OK) == 0) {
            return x;
        }
    }
    return 0;
}

static void setupBBBMemoryMap() {
    if (registersMemMapped) {
        return;
    }

    std::map<std::string, std::pair<int, int>> labelMapping;
    // newer kernels will number the chips differently so we'll use the
    // memory locations to figure out the mapping to the gpio0-3 that we use
    const std::string dirName("/sys/class/gpio");
    DIR* dir = opendir(dirName.c_str());
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string fn = entry->d_name;
        if (fn.starts_with("gpiochip")) {
            std::string fn2 = dirName + "/" + fn;
            char buf[PATH_MAX + 1];
            realpath(fn2.c_str(), buf);

            std::string lab = GetFileContents(fn2 + "/label");
            TrimWhiteSpace(lab);
            std::string target = buf;
            int start = std::atoi(fn.substr(8).c_str());
#ifdef PLATFORM_BB64
            if (lab.contains("tps65219-gpio")) {
                labelMapping[lab] = { 0, start };
            } else if (lab.contains("4201000.gpio")) {
                labelMapping[lab] = { 1, start };
            } else if (lab.contains("600000.gpio")) {
                labelMapping[lab] = { 2, start };
            } else if (lab.contains("601000.gpio")) {
                labelMapping[lab] = { 3, start };
            }
#else
            if (target.contains("44e07000.gpio")) {
                labelMapping[lab] = { 0, start };
            } else if (target.contains("4804c000.gpio")) {
                labelMapping[lab] = { 1, start };
            } else if (target.contains("481ac000.gpio")) {
                labelMapping[lab] = { 2, start };
            } else if (target.contains("481ae000.gpio")) {
                labelMapping[lab] = { 3, start };
            }
#endif
        }
    }
    closedir(dir);

    int curChip = 0;
    for (auto& a : gpiod::make_chip_iter()) {
        // std::string cname = a.name();
        std::string clabel = a.label();
        TrimWhiteSpace(clabel);
        if (labelMapping.contains(clabel)) {
            bbGPIOMap[labelMapping[clabel].first] = curChip;
            bbGPIOStart[labelMapping[clabel].first] = labelMapping[clabel].second;
        } else {
#ifdef PLATFORM_BBB
            // if mapping by base fails, try using specific line labels
            auto line = a.get_line(0);
            if (line.name() == "NC") {
                line.release();
                line = a.get_line(13);
            }
            std::string name = line.name();
            if (name == "[mdio_data]" || name == "P1.28 [I2C2_SCL]") {
                bbGPIOMap[0] = curChip;
            } else if (name == "P8_25 [mmc1_dat0]" || name == "P2.33") {
                bbGPIOMap[1] = curChip;
            } else if (name == "P9_15B" || name == "[SYSBOOT 7]" || name == "P2.20") {
                bbGPIOMap[2] = curChip;
            } else if (name == "[mii col]" || name == "P1.03 [USB1]") {
                bbGPIOMap[3] = curChip;
            }
            line.release();
#endif
        }
        ++curChip;
        if (curChip == 4) {
            break;
        }
    }
    // printf("Mapping %d %d %d %d\n", bbGPIOMap[0], bbGPIOMap[1], bbGPIOMap[2], bbGPIOMap[3]);
    registersMemMapped = 1;
}

// ------------------------------------------------------------------------

BBBPinCapabilities::BBBPinCapabilities(const std::string& n, uint32_t k) :
    GPIODCapabilities(n, k), pru(-1), pruPin(-1) {
    setupBBBMemoryMap();
    gpioIdx = bbGPIOMap[k / 32];
    gpio = k % 32;
}
BBBPinCapabilities::BBBPinCapabilities(const std::string& n, uint32_t g, uint32_t o) :
    GPIODCapabilities(n, g), pru(-1), pruPin(-1) {
    setupBBBMemoryMap();
    gpioIdx = bbGPIOMap[g];
    gpio = o;
    kernelGpio = bbGPIOStart[g] + o;
}

#ifdef PLATFORM_BB64
int BBBPinCapabilities::mappedGPIOIdx() const {
    int mp = 0;
    for (int x = 0; x < 4; x++) {
        if (bbGPIOMap[x] == gpioIdx) {
            mp = x;
        }
    }
    if (mp == 1) {
        // MCU Domain
        return 0;
    }
    if (mp == 3) {
        // gpio1, all pins are in the first 32
        return 3;
    }
    // gpio0, map 32-63 to 1 and 64-95 to 2
    if (gpio < 64) {
        return 1;
    }
    return 2;
}
int BBBPinCapabilities::mappedGPIO() const {
    int mp = 0;
    for (int x = 0; x < 4; x++) {
        if (bbGPIOMap[x] == gpioIdx) {
            mp = x;
        }
    }
    if (mp == 1) {
        // MCU Domain
        return gpio;
    }
    if (mp == 3) {
        // gpio1, all pins are in the first 32
        return gpio;
    }
    // gpio0, map 32-63 to 1 and 64-95 to 2
    if (gpio < 64) {
        return gpio - 32;
    }
    return gpio - 64;
}
#else
int BBBPinCapabilities::mappedGPIOIdx() const {
    for (int x = 0; x < 4; x++) {
        if (bbGPIOMap[x] == gpioIdx) {
            return x;
        }
    }
    return gpioIdx;
}
int BBBPinCapabilities::mappedGPIO() const {
    return gpio;
}
#endif

Json::Value BBBPinCapabilities::toJSON() const {
    Json::Value ret = PinCapabilities::toJSON();
    if (pru != -1) {
        ret["pru"] = pru;
        ret["pruPin"] = pruPin;
    }
    return ret;
}

bool BBBPinCapabilities::supportPWM() const {
    return pwm != -1;
}

int BBBPinCapabilities::configPin(const std::string& m,
                                  bool directionOut) const {
    std::string mode = m;
    bool enableI2C = mode == "i2c";
    if (i2cBus >= 0 && !enableI2C) {
        enableOledScreen(i2cBus, false);
    }

    char pinName[16];
    char dir_name[256];
    strcpy(pinName, name.c_str());
    if (pinName[2] == '-') {
        pinName[2] = '_';
    }

    if (FileExists("/usr/bin/pinctrl")) {
        char buf[256];
        std::string pm = m;
        if (m == "gpio" && directionOut) {
            pm = "gpio_out";
        }
        snprintf(buf, 256, "/usr/bin/pinctrl -s %s %s", pinName, m.c_str());
        // printf("%s\n", buf);
        system(buf);
    }

    snprintf(dir_name, sizeof(dir_name),
             "/sys/devices/platform/ocp/ocp:%s_pinmux/state",
             pinName);
    FILE* dir = fopen(dir_name, "w");
    if (dir) {
        fprintf(dir, "%s\n", mode.c_str());
        fclose(dir);
    }

    if (mode == "i2c") {
        // GPIODCapabilities::configPin("gpio", false);
    } else if (mode == "pwm") {
        // GPIODCapabilities::configPin("gpio", false);
    } else if (mode == "pruout") {
        GPIODCapabilities::configPin("gpio", false);
    } else if (mode == "pruin") {
        // GPIODCapabilities::configPin("gpio", false);
    } else if (mode == "uart") {
        // GPIODCapabilities::configPin("gpio", false);
    } else {
        // gpio
        GPIODCapabilities::configPin(m, directionOut);
    }
    if (i2cBus >= 0 && enableI2C) {
        enableOledScreen(i2cBus, true);
    }
    return 0;
}

bool BBBPinCapabilities::setupPWM(int maxValue) const {
    if (pwm != -1 && (bbbPWMDutyFiles[pwm * 2 + subPwm] == nullptr)) {
        // set pin
        setupBBBMemoryMap();

        configPin("pwm");
        int chipNum = getBBBPWMChipNum(pwm);
        char dir_name[128];
        snprintf(dir_name, sizeof(dir_name), "%s%d/export", bbbPWMDeviceName, chipNum);
        FILE* dir = fopen(dir_name, "w");
        fprintf(dir, "%d", subPwm);
        fclose(dir);

        snprintf(dir_name, sizeof(dir_name), "%s%d/pwm%d/period",
                 bbbPWMDeviceName, chipNum, subPwm);
        dir = fopen(dir_name, "w");
        fprintf(dir, "%d", maxValue);
        fclose(dir);

        snprintf(dir_name, sizeof(dir_name), "%s%d/pwm%d/duty_cycle",
                 bbbPWMDeviceName, chipNum, subPwm);
        dir = fopen(dir_name, "w");
        fprintf(dir, "0");
        fflush(dir);
        bbbPWMDutyFiles[pwm * 2 + subPwm] = dir;

        snprintf(dir_name, sizeof(dir_name), "%s%d/pwm%d/enable",
                 bbbPWMDeviceName, chipNum, subPwm);
        dir = fopen(dir_name, "w");
        fprintf(dir, "1");
        fclose(dir);
    }
    return false;
}

void BBBPinCapabilities::setPWMValue(int value) const {
    if (pwm != -1) {
        setupBBBMemoryMap();
        fprintf(bbbPWMDutyFiles[pwm * 2 + subPwm], "%d", value);
        fflush(bbbPWMDutyFiles[pwm * 2 + subPwm]);
    }
}
// PWM locations
#define PWMSS0_BASE 0x48300000
#define PWMSS1_BASE 0x48302000
#define PWMSS2_BASE 0x48304000

int BBBPinCapabilities::getPWMRegisterAddress() const {
    // need to add 0x200 to the base address to get the
    // address to the eHRPWM register which is what we really use
    if (pwm == 0) {
        return PWMSS0_BASE + 0x200;
    } else if (pwm == 1) {
        return PWMSS1_BASE + 0x200;
    } else if (pwm == 2) {
        return PWMSS2_BASE + 0x200;
    }
    return 0;
}

class NullBBBPinCapabilities : public BBBPinCapabilities {
public:
    NullBBBPinCapabilities() :
        BBBPinCapabilities("-none-", 0, 0) {}
    virtual const PinCapabilities* ptr() const override { return nullptr; }
};
static NullBBBPinCapabilities NULL_BBB_INSTANCE;

const PinCapabilities& BBBPinProvider::getPinByName(const std::string& name) {
    for (auto& a : BBB_PINS) {
        if (a.name == name) {
            return a;
        }
    }
    return NULL_BBB_INSTANCE;
}

const PinCapabilities& BBBPinProvider::getPinByGPIO(int i) {
    for (auto& a : BBB_PINS) {
        if (a.kernelGpio == i) {
            return a;
        }
    }
    return NULL_BBB_INSTANCE;
}

void BBBPinProvider::Init() {
    if (BBB_PINS.empty()) {
#ifdef PLATFORM_BBB
        if (getBeagleBoneType() == PocketBeagle) {
            BBB_PINS.emplace_back("P1-02", 87).setPRU(1, 9);
            BBB_PINS.emplace_back("P1-04", 89).setPRU(1, 11);
            BBB_PINS.emplace_back("P1-06", 5).setI2C(1);
            BBB_PINS.emplace_back("P1-08", 2).setPwm(0, 0).setUART("ttyS2-rx");
            BBB_PINS.emplace_back("P1-10", 3).setPwm(0, 1).setUART("ttyS2-tx");
            BBB_PINS.emplace_back("P1-12", 4).setI2C(1);
            BBB_PINS.emplace_back("P1-20", 20);
            BBB_PINS.emplace_back("P1-26", 12).setI2C(2);
            BBB_PINS.emplace_back("P1-28", 13).setI2C(2);
            BBB_PINS.emplace_back("P1-29", 117).setPRU(0, 7);
            BBB_PINS.emplace_back("P1-30", 43).setPRU(1, 15).setUART("ttyS0-tx");
            BBB_PINS.emplace_back("P1-31", 114).setPRU(0, 4);
            BBB_PINS.emplace_back("P1-32", 42).setPRU(1, 14).setUART("ttyS0-rx");
            BBB_PINS.emplace_back("P1-33", 111).setPRU(0, 1).setPwm(0, 1);
            BBB_PINS.emplace_back("P1-34", 26);
            BBB_PINS.emplace_back("P1-35", 88).setPRU(1, 10);
            BBB_PINS.emplace_back("P1-36", 110).setPRU(0, 0).setPwm(0, 0);
            BBB_PINS.emplace_back("P2-01", 50).setPwm(1, 0);
            BBB_PINS.emplace_back("P2-02", 59);
            BBB_PINS.emplace_back("P2-03", 23).setPwm(2, 1);
            BBB_PINS.emplace_back("P2-04", 58);
            BBB_PINS.emplace_back("P2-05", 30).setUART("ttyS4-rx");
            BBB_PINS.emplace_back("P2-06", 57);
            BBB_PINS.emplace_back("P2-07", 31).setUART("ttyS4-tx");
            BBB_PINS.emplace_back("P2-08", 60);
            BBB_PINS.emplace_back("P2-09", 15).setUART("ttyS1-tx");
            BBB_PINS.emplace_back("P2-10", 52);
            BBB_PINS.emplace_back("P2-11", 14).setUART("ttyS1-rx");
            BBB_PINS.emplace_back("P2-17", 65);
            BBB_PINS.emplace_back("P2-18", 47);
            BBB_PINS.emplace_back("P2-19", 27);
            BBB_PINS.emplace_back("P2-20", 64);
            BBB_PINS.emplace_back("P2-22", 46);
            BBB_PINS.emplace_back("P2-24", 44).setPRU(0, 14);
            BBB_PINS.emplace_back("P2-25", 41).setUART("ttyS4-tx");
            BBB_PINS.emplace_back("P2-27", 40).setUART("ttyS4-rx");
            BBB_PINS.emplace_back("P2-28", 116).setPRU(0, 6);
            BBB_PINS.emplace_back("P2-29", 7).setUART("ttyS3-tx");
            BBB_PINS.emplace_back("P2-30", 113).setPRU(0, 3);
            BBB_PINS.emplace_back("P2-31", 19);
            BBB_PINS.emplace_back("P2-32", 112).setPRU(0, 2);
            BBB_PINS.emplace_back("P2-33", 45).setPRU(0, 15);
            BBB_PINS.emplace_back("P2-34", 115).setPRU(0, 5);
            BBB_PINS.emplace_back("P2-35", 86).setPRU(1, 8);
        } else {
            BBB_PINS.emplace_back("P8-03", 38);
            BBB_PINS.emplace_back("P8-04", 39);
            BBB_PINS.emplace_back("P8-05", 34);
            BBB_PINS.emplace_back("P8-06", 35);
            BBB_PINS.emplace_back("P8-07", 66);
            BBB_PINS.emplace_back("P8-08", 67);
            BBB_PINS.emplace_back("P8-09", 69);
            BBB_PINS.emplace_back("P8-10", 68);
            BBB_PINS.emplace_back("P8-11", 45).setPRU(0, 15);
            BBB_PINS.emplace_back("P8-12", 44).setPRU(0, 14);
            BBB_PINS.emplace_back("P8-13", 23).setPwm(2, 1);
            BBB_PINS.emplace_back("P8-14", 26);
            BBB_PINS.emplace_back("P8-15", 47);
            BBB_PINS.emplace_back("P8-16", 46);
            BBB_PINS.emplace_back("P8-17", 27);
            BBB_PINS.emplace_back("P8-18", 65);
            BBB_PINS.emplace_back("P8-19", 22).setPwm(2, 0);
            BBB_PINS.emplace_back("P8-20", 63).setPRU(1, 13);
            BBB_PINS.emplace_back("P8-21", 62).setPRU(1, 12);
            BBB_PINS.emplace_back("P8-22", 37);
            BBB_PINS.emplace_back("P8-23", 36);
            BBB_PINS.emplace_back("P8-24", 33);
            BBB_PINS.emplace_back("P8-25", 32);
            BBB_PINS.emplace_back("P8-26", 61);
            BBB_PINS.emplace_back("P8-27", 86).setPRU(1, 8);
            BBB_PINS.emplace_back("P8-28", 88).setPRU(1, 10);
            BBB_PINS.emplace_back("P8-29", 87).setPRU(1, 9);
            BBB_PINS.emplace_back("P8-30", 89).setPRU(1, 11);
            BBB_PINS.emplace_back("P8-31", 10);
            BBB_PINS.emplace_back("P8-32", 11);
            BBB_PINS.emplace_back("P8-33", 9);
            BBB_PINS.emplace_back("P8-34", 81).setPwm(1, 1);
            BBB_PINS.emplace_back("P8-35", 8);
            BBB_PINS.emplace_back("P8-36", 80).setPwm(1, 0);
            BBB_PINS.emplace_back("P8-37", 78).setUART("ttyS5-tx");
            BBB_PINS.emplace_back("P8-38", 79).setUART("ttyS5-rx");
            BBB_PINS.emplace_back("P8-39", 76).setPRU(1, 6);
            BBB_PINS.emplace_back("P8-40", 77).setPRU(1, 7);
            BBB_PINS.emplace_back("P8-41", 74).setPRU(1, 4);
            BBB_PINS.emplace_back("P8-42", 75).setPRU(1, 5);
            BBB_PINS.emplace_back("P8-43", 72).setPRU(1, 2);
            BBB_PINS.emplace_back("P8-44", 73).setPRU(1, 3);
            BBB_PINS.emplace_back("P8-45", 70).setPRU(1, 0).setPwm(2, 0);
            BBB_PINS.emplace_back("P8-46", 71).setPRU(1, 1).setPwm(2, 1);
            BBB_PINS.emplace_back("P9-11", 30).setUART("ttyS4-rx");
            BBB_PINS.emplace_back("P9-12", 60);
            BBB_PINS.emplace_back("P9-13", 31).setUART("ttyS4-tx");
            BBB_PINS.emplace_back("P9-14", 50).setPwm(1, 0);
            BBB_PINS.emplace_back("P9-15", 48);
            BBB_PINS.emplace_back("P9-16", 51).setPwm(1, 1);
            BBB_PINS.emplace_back("P9-17", 5).setI2C(1);
            BBB_PINS.emplace_back("P9-18", 4).setI2C(1);
            BBB_PINS.emplace_back("P9-19", 13).setI2C(2);
            BBB_PINS.emplace_back("P9-20", 12).setI2C(2);
            BBB_PINS.emplace_back("P9-21", 3).setPwm(0, 1).setUART("ttyS2-tx");
            BBB_PINS.emplace_back("P9-22", 2).setPwm(0, 0).setUART("ttyS2-rx");
            BBB_PINS.emplace_back("P9-23", 49);
            BBB_PINS.emplace_back("P9-24", 15).setUART("ttyS1-tx");
            BBB_PINS.emplace_back("P9-25", 117).setPRU(0, 7);
            BBB_PINS.emplace_back("P9-26", 14).setUART("ttyS1-rx");
            BBB_PINS.emplace_back("P9-27", 115).setPRU(0, 5);
            BBB_PINS.emplace_back("P9-28", 113).setPRU(0, 3);
            BBB_PINS.emplace_back("P9-29", 111).setPRU(0, 1).setPwm(0, 1);
            BBB_PINS.emplace_back("P9-30", 112).setPRU(0, 2);
            BBB_PINS.emplace_back("P9-31", 110).setPRU(0, 0).setPwm(0, 0);
            BBB_PINS.emplace_back("P9-41", 20);
            BBB_PINS.emplace_back("P9-91", 116).setPRU(0, 6);
            BBB_PINS.emplace_back("P9-42", 7).setUART("ttyS3-tx");
            BBB_PINS.emplace_back("P9-92", 114).setPRU(0, 4);
        }
#endif
#ifdef PLATFORM_BB64
        BeagleBoneType tp = getBeagleBoneType();
        if (tp == PocketBeagle || tp == BeaglePlay) {
            BBB_PINS.emplace_back("P1-02", 3, 10);
            BBB_PINS.emplace_back("P1-04", 3, 12);
            BBB_PINS.emplace_back("P1-06", 3, 13).setUART("ttyS1-rx");
            BBB_PINS.emplace_back("P1-08", 3, 14).setUART("ttyS1-tx");
            BBB_PINS.emplace_back("P1-10", 3, 7).setUART("ttyS6-rx");
            BBB_PINS.emplace_back("P1-12", 3, 8).setUART("ttyS6-tx");
            BBB_PINS.emplace_back("P1-13", 2, 36);
            BBB_PINS.emplace_back("P1-19", 3, 1);
            BBB_PINS.emplace_back("P1-20", 2, 50).setUART("ttyS4-tx");
            BBB_PINS.emplace_back("P1-21", 3, 6);
            BBB_PINS.emplace_back("P1-23", 3, 5);
            BBB_PINS.emplace_back("P1-25", 3, 4);
            BBB_PINS.emplace_back("P1-26", 2, 44).setUART("ttyS4-tx").setI2C(2);
            BBB_PINS.emplace_back("P1-27", 3, 3);
            BBB_PINS.emplace_back("P1-28", 2, 43).setUART("ttyS4-rx").setI2C(2);
            BBB_PINS.emplace_back("P1-29", 2, 62);
            BBB_PINS.emplace_back("P1-30", 3, 21).setUART("ttyS0-tx");
            BBB_PINS.emplace_back("P1-31", 2, 59);
            BBB_PINS.emplace_back("P1-32", 3, 20).setUART("ttyS4-rx");
            BBB_PINS.emplace_back("P1-33", 2, 56);
            BBB_PINS.emplace_back("P1-33b", 3, 29);
            BBB_PINS.emplace_back("P1-34", 3, 2);
            BBB_PINS.emplace_back("P1-35", 2, 88); ///?
            BBB_PINS.emplace_back("P1-36", 2, 55);
            BBB_PINS.emplace_back("P1-36b", 3, 28);

            BBB_PINS.emplace_back("P2-01", 3, 11);
            BBB_PINS.emplace_back("P2-02", 2, 45).setUART("ttyS2-rx");
            BBB_PINS.emplace_back("P2-03", 3, 9);
            BBB_PINS.emplace_back("P2-04", 2, 46).setUART("ttyS2-tx");
            BBB_PINS.emplace_back("P2-05", 3, 24).setUART("ttyS5-rx");
            BBB_PINS.emplace_back("P2-06", 2, 47).setUART("ttyS3-rx");
            BBB_PINS.emplace_back("P2-07", 3, 25).setUART("ttyS5-tx");
            BBB_PINS.emplace_back("P2-08", 2, 48);
            BBB_PINS.emplace_back("P2-09", 3, 22).setUART("ttyS2-rx");
            BBB_PINS.emplace_back("P2-10", 2, 91); ///?
            BBB_PINS.emplace_back("P2-11", 3, 23).setUART("ttyS3-tx");
            BBB_PINS.emplace_back("P2-17", 2, 64); ///?
            BBB_PINS.emplace_back("P2-18", 2, 53).setUART("ttyS6-rx");
            BBB_PINS.emplace_back("P2-19", 3, 0);
            BBB_PINS.emplace_back("P2-20", 2, 49).setUART("ttyS4-rx");
            BBB_PINS.emplace_back("P2-22", 2, 63);
            BBB_PINS.emplace_back("P2-24", 2, 51).setUART("ttyS5-rx");
            BBB_PINS.emplace_back("P2-25", 3, 19);
            BBB_PINS.emplace_back("P2-27", 3, 18);
            BBB_PINS.emplace_back("P2-28", 2, 61);
            BBB_PINS.emplace_back("P2-29", 2, 40);
            BBB_PINS.emplace_back("P2-29b", 3, 17);
            BBB_PINS.emplace_back("P2-30", 2, 58);
            BBB_PINS.emplace_back("P2-31", 3, 15);
            BBB_PINS.emplace_back("P2-32", 2, 57);
            BBB_PINS.emplace_back("P2-33", 2, 52).setUART("ttyS5-tx");
            BBB_PINS.emplace_back("P2-34", 2, 60);
            BBB_PINS.emplace_back("P2-35", 2, 54);
            BBB_PINS.emplace_back("P2-36", 3, 16);
        } else {
            printf("Unknown type: %d\n", getBeagleBoneType());
        }
#endif
    }
}

std::vector<std::string> BBBPinProvider::getPinNames() {
    std::vector<std::string> ret;
    for (auto& a : BBB_PINS) {
        ret.push_back(a.name);
    }
    return ret;
}
const PinCapabilities& BBBPinProvider::getPinByUART(const std::string& n) {
    for (auto& a : BBB_PINS) {
        if (a.uart == n) {
            return a;
        }
    }
    return NULL_BBB_INSTANCE;
}
