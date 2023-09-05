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

#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <thread>

#include "GPIOUtils.h"
#include "commands/Commands.h"

// No platform information on how to control pins
static NoPinCapabilities NULL_PIN_INSTANCE("-none-", 0);

const NoPinCapabilities& NoPinCapabilitiesProvider::getPinByName(const std::string& name) { return NULL_PIN_INSTANCE; }
const NoPinCapabilities& NoPinCapabilitiesProvider::getPinByGPIO(int i) { return NULL_PIN_INSTANCE; }
const NoPinCapabilities& NoPinCapabilitiesProvider::getPinByUART(const std::string& n) { return NULL_PIN_INSTANCE; }

Json::Value PinCapabilities::toJSON() const {
    Json::Value ret;
    if (name != "" && name != "-non-") {
        ret["pin"] = name;
        ret["gpio"] = kernelGpio;
        ret["gpioChip"] = gpioIdx;
        ret["gpioLine"] = gpio;
        if (pru != -1) {
            ret["pru"] = pru;
            ret["pruPin"] = pruPin;
        }
        if (pwm != -1) {
            ret["pwm"] = pwm;
            ret["subPwm"] = subPwm;
        }
        if (i2cBus != -1) {
            ret["i2c"] = i2cBus;
        }
        if (uart != "") {
            ret["uart"] = uart;
        }
        ret["supportsPullUp"] = supportsPullUp();
        ret["supportsPullDown"] = supportsPullDown();
    }
    return ret;
}

void PinCapabilities::enableOledScreen(int i2cBus, bool enable) {
    // this pin is i2c, we may need to tell fppoled to turn off the display
    // before we shutdown this pin because once we re-configure, i2c will
    // be unavailable and the display won't update
    int smfd = shm_open("fppoled", O_CREAT | O_RDWR, 0);
    ftruncate(smfd, 1024);
    unsigned int* status = (unsigned int*)mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, smfd, 0);
    if (i2cBus == status[0]) {
        printf("Signal to fppoled to enable/disable I2C:   Bus: %d   Enable: %d\n", i2cBus, enable);
        if (!enable) {
            // force the display off
            status[2] = 1;
            int count = 0;
            while (status[1] != 0 && count < 150) {
                count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            // allow the display to come back on
            status[2] = 0;
        }
    }
    close(smfd);
    munmap(status, 1024);
}

// the built in GPIO chips that are handled by the more optimized
// platform specific GPIO drivers
static const std::set<std::string> PLATFORM_IGNORES{
    "pinctrl-bcm2835", // raspberry pi's
    "pinctrl-bcm2711", // Pi4
    "raspberrypi-exp-gpio",
    "brcmvirt-gpio",
    "gpio-0-31", // beagles
    "gpio-32-63",
    "gpio-64-95",
    "gpio-96-127"
};
// No platform information on how to control pins
static std::string PROCESS_NAME = "FPPD";

int GPIODCapabilities::gpiodVersion = 0;

bool GPIODCapabilities::supportsPullUp() const {
    return gpiodVersion >= 105;
}
bool GPIODCapabilities::supportsPullDown() const {
    return gpiodVersion >= 105;
}

#ifdef HASGPIOD
constexpr int MAX_GPIOD_CHIPS = 15;
class GPIOChipHolder {
public:
    static GPIOChipHolder INSTANCE;

    std::array<gpiod::chip, MAX_GPIOD_CHIPS> chips;
};
GPIOChipHolder GPIOChipHolder::INSTANCE;
#endif
int GPIODCapabilities::configPin(const std::string& mode,
                                 bool directionOut) const {
#ifdef HASGPIOD
    if (chip == nullptr) {
        if (!GPIOChipHolder::INSTANCE.chips[gpioIdx]) {
            std::string n = std::to_string(gpioIdx);
            GPIOChipHolder::INSTANCE.chips[gpioIdx].open(n, gpiod::chip::OPEN_BY_NUMBER);
        }
        chip = &GPIOChipHolder::INSTANCE.chips[gpioIdx];
        line = chip->get_line(gpio);
    }
    gpiod::line_request req;
    req.consumer = PROCESS_NAME;
    if (directionOut) {
        req.request_type = gpiod::line_request::DIRECTION_OUTPUT;
    } else {
        req.request_type = gpiod::line_request::DIRECTION_INPUT;

        if (gpiodVersion >= 105) {
            // pull up/down was added in libgpiod 1.5
            if (mode == "gpio_pu") {
                req.flags |= ::std::bitset<32>(GPIOD_BIT(5));
            } else if (mode == "gpio_pd") {
                req.flags |= ::std::bitset<32>(GPIOD_BIT(4));
            } else {
                req.flags |= ::std::bitset<32>(GPIOD_BIT(3));
            }
        }
    }
    if (req.request_type != lastRequestType) {
        if (line.is_requested()) {
            line.release();
        }
        line.request(req, 1);
        lastRequestType = req.request_type;
    }
#endif
    return 0;
}
bool GPIODCapabilities::getValue() const {
#ifdef HASGPIOD
    return line.get_value();
#else
    return 0;
#endif
}
void GPIODCapabilities::setValue(bool i) const {
#ifdef HASGPIOD
    line.set_value(i ? 1 : 0);
#endif
}
static std::vector<GPIODCapabilities> GPIOD_PINS;
static PinCapabilitiesProvider* PIN_PROVIDER = nullptr;

void PinCapabilities::InitGPIO(const std::string& process, PinCapabilitiesProvider* p) {
    PROCESS_NAME = process;
    if (p == nullptr) {
        p = new NoPinCapabilitiesProvider();
    }
    PIN_PROVIDER = p;
#ifdef HASGPIOD
    const char* ver = gpiod_version_string();
    bool inMin = false;
    int gvMaj = 0;
    int gvMin = 0;

    bool hasDot = false;
    while (*ver) {
        if (*ver == '.') {
            if (inMin) {
                while (ver[1]) {
                    ver++;
                }
            } else {
                inMin = true;
            }
        } else {
            if (inMin) {
                gvMin *= 10;
                gvMin += *ver - '0';
            } else {
                gvMaj *= 10;
                gvMaj += *ver - '0';
            }
        }
        ver++;
    }
    GPIODCapabilities::gpiodVersion = gvMaj * 100 + gvMin;
    int chipCount = 0;
    int pinCount = 0;
    ::gpiod_chip* chip = gpiod_chip_open_by_number(0);
    if (chip != nullptr) {
        ::gpiod_chip_close(chip);
        if (GPIOD_PINS.empty()) {
            // has at least on chip
            std::set<std::string> found;
            for (auto& a : gpiod::make_chip_iter()) {
                std::string name = a.name();
                std::string label = a.label();

                if (PLATFORM_IGNORES.find(label) == PLATFORM_IGNORES.end()) {
                    char i = 'b';
                    while (found.find(label) != found.end()) {
                        label = a.label() + i;
                        i++;
                    }
                    found.insert(label);
                    for (int x = 0; x < a.num_lines(); x++) {
                        std::string n = label + "-" + std::to_string(x);
                        GPIODCapabilities caps(n, pinCount + x);
                        caps.setGPIO(chipCount, x);
                        GPIOD_PINS.push_back(GPIODCapabilities(n, pinCount + x).setGPIO(chipCount, x));
                    }
                }
                pinCount += a.num_lines();
                chipCount++;
            }
        }
    }
#endif
    PIN_PROVIDER->Init();
}
std::vector<std::string> PinCapabilities::getPinNames() {
    std::vector<std::string> pn = PIN_PROVIDER->getPinNames();
    for (auto& a : GPIOD_PINS) {
        pn.push_back(a.name);
    }
    return pn;
}

const PinCapabilities& PinCapabilities::getPinByName(const std::string& n) {
    for (auto& a : GPIOD_PINS) {
        if (n == a.name) {
            return a;
        }
    }
    return PIN_PROVIDER->getPinByName(n);
}
const PinCapabilities& PinCapabilities::getPinByGPIO(int i) {
    for (auto& a : GPIOD_PINS) {
        if (i == a.kernelGpio) {
            return a;
        }
    }
    return PIN_PROVIDER->getPinByGPIO(i);
}
const PinCapabilities& PinCapabilities::getPinByUART(const std::string& n) {
    return PIN_PROVIDER->getPinByUART(n);
}

void PinCapabilities::SetMultiPinValue(const std::list<const PinCapabilities*>& pins, int v) {
    if (pins.empty()) {
        return;
    }
    for (auto p : pins) {
        p->setValue(v);
    }
}
