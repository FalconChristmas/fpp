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
#include <fcntl.h>
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

static uint32_t bbGPIOMap[] = { 0, 1, 2, 3 };
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

    // newer kernels will number the chips differently so we'll use the
    // labels to figure out the mapping to the gpio0-3 that we use
    int curChip = 0;
    for (auto& a : gpiod::make_chip_iter()) {
        // std::string cname = a.name();
        // std::string clabel = a.label();
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
        ++curChip;
        if (curChip == 4) {
            break;
        }
    }
    registersMemMapped = 1;
}

// ------------------------------------------------------------------------

BBBPinCapabilities::BBBPinCapabilities(const std::string& n, uint32_t k, uint32_t o) :
    GPIODCapabilities(n, k), pru(-1), pruPin(-1) {
    setupBBBMemoryMap();
    gpioIdx = bbGPIOMap[k / 32];
    gpio = k % 32;
}

int BBBPinCapabilities::mappedGPIOIdx() const {
    for (int x = 0; x < 4; x++) {
        if (bbGPIOMap[x] == gpioIdx) {
            return x;
        }
    }
    return gpioIdx;
}

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
        if (getBeagleBoneType() == PocketBeagle) {
            BBB_PINS.emplace_back("P1-02", 87, 0x8E4).setPRU(1, 9, 6, 5);
            BBB_PINS.emplace_back("P1-04", 89, 0x8EC).setPRU(1, 11, 6, 5);
            BBB_PINS.emplace_back("P1-06", 5, 0x95C).setI2C(1, 2);
            BBB_PINS.emplace_back("P1-08", 2, 0x950).setPwm(0, 0, 3).setUART("ttyS2-rx", 1);
            BBB_PINS.emplace_back("P1-10", 3, 0x954).setPwm(0, 1, 3).setUART("ttyS2-tx", 1);
            BBB_PINS.emplace_back("P1-12", 4, 0x958).setI2C(1, 2);
            BBB_PINS.emplace_back("P1-20", 20, 0x9B4);
            BBB_PINS.emplace_back("P1-26", 12, 0x978).setI2C(2, 3);
            BBB_PINS.emplace_back("P1-28", 13, 0x97C).setI2C(2, 3);
            BBB_PINS.emplace_back("P1-29", 117, 0x9AC).setPRU(0, 7, 6, 5);
            BBB_PINS.emplace_back("P1-30", 43, 0x974).setPRU(1, 15, 6, 5).setUART("ttyS0-tx", 0);
            BBB_PINS.emplace_back("P1-31", 114, 0x9A0).setPRU(0, 4, 6, 5);
            BBB_PINS.emplace_back("P1-32", 42, 0x970).setPRU(1, 14, 6, 5).setUART("ttyS0-rx", 0);
            BBB_PINS.emplace_back("P1-33", 111, 0x994).setPRU(0, 1, 6, 5).setPwm(0, 1, 1);
            BBB_PINS.emplace_back("P1-34", 26, 0x828);
            BBB_PINS.emplace_back("P1-35", 88, 0x8E8).setPRU(1, 10, 6, 5);
            BBB_PINS.emplace_back("P1-36", 110, 0x990).setPRU(0, 0, 6, 5).setPwm(0, 0, 1);
            BBB_PINS.emplace_back("P2-01", 50, 0x848).setPwm(1, 0, 6);
            BBB_PINS.emplace_back("P2-02", 59, 0x86C);
            BBB_PINS.emplace_back("P2-03", 23, 0x824).setPwm(2, 1, 4);
            BBB_PINS.emplace_back("P2-04", 58, 0x868);
            BBB_PINS.emplace_back("P2-05", 30, 0x870).setUART("ttyS4-rx", 6);
            BBB_PINS.emplace_back("P2-06", 57, 0x864);
            BBB_PINS.emplace_back("P2-07", 31, 0x874).setUART("ttyS4-tx", 6);
            BBB_PINS.emplace_back("P2-08", 60, 0x878);
            BBB_PINS.emplace_back("P2-09", 15, 0x984).setUART("ttyS1-tx", 0);
            BBB_PINS.emplace_back("P2-10", 52, 0x850);
            BBB_PINS.emplace_back("P2-11", 14, 0x980).setUART("ttyS1-rx", 0);
            BBB_PINS.emplace_back("P2-17", 65, 0x88C);
            BBB_PINS.emplace_back("P2-18", 47, 0x83C);
            BBB_PINS.emplace_back("P2-19", 27, 0x82C);
            BBB_PINS.emplace_back("P2-20", 64, 0x888);
            BBB_PINS.emplace_back("P2-22", 46, 0x838);
            BBB_PINS.emplace_back("P2-24", 44, 0x830).setPRU(0, 14, -1, 6);
            BBB_PINS.emplace_back("P2-25", 41, 0x96C).setUART("ttyS4-tx", 1);
            BBB_PINS.emplace_back("P2-27", 40, 0x968).setUART("ttyS4-rx", 1);
            BBB_PINS.emplace_back("P2-28", 116, 0x9A8).setPRU(0, 6, 6, 5);
            BBB_PINS.emplace_back("P2-29", 7, 0x964).setUART("ttyS3-tx", 1);
            BBB_PINS.emplace_back("P2-30", 113, 0x99C).setPRU(0, 3, 6, 5);
            BBB_PINS.emplace_back("P2-31", 19, 0x9B0);
            BBB_PINS.emplace_back("P2-32", 112, 0x998).setPRU(0, 2, 6, 5);
            BBB_PINS.emplace_back("P2-33", 45, 0x834).setPRU(0, 15, -1, 6);
            BBB_PINS.emplace_back("P2-34", 115, 0x9A4).setPRU(0, 5, 6, 5);
            BBB_PINS.emplace_back("P2-35", 86, 0x8E0).setPRU(1, 8, 6, 5);
        } else {
            BBB_PINS.emplace_back("P8-03", 38, 0x818);
            BBB_PINS.emplace_back("P8-04", 39, 0x81C);
            BBB_PINS.emplace_back("P8-05", 34, 0x808);
            BBB_PINS.emplace_back("P8-06", 35, 0x80C);
            BBB_PINS.emplace_back("P8-07", 66, 0x890);
            BBB_PINS.emplace_back("P8-08", 67, 0x894);
            BBB_PINS.emplace_back("P8-09", 69, 0x89C);
            BBB_PINS.emplace_back("P8-10", 68, 0x898);
            BBB_PINS.emplace_back("P8-11", 45, 0x834).setPRU(0, 15, -1, 6);
            BBB_PINS.emplace_back("P8-12", 44, 0x830).setPRU(0, 14, -1, 6);
            BBB_PINS.emplace_back("P8-13", 23, 0x824).setPwm(2, 1, 4);
            BBB_PINS.emplace_back("P8-14", 26, 0x828);
            BBB_PINS.emplace_back("P8-15", 47, 0x83C);
            BBB_PINS.emplace_back("P8-16", 46, 0x838);
            BBB_PINS.emplace_back("P8-17", 27, 0x82C);
            BBB_PINS.emplace_back("P8-18", 65, 0x88C);
            BBB_PINS.emplace_back("P8-19", 22, 0x820).setPwm(2, 0, 4);
            BBB_PINS.emplace_back("P8-20", 63, 0x884).setPRU(1, 13, 6, 5);
            BBB_PINS.emplace_back("P8-21", 62, 0x880).setPRU(1, 12, 6, 5);
            BBB_PINS.emplace_back("P8-22", 37, 0x814);
            BBB_PINS.emplace_back("P8-23", 36, 0x810);
            BBB_PINS.emplace_back("P8-24", 33, 0x804);
            BBB_PINS.emplace_back("P8-25", 32, 0x800);
            BBB_PINS.emplace_back("P8-26", 61, 0x87C);
            BBB_PINS.emplace_back("P8-27", 86, 0x8E0).setPRU(1, 8, 6, 5);
            BBB_PINS.emplace_back("P8-28", 88, 0x8E8).setPRU(1, 10, 6, 5);
            BBB_PINS.emplace_back("P8-29", 87, 0x8E4).setPRU(1, 9, 6, 5);
            BBB_PINS.emplace_back("P8-30", 89, 0x8EC).setPRU(1, 11, 6, 5);
            BBB_PINS.emplace_back("P8-31", 10, 0x8D8);
            BBB_PINS.emplace_back("P8-32", 11, 0x8DC);
            BBB_PINS.emplace_back("P8-33", 9, 0x8D4);
            BBB_PINS.emplace_back("P8-34", 81, 0x8CC).setPwm(1, 1, 2);
            BBB_PINS.emplace_back("P8-35", 8, 0x8D0);
            BBB_PINS.emplace_back("P8-36", 80, 0x8C8).setPwm(1, 0, 2);
            BBB_PINS.emplace_back("P8-37", 78, 0x8C0).setUART("ttyS5-tx", 4);
            BBB_PINS.emplace_back("P8-38", 79, 0x8C4).setUART("ttyS5-rx", 4);
            BBB_PINS.emplace_back("P8-39", 76, 0x8B8).setPRU(1, 6, 6, 5);
            BBB_PINS.emplace_back("P8-40", 77, 0x8BC).setPRU(1, 7, 6, 5);
            BBB_PINS.emplace_back("P8-41", 74, 0x8B0).setPRU(1, 4, 6, 5);
            BBB_PINS.emplace_back("P8-42", 75, 0x8B4).setPRU(1, 5, 6, 5);
            BBB_PINS.emplace_back("P8-43", 72, 0x8A8).setPRU(1, 2, 6, 5);
            BBB_PINS.emplace_back("P8-44", 73, 0x8AC).setPRU(1, 3, 6, 5);
            BBB_PINS.emplace_back("P8-45", 70, 0x8A0).setPRU(1, 0, 6, 5).setPwm(2, 0, 3);
            BBB_PINS.emplace_back("P8-46", 71, 0x8A4).setPRU(1, 1, 6, 5).setPwm(2, 1, 3);
            BBB_PINS.emplace_back("P9-11", 30, 0x870).setUART("ttyS4-rx", 6);
            BBB_PINS.emplace_back("P9-12", 60, 0x878);
            BBB_PINS.emplace_back("P9-13", 31, 0x874).setUART("ttyS4-tx", 6);
            BBB_PINS.emplace_back("P9-14", 50, 0x848).setPwm(1, 0, 6);
            BBB_PINS.emplace_back("P9-15", 48, 0x840);
            BBB_PINS.emplace_back("P9-16", 51, 0x84C).setPwm(1, 1, 6);
            BBB_PINS.emplace_back("P9-17", 5, 0x95C).setI2C(1, 2);
            BBB_PINS.emplace_back("P9-18", 4, 0x958).setI2C(1, 2);
            BBB_PINS.emplace_back("P9-19", 13, 0x97C).setI2C(2, 3);
            BBB_PINS.emplace_back("P9-20", 12, 0x978).setI2C(2, 3);
            BBB_PINS.emplace_back("P9-21", 3, 0x954).setPwm(0, 1, 3).setUART("ttyS2-tx", 1);
            BBB_PINS.emplace_back("P9-22", 2, 0x950).setPwm(0, 0, 3).setUART("ttyS2-rx", 1);
            BBB_PINS.emplace_back("P9-23", 49, 0x844);
            BBB_PINS.emplace_back("P9-24", 15, 0x984).setUART("ttyS1-tx", 0);
            BBB_PINS.emplace_back("P9-25", 117, 0x9AC).setPRU(0, 7, 6, 5);
            BBB_PINS.emplace_back("P9-26", 14, 0x980).setUART("ttyS1-rx", 0);
            BBB_PINS.emplace_back("P9-27", 115, 0x9A4).setPRU(0, 5, 6, 5);
            BBB_PINS.emplace_back("P9-28", 113, 0x99C).setPRU(0, 3, 6, 5);
            BBB_PINS.emplace_back("P9-29", 111, 0x994).setPRU(0, 1, 6, 5).setPwm(0, 1, 1);
            BBB_PINS.emplace_back("P9-30", 112, 0x998).setPRU(0, 2, 6, 5);
            BBB_PINS.emplace_back("P9-31", 110, 0x990).setPRU(0, 0, 6, 5).setPwm(0, 0, 1);
            BBB_PINS.emplace_back("P9-41", 20, 0x9B4);
            BBB_PINS.emplace_back("P9-91", 116, 0x9A8).setPRU(0, 6, 6, 5);
            BBB_PINS.emplace_back("P9-42", 7, 0x964).setUART("ttyS3-tx", 1);
            BBB_PINS.emplace_back("P9-92", 114, 0x9A0).setPRU(0, 4, 6, 5);
        }
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
