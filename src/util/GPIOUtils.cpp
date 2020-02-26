#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <thread>

#if __has_include(<gpiod.hpp>)
#include <gpiod.hpp>
#define HASGPIOD
#endif

#include "GPIOUtils.h"
#include "commands/Commands.h"


#if defined(PLATFORM_BBB)
#include "BBBUtils.h"
#define PLAT_GPIO_CLASS BBBPinCapabilities
#elif defined(PLATFORM_PI)
#include "PiGPIOUtils.h"
#define PLAT_GPIO_CLASS PiGPIOPinCapabilities
#elif defined(USEWIRINGPI)
#include "WiringPiGPIO.h"
#define PLAT_GPIO_CLASS WPPinCapabilities
#elif defined(PLATFORM_UNKNOWN) || defined(PLATFORM_DOCKER)
#include "TmpFileGPIO.h"
#define PLAT_GPIO_CLASS TmpFilePinCapabilities
#else
// No platform information on how to control pins
class NoPinCapabilities : public PinCapabilitiesFluent<NoPinCapabilities> {
public:
    NoPinCapabilities(const std::string &n, uint32_t kg) : PinCapabilitiesFluent(n, kg)
    {}
    
    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override { return 0;}
    
    virtual bool getValue() const override { return false; }
    virtual void setValue(bool i) const override {}
    
    virtual bool setupPWM(int maxValueNS = 25500) const override {return false;}
    virtual void setPWMValue(int valueNS) const override {}
    
    virtual int getPWMRegisterAddress() const override { return 0;};
    virtual bool supportPWM() const override { return false; };
    
    static void Init() {}
    static const NoPinCapabilities &getPinByName(const std::string &name);
    static const NoPinCapabilities &getPinByGPIO(int i);
    static const NoPinCapabilities &getPinByUART(const std::string &n);
    static std::vector<std::string> getPinNames();

};
class NullNoPinCapabilities : public NoPinCapabilities {
public:
    NullNoPinCapabilities() : NoPinCapabilities("-none-", 0) {}
    virtual const PinCapabilities *ptr() const override { return nullptr; }
};
static NullNoPinCapabilities NULL_PIN_INSTANCE;

const NoPinCapabilities &NoPinCapabilities::getPinByName(const std::string &name) {
    return NULL_PIN_INSTANCE;
}
const NoPinCapabilities &NoPinCapabilities::getPinByGPIO(int i) {
    return NULL_PIN_INSTANCE;
}
const NoPinCapabilities &NoPinCapabilities::getPinByUART(const std::string &n) {
    return NULL_PIN_INSTANCE;
}

std::vector<std::string> NoPinCapabilities::getPinNames() {
    return std::vector<std::string>();
}
#define PLAT_GPIO_CLASS NoPinCapabilities
#endif

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
    //this pin is i2c, we may need to tell fppoled to turn off the display
    //before we shutdown this pin because once we re-configure, i2c will
    //be unavailable and the display won't update
    int smfd = shm_open("fppoled", O_CREAT | O_RDWR, 0);
    ftruncate(smfd, 1024);
    unsigned int *status = (unsigned int *)mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, smfd, 0);
    if (i2cBus == status[0]) {
        printf("Signal to fppoled to enable/disable I2C:   Bus: %d   Enable: %d\n", i2cBus, enable);
        if (!enable) {
            //force the display off
            status[2] = 1;
            int count = 0;
            while (status[1] != 0 && count < 150) {
                count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } else {
            //allow the display to come back on
            status[2] = 0;
        }
    }
    close(smfd);
    munmap(status, 1024);
}

//the built in GPIO chips that are handled by the more optimized
//platform specific GPIO drivers
static const std::set<std::string> PLATFORM_IGNORES {
    "pinctrl-bcm2835", //raspberry pi's
    "raspberrypi-exp-gpio",
    "brcmvirt-gpio",
    "gpio-0-31", //beagles
    "gpio-32-63",
    "gpio-64-95",
    "gpio-96-127"
};
// No platform information on how to control pins
class GPIODCapabilities : public PinCapabilitiesFluent<GPIODCapabilities> {
public:
    GPIODCapabilities(const std::string &n, uint32_t kg) : PinCapabilitiesFluent(n, kg)
    {}
    
    
#ifdef HASGPIOD
    mutable gpiod::line line;
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override {
        std::string n = std::to_string(gpioIdx);
        line = gpiod::chip(n, gpiod::chip::OPEN_BY_NUMBER).get_line(gpio);
        gpiod::line_request req;
        req.consumer = "FPPD";
        if (directionOut) {
            req.request_type = gpiod::line_request::DIRECTION_OUTPUT;
        } else {
            req.request_type = gpiod::line_request::DIRECTION_INPUT;
        }
        line.request(req, 0);
        return 0;
    }
    virtual bool getValue() const override {
        return line.get_value();
    }
    virtual void setValue(bool i) const override {
        line.set_value(i ? 1 : 0);
    }
#endif
    
    
    virtual bool supportsPullUp() const override { return false; }
    virtual bool supportsPullDown() const override { return false; }

    
    
    virtual bool setupPWM(int maxValueNS = 25500) const override {return false;}
    virtual void setPWMValue(int valueNS) const override {}
    
    virtual int getPWMRegisterAddress() const override { return 0;};
    virtual bool supportPWM() const override { return false; };
    
};
static std::vector<GPIODCapabilities> GPIOD_PINS;


void PinCapabilities::InitGPIO() {
#ifdef HASGPIOD
    int chipCount = 0;
    int pinCount = 0;
    ::gpiod_chip* chip = gpiod_chip_open_by_number(0);
    if (chip != nullptr) {
        ::gpiod_chip_close(chip);
        // has at least on chip
        std::set<std::string> found;
        for (auto &a : gpiod::make_chip_iter()) {
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
#endif
    PLAT_GPIO_CLASS::Init();
}
std::vector<std::string> PinCapabilities::getPinNames() {
    std::vector<std::string> pn = PLAT_GPIO_CLASS::getPinNames();
    for (auto &a : GPIOD_PINS) {
        pn.push_back(a.name);
    }
    return pn;
}

const PinCapabilities &PinCapabilities::getPinByName(const std::string &n) {
    for (auto &a : GPIOD_PINS) {
        if (n == a.name) {
            return a;
        }
    }
    return PLAT_GPIO_CLASS::getPinByName(n);
}
const PinCapabilities &PinCapabilities::getPinByGPIO(int i) {
    for (auto &a : GPIOD_PINS) {
        if (i == a.kernelGpio) {
            return a;
        }
    }
    return PLAT_GPIO_CLASS::getPinByGPIO(i);
}
const PinCapabilities &PinCapabilities::getPinByUART(const std::string &n) {
    return PLAT_GPIO_CLASS::getPinByUART(n);
}
