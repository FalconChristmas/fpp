#include <fcntl.h>
#include <unistd.h>
#include <stropts.h>

#include <vector>

#include "bcm2835.h"
#include "PiGPIOUtils.h"
#include "PiFaceUtils.h"

PiGPIOPinCapabilities::PiGPIOPinCapabilities(const std::string &n, uint32_t kg)
: PinCapabilitiesFluent(n, kg) {
}


int PiGPIOPinCapabilities::configPin(const std::string& mode,
                                 bool directionOut) const {
    if (mode == "pwm" && pwm != -1) {
        bcm2835_gpio_fsel(kernelGpio, BCM2835_GPIO_FSEL_ALT5); //ALT5 is the PWM
        return 0;
    }
    
    if (mode == "pwm" || mode == "uart") {
        return 0;
    }
    
    bcm2835_gpio_fsel(kernelGpio, directionOut ? BCM2835_GPIO_FSEL_OUTP : BCM2835_GPIO_FSEL_INPT);
    if (mode == "gpio_pu") {
        bcm2835_gpio_set_pud(kernelGpio, BCM2835_GPIO_PUD_UP);
    } else if (mode == "gpio_pd") {
        bcm2835_gpio_set_pud(kernelGpio, BCM2835_GPIO_PUD_DOWN);
    } else {
        bcm2835_gpio_set_pud(kernelGpio, BCM2835_GPIO_PUD_OFF);
    }
    return 0;
}

bool PiGPIOPinCapabilities::getValue() const {
    return bcm2835_gpio_lev(kernelGpio);
}
void PiGPIOPinCapabilities::setValue(bool i) const {
    if (i) {
        bcm2835_gpio_set(kernelGpio);
    } else {
        bcm2835_gpio_clr(kernelGpio);
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

static std::vector<PiGPIOPinCapabilities> PI_PINS;


void PiGPIOPinCapabilities::Init() {
    if (bcm2835_init()) {
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-3", 2).setI2C(1));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-5", 3).setI2C(1));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-7", 4));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-8", 14));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-10", 15));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-11", 17));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-12", 18).setPwm(0, 0));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-13", 27).setPwm(1, 0));
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
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-33", 13));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-35", 19).setPwm(1, 0));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-36", 16));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-37", 26));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-38", 20));
        PI_PINS.push_back(PiGPIOPinCapabilities("P1-40", 21));
    }
    PiFacePinCapabilities::Init();
}
const PinCapabilities &PiGPIOPinCapabilities::getPinByName(const std::string &name) {
    for (auto &a : PI_PINS) {
        if (a.name == name) {
            return a;
        }
    }
    return PiFacePinCapabilities::getPinByName(name);
}
const PinCapabilities &PiGPIOPinCapabilities::getPinByGPIO(int i) {
    for (auto &a : PI_PINS) {
        if (a.kernelGpio == i) {
            return a;
        }
    }
    return PiFacePinCapabilities::getPinByGPIO(i);
}

std::vector<std::string> PiGPIOPinCapabilities::getPinNames() {
    std::vector<std::string> ret;
    for (auto &a : PI_PINS) {
        ret.push_back(a.name);
    }
    PiFacePinCapabilities::getPinNames(ret);
    return ret;
}
const PinCapabilities &PiGPIOPinCapabilities::getPinByUART(const std::string &n) {
    return PiFacePinCapabilities::getPinByUART(n);
}
