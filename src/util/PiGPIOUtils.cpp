/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include <fcntl.h>

#include "PiFaceUtils.h"
#include "PiGPIOUtils.h"
#include "bcm2835.h"
#include "common_mini.h"

class PiGPIOPinCapabilities : public PinCapabilitiesFluent<PiGPIOPinCapabilities> {
public:
    PiGPIOPinCapabilities(const std::string& n, uint32_t kg);
    virtual void releasePin() const override;
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true,
                          const std::string& desc = "") const override;

    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;

    virtual bool setupPWM(int maxValueNS = 25500) const override;
    virtual void setPWMValue(int valueNS) const override;

    virtual int getPWMRegisterAddress() const override { return 0; };
    virtual bool supportPWM() const {
        return pwm != -1;
    }
};

static bool isPi5() {
    std::string model = GetFileContents("/proc/device-tree/model");
    static bool pi5 = startsWith(model, "Raspberry Pi 5") ||
                      startsWith(model, "Raspberry Pi Compute Module 5");
    return pi5;
}

PiGPIOPinCapabilities::PiGPIOPinCapabilities(const std::string& n, uint32_t kg) :
    PinCapabilitiesFluent(n, kg) {
}

void PiGPIOPinCapabilities::releasePin() const {
    configPin("default", false);
    PiGPIOPinCapabilities::releasePin();
}

int PiGPIOPinCapabilities::configPin(const std::string& mode,
                                     bool directionOut,
                                     const std::string& desc) const {
    if (mode == "pwm" && pwm != -1) {
        bcm2835_gpio_fsel(gpio, BCM2835_GPIO_FSEL_ALT5); // ALT5 is the PWM
        return 0;
    }

    if (mode == "dpi") {
        bcm2835_gpio_fsel(gpio, BCM2835_GPIO_FSEL_ALT2); // ALT2 is DPI
        return 0;
    }

    if (mode == "pwm" || mode == "uart") {
        return 0;
    }

    bcm2835_gpio_fsel(gpio, directionOut ? BCM2835_GPIO_FSEL_OUTP : BCM2835_GPIO_FSEL_INPT);
    if (mode == "gpio_pu") {
        bcm2835_gpio_set_pud(gpio, BCM2835_GPIO_PUD_UP);
    } else if (mode == "gpio_pd") {
        bcm2835_gpio_set_pud(gpio, BCM2835_GPIO_PUD_DOWN);
    } else {
        bcm2835_gpio_set_pud(gpio, BCM2835_GPIO_PUD_OFF);
    }
    return 0;
}

bool PiGPIOPinCapabilities::getValue() const {
    return bcm2835_gpio_lev(gpio);
}
void PiGPIOPinCapabilities::setValue(bool i) const {
    if (i) {
        bcm2835_gpio_set(gpio);
    } else {
        bcm2835_gpio_clr(gpio);
    }
}

bool PiGPIOPinCapabilities::setupPWM(int maxValueNS) const {
    if (pwm >= 0) {
        bcm2835_pwm_set_mode(pwm, 0, 1);
        bcm2835_pwm_set_clock(BCM2835_PWM_CLOCK_DIVIDER_8);
        bcm2835_pwm_set_range(pwm, maxValueNS);
        bcm2835_pwm_set_data(pwm, 0);
    }
    return true;
}
void PiGPIOPinCapabilities::setPWMValue(int valueNS) const {
    if (pwm >= 0) {
        bcm2835_pwm_set_data(pwm, valueNS);
    }
}

class Pi5GPIODCapabilities : public GPIODCapabilities {
public:
    Pi5GPIODCapabilities(const std::string& n, uint32_t kg) :
        GPIODCapabilities(n, kg) {
        gpioIdx = getPinctrlRpiChip();
        gpioName = pinctrlRpiChipName;
        gpio = kg;
    }

    virtual ~Pi5GPIODCapabilities() {
        if (dutyFile != nullptr) {
            fclose(dutyFile);
        }
    }

    Pi5GPIODCapabilities& setResetMode(const std::string& mode) {
        resetMode = mode;
        return *this;
    }
    Pi5GPIODCapabilities& setPwm(int p, int sub) {
        GPIODCapabilities::setPwm(p, sub);
        return *this;
    }

    virtual void releasePin() const override {
        char buf[256];
        snprintf(buf, 256, "/usr/bin/pinctrl set %d %s", gpio, resetMode.c_str());
        system(buf);
        GPIODCapabilities::releasePin();
    }
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true,
                          const std::string& desc = "") const override {
        // see https://datasheets.raspberrypi.com/rp1/rp1-peripherals.pdf
        // 1-3: https://elinux.org/RPi_BCM2835_GPIOs
        // 4/5: https://elinux.org/RPi_BCM2711_GPIOs
        if (mode == "pwm" && pwm != -1) {
            // alt3 is pwm
            char buf[256];
            snprintf(buf, 256, "/usr/bin/pinctrl set %d a3", gpio);
            system(buf);
            return 0;
        }

        if (mode == "dpi") {
            // alt1 is dpi
            char buf[256];
            snprintf(buf, 256, "/usr/bin/pinctrl set %d a1", gpio);
            system(buf);
            return 0;
        }

        if (mode == "pwm" || mode == "uart") {
            return 0;
        }
        return GPIODCapabilities::configPin(mode, directionOut, desc);
    }

    virtual bool supportPWM() const override { return pwm != -1; }
    virtual bool setupPWM(int maxValue = 25500) const override {
        if (pwm != -1) {
            configPin("pwm", true, "PWM");
            char dir_name[128];
            FILE* dir = fopen("/sys/class/pwm/pwmchip0/export", "w");
            fprintf(dir, "%d", subPwm);
            fclose(dir);

            snprintf(dir_name, sizeof(dir_name), "/sys/class/pwm/pwmchip0/pwm%d/period", subPwm);
            dir = fopen(dir_name, "w");
            fprintf(dir, "%d", maxValue);
            fclose(dir);

            snprintf(dir_name, sizeof(dir_name), "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", subPwm);
            dutyFile = fopen(dir_name, "w");
            fprintf(dutyFile, "0");
            fflush(dutyFile);

            snprintf(dir_name, sizeof(dir_name), "/sys/class/pwm/pwmchip0/pwm%d/enable", subPwm);
            dir = fopen(dir_name, "w");
            fprintf(dir, "1");
            fclose(dir);
        }
        return 0;
    }
    virtual void setPWMValue(int valueNS) const override {
        if (pwm != -1) {
            fprintf(dutyFile, "%d", valueNS);
            fflush(dutyFile);
        }
    }

    static int getPinctrlRpiChip() {
        if (pinctrlRpiChip == -1) {
            pinctrlRpiChip = 0; // Default fallback
#ifdef IS_GPIOD_CXX_V2
            int has54lineChip = -1;
            std::string has54lineChipName;
            for (const auto& entry : std::filesystem::directory_iterator("/dev/")) {
                if (startsWith(entry.path().filename().string(), "gpiochip")) {
                    try {
                        auto chip = gpiod::chip(entry.path().string());
                        auto info = chip.get_info();
                        if (info.label() == "pinctrl-rpi1") {
                            pinctrlRpiChip = std::stoi(entry.path().filename().string().substr(8));
                            pinctrlRpiChipName = info.name();
                            return pinctrlRpiChip;
                        } else if (info.num_lines() == 54) {
                            has54lineChip = std::stoi(entry.path().filename().string().substr(8));
                            has54lineChipName = info.name();
                        }
                    } catch (...) {
                        // Chip doesn't exist, continue
                    }
                }
            }
            if (has54lineChip != -1) {
                pinctrlRpiChip = has54lineChip;
                pinctrlRpiChipName = has54lineChipName;
                return pinctrlRpiChip;
            }
#else
            for (auto& a : gpiod::make_chip_iter()) {
                std::string label = a.label();
                if (label == "pinctrl-rp1") {
                    pinctrlRpiChipName = a.name();
                    return pinctrlRpiChip;
                }
                pinctrlRpiChip++;
            }
            // didn't find it by name, try finding the chip with 54 lines
            pinctrlRpiChip = 0;
            for (auto& a : gpiod::make_chip_iter()) {
                if (a.num_lines() == 54) {
                    pinctrlRpiChipName = a.name();
                    return pinctrlRpiChip;
                }
                pinctrlRpiChip++;
            }
#endif
        }
        return pinctrlRpiChip;
    }
    mutable FILE* dutyFile = nullptr;
    static int pinctrlRpiChip;
    static std::string pinctrlRpiChipName;
    std::string resetMode = "a0";
};
int Pi5GPIODCapabilities::pinctrlRpiChip = -1;
std::string Pi5GPIODCapabilities::pinctrlRpiChipName;
static std::vector<PiGPIOPinCapabilities> PI_PINS;
static std::vector<Pi5GPIODCapabilities> PI5_PINS;

void PiGPIOPinProvider::Init() {
    if (isPi5()) {
        std::string ipOrNo = isPi5() ? "no" : "ip";
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-3", 2).setResetMode("a3"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-5", 3).setResetMode("a3"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-7", 4));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-8", 14));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-10", 15));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-11", 17).setResetMode(ipOrNo));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-12", 18).setPwm(0, 0).setResetMode(ipOrNo));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-13", 27).setPwm(0, 1).setResetMode(ipOrNo));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-15", 22).setResetMode(ipOrNo));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-16", 23).setResetMode(ipOrNo));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-18", 24).setResetMode(ipOrNo));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-19", 10));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-21", 9));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-22", 25).setResetMode(ipOrNo));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-23", 11));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-24", 8).setResetMode("op"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-26", 7).setResetMode("op"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-27", 0).setResetMode("ip"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-28", 1).setResetMode("ip"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-29", 5).setResetMode("ip"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-31", 6).setResetMode("ip"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-32", 12).setResetMode("ip").setPwm(0, 0));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-33", 13).setResetMode("ip"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-35", 19).setResetMode(ipOrNo).setPwm(0, 1));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-36", 16).setResetMode(ipOrNo));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-37", 26).setResetMode("ip"));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-38", 20).setResetMode(ipOrNo));
        PI5_PINS.push_back(Pi5GPIODCapabilities("P1-40", 21).setResetMode(ipOrNo));
    } else if (bcm2835_init()) {
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-3", 2).setI2C(1));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-5", 3).setI2C(1));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-7", 4));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-8", 14));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-10", 15));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-11", 17));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-12", 18).setPwm(0, 0));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-13", 27));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-15", 22));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-16", 23));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-18", 24));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-19", 10));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-21", 9));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-22", 25));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-23", 11));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-24", 8));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-26", 7));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-27", 0).setI2C(0));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-28", 1).setI2C(0));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-29", 5));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-31", 6));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-32", 12).setPwm(0, 0));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-33", 13).setPwm(1, 0));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-35", 19).setPwm(1, 0));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-36", 16));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-37", 26));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-38", 20));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-40", 21));
    }
    PiFacePinCapabilities::Init();
}
const PinCapabilities& PiGPIOPinProvider::getPinByName(const std::string& name) {
    for (auto& a : PI_PINS) {
        if (a.name == name) {
            return a;
        }
    }
    for (auto& a : PI5_PINS) {
        if (a.name == name) {
            return a;
        }
    }
    return PiFacePinCapabilities::getPinByName(name);
}
const PinCapabilities& PiGPIOPinProvider::getPinByGPIO(int chip, int gpio) {
    if (chip == 0) {
        for (auto& a : PI_PINS) {
            if (a.gpioIdx == chip && a.gpio == gpio) {
                return a;
            }
        }
        for (auto& a : PI5_PINS) {
            if (a.gpioIdx == chip && a.gpio == gpio) {
                return a;
            }
        }
    }
    return PiFacePinCapabilities::getPinByGPIO(chip, gpio);
}

std::vector<std::string> PiGPIOPinProvider::getPinNames() {
    std::vector<std::string> ret;
    for (auto& a : PI_PINS) {
        ret.push_back(a.name);
    }
    for (auto& a : PI5_PINS) {
        ret.push_back(a.name);
    }
    PiFacePinCapabilities::getPinNames(ret);
    return ret;
}
const PinCapabilities& PiGPIOPinProvider::getPinByUART(const std::string& n) {
    return PiFacePinCapabilities::getPinByUART(n);
}
