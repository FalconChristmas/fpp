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

static const std::vector<BBBPinCapabilities> PB_PINS = {
    BBBPinCapabilities("P1-02", 87, 0x8E4).setPRU(1, 9, 6, 5),
    BBBPinCapabilities("P1-04", 89, 0x8EC).setPRU(1, 11, 6, 5),
    BBBPinCapabilities("P1-06", 5, 0x95C).setI2C(1, 2),
    BBBPinCapabilities("P1-08", 2, 0x950).setPwm(0, 0, 3).setUART("ttyS2-rx", 1),
    BBBPinCapabilities("P1-10", 3, 0x954).setPwm(0, 1, 3).setUART("ttyS2-tx", 1),
    BBBPinCapabilities("P1-12", 4, 0x958).setI2C(1, 2),
    BBBPinCapabilities("P1-20", 20, 0x9B4),
    BBBPinCapabilities("P1-26", 12, 0x978).setI2C(2, 3),
    BBBPinCapabilities("P1-28", 13, 0x97C).setI2C(2, 3),
    BBBPinCapabilities("P1-29", 117, 0x9AC).setPRU(0, 7, 6, 5),
    BBBPinCapabilities("P1-30", 43, 0x974).setPRU(1, 15, 6, 5).setUART("ttyS0-tx", 0),
    BBBPinCapabilities("P1-31", 114, 0x9A0).setPRU(0, 4, 6, 5),
    BBBPinCapabilities("P1-32", 42, 0x970).setPRU(1, 14, 6, 5).setUART("ttyS0-rx", 0),
    BBBPinCapabilities("P1-33", 111, 0x994).setPRU(0, 1, 6, 5).setPwm(0, 1, 1),
    BBBPinCapabilities("P1-34", 26, 0x828),
    BBBPinCapabilities("P1-35", 88, 0x8E8).setPRU(1, 10, 6, 5),
    BBBPinCapabilities("P1-36", 110, 0x990).setPRU(0, 0, 6, 5).setPwm(0, 0, 1),
    BBBPinCapabilities("P2-01", 50, 0x848).setPwm(1, 0, 6),
    BBBPinCapabilities("P2-02", 59, 0x86C),
    BBBPinCapabilities("P2-03", 23, 0x824).setPwm(2, 1, 4),
    BBBPinCapabilities("P2-04", 58, 0x868),
    BBBPinCapabilities("P2-05", 30, 0x870).setUART("ttyS4-rx", 6),
    BBBPinCapabilities("P2-06", 57, 0x864),
    BBBPinCapabilities("P2-07", 31, 0x874).setUART("ttyS4-tx", 6),
    BBBPinCapabilities("P2-08", 60, 0x878),
    BBBPinCapabilities("P2-09", 15, 0x984).setUART("ttyS1-tx", 0),
    BBBPinCapabilities("P2-10", 52, 0x850),
    BBBPinCapabilities("P2-11", 14, 0x980).setUART("ttyS1-rx", 0),
    BBBPinCapabilities("P2-17", 65, 0x88C),
    BBBPinCapabilities("P2-18", 47, 0x83C),
    BBBPinCapabilities("P2-19", 27, 0x82C),
    BBBPinCapabilities("P2-20", 64, 0x888),
    BBBPinCapabilities("P2-22", 46, 0x838),
    BBBPinCapabilities("P2-24", 44, 0x830).setPRU(0, 14, -1, 6),
    BBBPinCapabilities("P2-25", 41, 0x96C).setUART("ttyS4-tx", 1),
    BBBPinCapabilities("P2-27", 40, 0x968).setUART("ttyS4-rx", 1),
    BBBPinCapabilities("P2-28", 116, 0x9A8).setPRU(0, 6, 6, 5),
    BBBPinCapabilities("P2-29", 7, 0x964).setUART("ttyS3-tx", 1),
    BBBPinCapabilities("P2-30", 113, 0x99C).setPRU(0, 3, 6, 5),
    BBBPinCapabilities("P2-31", 19, 0x9B0),
    BBBPinCapabilities("P2-32", 112, 0x998).setPRU(0, 2, 6, 5),
    BBBPinCapabilities("P2-33", 45, 0x834).setPRU(0, 15, -1, 6),
    BBBPinCapabilities("P2-34", 115, 0x9A4).setPRU(0, 5, 6, 5),
    BBBPinCapabilities("P2-35", 86, 0x8E0).setPRU(1, 8, 6, 5)
};
static const std::vector<BBBPinCapabilities> BBB_PINS = {
    BBBPinCapabilities("P8-03", 38, 0x818),
    BBBPinCapabilities("P8-04", 39, 0x81C),
    BBBPinCapabilities("P8-05", 34, 0x808),
    BBBPinCapabilities("P8-06", 35, 0x80C),
    BBBPinCapabilities("P8-07", 66, 0x890),
    BBBPinCapabilities("P8-08", 67, 0x894),
    BBBPinCapabilities("P8-09", 69, 0x89C),
    BBBPinCapabilities("P8-10", 68, 0x898),
    BBBPinCapabilities("P8-11", 45, 0x834).setPRU(0, 15, -1, 6),
    BBBPinCapabilities("P8-12", 44, 0x830).setPRU(0, 14, -1, 6),
    BBBPinCapabilities("P8-13", 23, 0x824).setPwm(2, 1, 4),
    BBBPinCapabilities("P8-14", 26, 0x828),
    BBBPinCapabilities("P8-15", 47, 0x83C),
    BBBPinCapabilities("P8-16", 46, 0x838),
    BBBPinCapabilities("P8-17", 27, 0x82C),
    BBBPinCapabilities("P8-18", 65, 0x88C),
    BBBPinCapabilities("P8-19", 22, 0x820).setPwm(2, 0, 4),
    BBBPinCapabilities("P8-20", 63, 0x884).setPRU(1, 13, 6, 5),
    BBBPinCapabilities("P8-21", 62, 0x880).setPRU(1, 12, 6, 5),
    BBBPinCapabilities("P8-22", 37, 0x814),
    BBBPinCapabilities("P8-23", 36, 0x810),
    BBBPinCapabilities("P8-24", 33, 0x804),
    BBBPinCapabilities("P8-25", 32, 0x800),
    BBBPinCapabilities("P8-26", 61, 0x87C),
    BBBPinCapabilities("P8-27", 86, 0x8E0).setPRU(1, 8, 6, 5),
    BBBPinCapabilities("P8-28", 88, 0x8E8).setPRU(1, 10, 6, 5),
    BBBPinCapabilities("P8-29", 87, 0x8E4).setPRU(1, 9, 6, 5),
    BBBPinCapabilities("P8-30", 89, 0x8EC).setPRU(1, 11, 6, 5),
    BBBPinCapabilities("P8-31", 10, 0x8D8),
    BBBPinCapabilities("P8-32", 11, 0x8DC),
    BBBPinCapabilities("P8-33", 9, 0x8D4),
    BBBPinCapabilities("P8-34", 81, 0x8CC).setPwm(1, 1, 2),
    BBBPinCapabilities("P8-35", 8, 0x8D0),
    BBBPinCapabilities("P8-36", 80, 0x8C8).setPwm(1, 0, 2),
    BBBPinCapabilities("P8-37", 78, 0x8C0).setUART("ttyS5-tx", 4),
    BBBPinCapabilities("P8-38", 79, 0x8C4).setUART("ttyS5-rx", 4),
    BBBPinCapabilities("P8-39", 76, 0x8B8).setPRU(1, 6, 6, 5),
    BBBPinCapabilities("P8-40", 77, 0x8BC).setPRU(1, 7, 6, 5),
    BBBPinCapabilities("P8-41", 74, 0x8B0).setPRU(1, 4, 6, 5),
    BBBPinCapabilities("P8-42", 75, 0x8B4).setPRU(1, 5, 6, 5),
    BBBPinCapabilities("P8-43", 72, 0x8A8).setPRU(1, 2, 6, 5),
    BBBPinCapabilities("P8-44", 73, 0x8AC).setPRU(1, 3, 6, 5),
    BBBPinCapabilities("P8-45", 70, 0x8A0).setPRU(1, 0, 6, 5).setPwm(2, 0, 3),
    BBBPinCapabilities("P8-46", 71, 0x8A4).setPRU(1, 1, 6, 5).setPwm(2, 1, 3),
    BBBPinCapabilities("P9-11", 30, 0x870).setUART("ttyS4-rx", 6),
    BBBPinCapabilities("P9-12", 60, 0x878),
    BBBPinCapabilities("P9-13", 31, 0x874).setUART("ttyS4-tx", 6),
    BBBPinCapabilities("P9-14", 50, 0x848).setPwm(1, 0, 6),
    BBBPinCapabilities("P9-15", 48, 0x840),
    BBBPinCapabilities("P9-16", 51, 0x84C).setPwm(1, 1, 6),
    BBBPinCapabilities("P9-17", 5, 0x95C).setI2C(1, 2),
    BBBPinCapabilities("P9-18", 4, 0x958).setI2C(1, 2),
    BBBPinCapabilities("P9-19", 13, 0x97C).setI2C(2, 3),
    BBBPinCapabilities("P9-20", 12, 0x978).setI2C(2, 3),
    BBBPinCapabilities("P9-21", 3, 0x954).setPwm(0, 1, 3).setUART("ttyS2-tx", 1),
    BBBPinCapabilities("P9-22", 2, 0x950).setPwm(0, 0, 3).setUART("ttyS2-rx", 1),
    BBBPinCapabilities("P9-23", 49, 0x844),
    BBBPinCapabilities("P9-24", 15, 0x984).setUART("ttyS1-tx", 0),
    BBBPinCapabilities("P9-25", 117, 0x9AC).setPRU(0, 7, 6, 5),
    BBBPinCapabilities("P9-26", 14, 0x980).setUART("ttyS1-rx", 0),
    BBBPinCapabilities("P9-27", 115, 0x9A4).setPRU(0, 5, 6, 5),
    BBBPinCapabilities("P9-28", 113, 0x99C).setPRU(0, 3, 6, 5),
    BBBPinCapabilities("P9-29", 111, 0x994).setPRU(0, 1, 6, 5).setPwm(0, 1, 1),
    BBBPinCapabilities("P9-30", 112, 0x998).setPRU(0, 2, 6, 5),
    BBBPinCapabilities("P9-31", 110, 0x990).setPRU(0, 0, 6, 5).setPwm(0, 0, 1),
    BBBPinCapabilities("P9-41", 20, 0x9B4),
    BBBPinCapabilities("P9-91", 116, 0x9A8).setPRU(0, 6, 6, 5),
    BBBPinCapabilities("P9-42", 7, 0x964).setUART("ttyS3-tx", 1),
    BBBPinCapabilities("P9-92", 114, 0x9A0).setPRU(0, 4, 6, 5),
};

// ------------------------------------------------------------------------

#define GPIO_DATAIN 0x138
#define GPIO_DATAOUT 0x13C
#define GPIO_SETDATAOUT 0x194
#define GPIO_CLEARDATAOUT 0x190

// GPIO locations
#define GPIO0_BASE 0x44E07000
#define GPIO1_BASE 0x4804C000
#define GPIO2_BASE 0x481AC000
#define GPIO3_BASE 0x481AE000
#define GPIO_SIZE 0x00001000

#define CONTROL_MODULE_BASE 0x44E10000
#define CONTROL_MODULE_SIZE 128 * 1024 * 1024

// 1MHz frequence
// #define HZ 1000000.0f
#define HZ 10000.0f

static uint8_t* bbGPIOMap[] = { 0, 0, 0, 0 };
static uint8_t* controlModule = nullptr;

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
    int fd = open("/dev/mem", O_RDWR);
    bbGPIOMap[0] = (uint8_t*)mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO0_BASE);
    bbGPIOMap[1] = (uint8_t*)mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO1_BASE);
    bbGPIOMap[2] = (uint8_t*)mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO2_BASE);
    bbGPIOMap[3] = (uint8_t*)mmap(0, GPIO_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, GPIO3_BASE);
    controlModule = (uint8_t*)mmap(0, CONTROL_MODULE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, CONTROL_MODULE_BASE);
    close(fd);
    registersMemMapped = 1;
}

// ------------------------------------------------------------------------

BBBPinCapabilities::BBBPinCapabilities(const std::string& n, uint32_t k, uint32_t o) :
    PinCapabilities(n, k), configOffset(o), pru(-1), pruPin(-1), pruInMode(-1), pruOutMode(-1), pwmMode(-1), uartMode(-1), i2cMode(-1) {
    setupBBBMemoryMap();
    gpioIdx = k / 32;
    gpio = k % 32;
    if (configOffset == 0) {
        defaultConfig = 0;
    } else {
        defaultConfig = controlModule[configOffset];
    }
    // printf("%s    Def: %X\n", name.c_str(), defaultConfig);
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
    // printf("%s Mode: %s    Cur: %X\n", name.c_str(), m.c_str(), controlModule[configOffset]);
    std::string mode = m;
    if (mode == "default") {
        controlModule[configOffset] = defaultConfig;
        return 0;
    }
    bool enableI2C = mode == "i2c";
    if (i2cBus >= 0 && !enableI2C) {
        enableOledScreen(i2cBus, false);
    }

    if (mode == "i2c") {
        controlModule[configOffset] = i2cMode | 0x30;
    } else if (mode == "pwm") {
        controlModule[configOffset] = uartMode | 0x28;
    } else if (mode == "pruout") {
        controlModule[configOffset] = pruOutMode | 0x28;
    } else if (mode == "pruin") {
        controlModule[configOffset] = pruOutMode | 0x20;
    } else if (mode == "uart") {
        controlModule[configOffset] = uartMode | 0x30;
    } else {
        // gpio
        uint8_t nm = 0x27; // gpio is mode 7
        if (mode == "gpio_pu") {
            nm |= 0x10;
        } else if (mode == "gpio_pd") {
            nm |= 0x08;
        }
        controlModule[configOffset] = nm;
    }

    char pinName[16];
    strcpy(pinName, name.c_str());
    if (pinName[2] == '-') {
        pinName[2] = '_';
    }
    char dir_name[128];

    int kgpio = kernelGpio;
    if (DirectoryExists("/sys/class/gpio/gpiochip512")) {
        kgpio += 512;
    }
    snprintf(dir_name, sizeof(dir_name), "/sys/class/gpio/gpio%u/direction", kgpio);
    if (access(dir_name, F_OK) == -1) {
        // not exported, we need to export it
        FILE* dir = fopen("/sys/class/gpio/export", "w");
        fprintf(dir, "%d", kgpio);
        fclose(dir);
    }
    snprintf(dir_name, sizeof(dir_name),
             "/sys/devices/platform/ocp/ocp:%s_pinmux/state",
             pinName);
    FILE* dir = fopen(dir_name, "w");
    if (dir) {
        fprintf(dir, "%s\n", mode.c_str());
        fclose(dir);
    }
    if (mode != "pwm" && mode != "i2c" && mode != "uart") {
        snprintf(dir_name, sizeof(dir_name),
                 "/sys/class/gpio/gpio%u/direction",
                 kgpio);

        dir = fopen(dir_name, "w");
        if (!dir) {
            return -1;
        }
        fprintf(dir, "%s\n", directionOut ? "out" : "in");
        fclose(dir);
    }
    if (i2cBus >= 0 && enableI2C) {
        enableOledScreen(i2cBus, true);
    }
    return 0;
}
bool BBBPinCapabilities::getValue() const {
    setupBBBMemoryMap();
    int set = 1 << gpio;
    uint32_t* base = (uint32_t*)(bbGPIOMap[gpioIdx] + GPIO_DATAIN);
    return (*base & set) ? true : false;
}
void BBBPinCapabilities::setValue(bool v) const {
    setupBBBMemoryMap();
    int set = 1 << gpio;
    uint8_t* base = bbGPIOMap[gpioIdx];
    if (v) {
        base += GPIO_SETDATAOUT;
    } else {
        base += GPIO_CLEARDATAOUT;
    }
    uint32_t* base32 = (uint32_t*)base;
    *base32 = set;
}

int BBBPinCapabilities::openValueForPoll() const {
    char dir_name[128];
    int kgpio = kernelGpio;
    if (DirectoryExists("/sys/class/gpio/gpiochip512")) {
        kgpio += 512;
    }
    snprintf(dir_name, sizeof(dir_name),
             "/sys/class/gpio/gpio%u/value",
             kgpio);
    return open(dir_name, O_RDONLY | O_NONBLOCK);
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
    if (getBeagleBoneType() == PocketBeagle) {
        for (auto& a : PB_PINS) {
            if (a.name == name) {
                return a;
            }
        }
    } else {
        for (auto& a : BBB_PINS) {
            if (a.name == name) {
                return a;
            }
        }
    }
    return NULL_BBB_INSTANCE;
}

const PinCapabilities& BBBPinProvider::getPinByGPIO(int i) {
    if (getBeagleBoneType() == PocketBeagle) {
        for (auto& a : PB_PINS) {
            if (a.kernelGpio == i) {
                return a;
            }
        }
    } else {
        for (auto& a : BBB_PINS) {
            if (a.kernelGpio == i) {
                return a;
            }
        }
    }
    return NULL_BBB_INSTANCE;
}

void BBBPinProvider::Init() {
}

std::vector<std::string> BBBPinProvider::getPinNames() {
    std::vector<std::string> ret;
    if (getBeagleBoneType() == PocketBeagle) {
        for (auto& a : PB_PINS) {
            ret.push_back(a.name);
        }
    } else {
        for (auto& a : BBB_PINS) {
            ret.push_back(a.name);
        }
    }
    return ret;
}
const PinCapabilities& BBBPinProvider::getPinByUART(const std::string& n) {
    if (getBeagleBoneType() == PocketBeagle) {
        for (auto& a : PB_PINS) {
            if (a.uart == n) {
                return a;
            }
        }
    } else {
        for (auto& a : BBB_PINS) {
            if (a.uart == n) {
                return a;
            }
        }
    }
    return NULL_BBB_INSTANCE;
}
