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

#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>

#include "PiFaceUtils.h"
#include "PiGPIOUtils.h"
#include "Warnings.h"
#include "common_mini.h"
#include "log.h"

static bool isPi5() {
    static bool pi5 = [] {
        std::string model = GetFileContents("/proc/device-tree/model");
        return startsWith(model, "Raspberry Pi 5") ||
               startsWith(model, "Raspberry Pi Compute Module 5");
    }();
    return pi5;
}

// Kernel 6.18 enabled strict pinmux mode for both pinctrl-rp1 and pinctrl-bcm2835
// (raspberrypi/linux "pinctrl: rp1: enable strict pinmux mode", raspberrypi/linux#5870).
// A header pin that a peripheral node claims in the device tree -- i2c1 on P1-3/5, spi0
// on P1-19/21/23, uart0 on P1-8/10, i2s on P1-12/35/38/40 -- is now reported to gpiolib
// as in use by the kernel, and every gpiod request against it fails with EINVAL. Through
// FPP 9.x (kernel 6.12) those drivers were non-strict, so re-muxing the pad with
// "pinctrl set" and then requesting it worked; on 6.18 it does not, because "pinctrl set"
// writes FUNCSEL behind the pinctrl driver's back and leaves the kernel's claim in place.
//
// The supported way to get the pin back is to make the owning device release it, which
// unbinding its driver does. That is safe when nothing has attached to the bus (the common
// case for buttons wired straight to the header on a board with no cape), and decidedly
// unsafe when something has: a child driver's remove path can wedge in the kernel and take
// the whole bus with it. So we unbind only when the owner has no child driver bound other
// than the ones known to detach cleanly, and warn with the offending driver named otherwise.
//
// On a non-strict kernel the debugfs format has no "device " prefix, so findPinMuxOwner()
// finds nothing and none of this engages -- which is exactly right, since a non-strict
// controller hands the pin over without help.
static constexpr const char* PINMUX_DEBUG_DIR = "/sys/kernel/debug/pinctrl";

struct PinMuxOwner {
    std::string device;   // platform device holding the mux claim, e.g. "1f00074000.i2c"
    std::string function; // mux function it selected, e.g. "i2c1"
    bool hog = false;
};

// The pin controllers that drive the 40-pin header. Other pinctrl instances (the bcm2712
// "aon" controller, expanders) enumerate overlapping pin numbers and must not be consulted.
static bool isHeaderPinctrlDir(const std::string& dir) {
    return endsWith(dir, "-pinctrl-rp1") || endsWith(dir, "-pinctrl-bcm2711") ||
           endsWith(dir, "-pinctrl-bcm2835");
}

// debugfs files report a size of 0, so they have to be streamed rather than slurped.
static bool findPinMuxOwner(int gpio, PinMuxOwner& owner) {
    const std::string prefix = "pin " + std::to_string(gpio) + " (";
    std::error_code ec;
    std::filesystem::directory_iterator dir(PINMUX_DEBUG_DIR, ec);
    if (ec) {
        // Without debugfs there is no way to find out which device holds a pin, so a
        // claimed pin can only be reported as unavailable by the failing request itself.
        static bool warned = false;
        if (!warned) {
            warned = true;
            LogWarn(VB_GPIO, "%s is not readable; pins claimed by a kernel driver cannot be freed for GPIO use\n",
                    PINMUX_DEBUG_DIR);
        }
        return false;
    }
    for (const auto& entry : dir) {
        if (!isHeaderPinctrlDir(entry.path().filename().string())) {
            continue;
        }
        std::ifstream in(entry.path().string() + "/pinmux-pins");
        std::string line;
        while (std::getline(in, line)) {
            if (!startsWith(line, prefix)) {
                continue;
            }
            // strict format: "pin 2 (gpio2): device 1f00074000.i2c function i2c1 group gpio2"
            size_t dpos = line.find("): device ");
            if (dpos == std::string::npos) {
                return false; // UNCLAIMED, GPIO-owned, or a non-strict kernel
            }
            std::string rest = line.substr(dpos + 10);
            owner.device = rest.substr(0, rest.find(' '));
            owner.hog = rest.find(" (HOG)") != std::string::npos;
            size_t fpos = rest.find(" function ");
            if (fpos != std::string::npos) {
                std::string fn = rest.substr(fpos + 10);
                owner.function = fn.substr(0, fn.find(' '));
            }
            return true;
        }
    }
    return false;
}

// A child that is still driver-bound means the bus is in use; removing the controller then
// runs that driver's remove path, which is where an unbind can hang. spidev is the one
// child FPP creates itself and is known to detach cleanly.
static bool findBlockingChildDriver(const std::string& devPath, std::string& blocker) {
    static const std::set<std::string> safeChildDrivers = { "spidev" };
    const std::string ownDriver = devPath + "/driver";
    std::error_code ec;
    auto end = std::filesystem::recursive_directory_iterator();
    for (auto it = std::filesystem::recursive_directory_iterator(
             devPath, std::filesystem::directory_options::skip_permission_denied, ec);
         it != end; it.increment(ec)) {
        if (ec) {
            break;
        }
        if (it.depth() >= 5) {
            it.disable_recursion_pending();
        }
        const std::filesystem::path& p = it->path();
        if (p.filename() != "driver" || p.string() == ownDriver) {
            continue;
        }
        std::filesystem::path target = std::filesystem::read_symlink(p, ec);
        if (ec) {
            ec.clear();
            continue;
        }
        std::string driver = target.filename().string();
        if (!safeChildDrivers.count(driver)) {
            blocker = driver;
            return true;
        }
    }
    return false;
}

// Hand a pin back from the peripheral that claimed it in the device tree so it can be
// used as a GPIO. Returns true if the pin is usable when it returns.
static bool releasePinFromKernelDriver(int gpio, const std::string& pinName) {
    // configPin() is reachable from the REST API as well as from startup, so the
    // once-per-pin bookkeeping needs to be serialized.
    static std::mutex handledLock;
    static std::set<int> handled;
    std::unique_lock<std::mutex> lock(handledLock);
    if (handled.count(gpio)) {
        return true; // already dealt with (either freed or warned about) this boot
    }

    PinMuxOwner owner;
    if (!findPinMuxOwner(gpio, owner)) {
        return true; // unclaimed, or a kernel that doesn't enforce the claim
    }
    if (owner.hog || owner.function == "gpio") {
        // A hog belongs to the pin controller itself, and a pin muxed as the GPIO
        // function stays requestable under strict mode via .function_is_gpio.
        return true;
    }
    handled.insert(gpio);

    const std::string devPath = "/sys/bus/platform/devices/" + owner.device;
    std::error_code ec;
    std::filesystem::path driverLink = std::filesystem::read_symlink(devPath + "/driver", ec);
    if (ec) {
        std::string w = "Pin " + pinName + " is claimed by " + owner.device + " (" + owner.function +
                        ") and cannot be used as a GPIO; no driver to unbind";
        WarningHolder::AddWarning(51, w);
        LogWarn(VB_GPIO, "%s\n", w.c_str());
        return false;
    }
    std::string driver = driverLink.filename().string();

    std::string blocker;
    if (findBlockingChildDriver(devPath, blocker)) {
        std::string w = "Pin " + pinName + " is claimed by " + owner.function + " (" + owner.device +
                        "), which is in use by the " + blocker +
                        " driver. Disable " + owner.function + " in config.txt to use this pin as a GPIO.";
        WarningHolder::AddWarning(51, w);
        LogWarn(VB_GPIO, "%s\n", w.c_str());
        return false;
    }

    LogInfo(VB_GPIO, "Pin %s is claimed by %s (%s); unbinding %s to free it for GPIO use\n",
            pinName.c_str(), owner.function.c_str(), owner.device.c_str(), driver.c_str());
    std::ofstream unbind("/sys/bus/platform/drivers/" + driver + "/unbind");
    unbind << owner.device;
    unbind.close();

    PinMuxOwner check;
    if (findPinMuxOwner(gpio, check)) {
        std::string w = "Pin " + pinName + " is claimed by " + check.function + " (" + check.device +
                        ") and could not be freed for GPIO use";
        WarningHolder::AddWarning(51, w);
        LogWarn(VB_GPIO, "%s\n", w.c_str());
        return false;
    }
    return true;
}

class PiGPIODCapabilities : public GPIODCapabilities {
public:
    PiGPIODCapabilities(const std::string& n, uint32_t kg) :
        GPIODCapabilities(n, kg) {
        gpioIdx = getPinctrlRpiChip();
        gpioName = pinctrlRpiChipName;
        gpio = kg;
    }

    virtual ~PiGPIODCapabilities() {
        if (dutyFile != nullptr) {
            fclose(dutyFile);
        }
    }

    PiGPIODCapabilities& setResetMode(const std::string& mode) {
        resetMode = mode;
        return *this;
    }
    PiGPIODCapabilities& setPwm(int p, int sub) {
        GPIODCapabilities::setPwm(p, sub);
        return *this;
    }
    PiGPIODCapabilities& setUART(const std::string& u, const std::string& um) {
        uart = u;
        uartMode = um;
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

        // Under strict pinmux a pin the device tree handed to a peripheral has to be
        // reclaimed from that driver before any gpiod request on it can succeed; re-muxing
        // the pad is not enough. See releasePinFromKernelDriver() above.
        if (startsWith(mode, "gpio")) {
            releasePinFromKernelDriver(gpio, name);
        }

        if (mode == "pwm" && pwm != -1) {
            // alt3 is pwm
            char buf[256];
            snprintf(buf, 256, "/usr/bin/pinctrl set %d a3", gpio);
            system(buf);
            return 0;
        }

        if (mode == "dpi") {
            // Pi 5 (RP1): DPI is alt1, Pi 4 and earlier (BCM2711/BCM2835): DPI is alt2
            const char* alt = isPi5() ? "a1" : "a2";
            char buf[256];
            snprintf(buf, 256, "/usr/bin/pinctrl set %d %s", gpio, alt);
            system(buf);
            return 0;
        }
        if (mode == "uart" && !uart.empty() && !uartMode.empty()) {
            char buf[256];
            snprintf(buf, 256, "/usr/bin/pinctrl set %d %s", gpio, uartMode.c_str());
            system(buf);
            return 0;
        }
        if (mode == "gpio_od") {
            // Open-drain (bit-banged I2C): set the pad to GPIO function with the
            // input buffer + pull-up enabled via pinctrl, then create the
            // open-drain gpiod request that actually drives/senses the line.
            char buf[256];
            snprintf(buf, 256, "/usr/bin/pinctrl set %d ip pu", gpio);
            system(buf);
            return GPIODCapabilities::configPin("gpio_od", false, desc);
        }
        // "no" is a releasePin()-only sentinel meaning "leave this pin alone on
        // release" (used for PWM-capable pins like P1-35/GPIO19 on Pi5, so a blind
        // reset doesn't clobber an active PWM alt-function). It must NOT be treated
        // like the real I2C alt-function sentinels "a0"/"a3" here -- doing so skips
        // GPIODCapabilities::configPin() entirely, so lastBias/lastDesc never get
        // set and the subsequent requestEventFile() request comes up with no pull-up
        // bias and a bare consumer name. That's what made the OLED HAT's Down button
        // (P1-35) come up floating/unresponsive specifically on Pi5.
        //
        // gpio_pu/gpio_pd must never take this shortcut, on any platform: it's a
        // bare direction-only pinctrl call with no bias support, so a pin whose
        // resetMode still has the "a0"/"a3" class default (i.e. nobody called
        // setResetMode() for it) silently comes up floating despite the caller
        // explicitly asking for a pull-up/down. Plain "gpio" has no bias to lose,
        // so it's the only mode allowed to keep the fast path here.
        if (startsWith(mode, "gpio") && mode != "gpio_pu" && mode != "gpio_pd" &&
            (resetMode == "a0" || resetMode == "a3")) {
            char buf[256];
            snprintf(buf, 256, "/usr/bin/pinctrl set %d %s", gpio, directionOut ? "op" : "ip");
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
            if (!dir) {
                return false;
            }
            fprintf(dir, "%d", subPwm);
            fclose(dir);

            snprintf(dir_name, sizeof(dir_name), "/sys/class/pwm/pwmchip0/pwm%d/period", subPwm);
            dir = fopen(dir_name, "w");
            if (!dir) {
                return false;
            }
            fprintf(dir, "%d", maxValue);
            fclose(dir);

            snprintf(dir_name, sizeof(dir_name), "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", subPwm);
            dutyFile = fopen(dir_name, "w");
            if (!dutyFile) {
                return false;
            }
            fprintf(dutyFile, "0");
            fflush(dutyFile);

            snprintf(dir_name, sizeof(dir_name), "/sys/class/pwm/pwmchip0/pwm%d/enable", subPwm);
            dir = fopen(dir_name, "w");
            if (!dir) {
                return false;
            }
            fprintf(dir, "1");
            fclose(dir);
            return true;
        }
        return false;
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
                        if (info.label() == "pinctrl-rpi1" || info.label() == "pinctrl-bcm2711" || info.label() == "pinctrl-bcm2835") {
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
                if (label == "pinctrl-rp1" || label == "pinctrl-bcm2711" || label == "pinctrl-bcm2835") {
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
    std::string uartMode;
};
int PiGPIODCapabilities::pinctrlRpiChip = -1;
std::string PiGPIODCapabilities::pinctrlRpiChipName;
static std::vector<PiGPIODCapabilities> PI_PINS;

void PiGPIOPinProvider::Init() {
    std::string ipOrNo = isPi5() ? "no" : "ip";
    std::string i2c = isPi5() ? "a3" : "a0";
    PI_PINS.push_back(PiGPIODCapabilities("P1-3", 2).setResetMode(i2c));
    PI_PINS.push_back(PiGPIODCapabilities("P1-5", 3).setResetMode(i2c));
    PI_PINS.push_back(PiGPIODCapabilities("P1-7", 4));
    PI_PINS.push_back(PiGPIODCapabilities("P1-8", 14).setUART("ttyAMA0-tx", isPi5() ? "a4" : "a0"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-10", 15).setUART("ttyAMA0-rx", isPi5() ? "a4" : "a0"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-11", 17).setResetMode(ipOrNo));
    PI_PINS.push_back(PiGPIODCapabilities("P1-12", 18).setPwm(0, 0).setResetMode(ipOrNo));
    PI_PINS.push_back(PiGPIODCapabilities("P1-13", 27).setPwm(0, 1).setResetMode(ipOrNo));
    PI_PINS.push_back(PiGPIODCapabilities("P1-15", 22).setResetMode(ipOrNo));
    PI_PINS.push_back(PiGPIODCapabilities("P1-16", 23).setResetMode(ipOrNo));
    PI_PINS.push_back(PiGPIODCapabilities("P1-18", 24).setResetMode(ipOrNo));
    PI_PINS.push_back(PiGPIODCapabilities("P1-19", 10));
    PI_PINS.push_back(PiGPIODCapabilities("P1-21", 9));
    PI_PINS.push_back(PiGPIODCapabilities("P1-22", 25).setResetMode(ipOrNo));
    PI_PINS.push_back(PiGPIODCapabilities("P1-23", 11));
    PI_PINS.push_back(PiGPIODCapabilities("P1-24", 8).setResetMode("op"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-26", 7).setResetMode("op"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-27", 0).setResetMode("ip"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-28", 1).setResetMode("ip"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-29", 5).setResetMode("ip"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-31", 6).setResetMode("ip"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-32", 12).setResetMode("ip").setPwm(0, 0));
    PI_PINS.push_back(PiGPIODCapabilities("P1-33", 13).setResetMode("ip"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-35", 19).setResetMode(ipOrNo).setPwm(0, 1));
    PI_PINS.push_back(PiGPIODCapabilities("P1-36", 16).setResetMode(ipOrNo));
    PI_PINS.push_back(PiGPIODCapabilities("P1-37", 26).setResetMode("ip"));
    PI_PINS.push_back(PiGPIODCapabilities("P1-38", 20).setResetMode(ipOrNo));
    PI_PINS.push_back(PiGPIODCapabilities("P1-40", 21).setResetMode(ipOrNo));

    PiFacePinCapabilities::Init();
}
const PinCapabilities& PiGPIOPinProvider::getPinByName(const std::string& name) {
    for (auto& a : PI_PINS) {
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
    }
    return PiFacePinCapabilities::getPinByGPIO(chip, gpio);
}

std::vector<std::string> PiGPIOPinProvider::getPinNames() {
    std::vector<std::string> ret;
    for (auto& a : PI_PINS) {
        ret.push_back(a.name);
    }
    PiFacePinCapabilities::getPinNames(ret);
    return ret;
}
const PinCapabilities& PiGPIOPinProvider::getPinByUART(const std::string& n) {
    for (auto& a : PI_PINS) {
        if (a.uart == n) {
            return a;
        }
    }
    return PiFacePinCapabilities::getPinByUART(n);
}
