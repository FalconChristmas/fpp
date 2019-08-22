#include <fcntl.h>
#include <unistd.h>
#include <stropts.h>

#include <vector>
#include <pigpio.h>

#include "PiGPIOUtils.h"
#include "PiFaceUtils.h"

PiGPIOPinCapabilities::PiGPIOPinCapabilities(const std::string &n, uint32_t kg)
: PinCapabilitiesFluent(n, kg) {
}


int PiGPIOPinCapabilities::configPin(const std::string& mode,
                                 bool directionOut) const {
    gpioSetMode(kernelGpio, directionOut ? PI_OUTPUT : PI_INPUT);
    
    if (mode == "gpio_pu") {
        gpioSetPullUpDown(kernelGpio, PI_PUD_UP);
    } else if (mode == "gpio_pd") {
        gpioSetPullUpDown(kernelGpio, PI_PUD_DOWN);
    } else {
        gpioSetPullUpDown(kernelGpio, PI_PUD_OFF);
    }
    return 0;
}

bool PiGPIOPinCapabilities::getValue() const {
    return gpioRead(kernelGpio);
}
void PiGPIOPinCapabilities::setValue(bool i) const {
    gpioWrite(kernelGpio, i);
}

bool PiGPIOPinCapabilities::setupPWM(int maxValueNS) const {
    gpioSetMode(kernelGpio, PI_OUTPUT);
    gpioSetPWMfrequency(kernelGpio, 800);
    gpioSetPWMrange(kernelGpio, maxValueNS/10);
    gpioPWM(kernelGpio, 0);
    return true;
}
void PiGPIOPinCapabilities::setPWMValue(int valueNS) const {
    gpioPWM(kernelGpio, valueNS / 10);
}

static std::vector<PiGPIOPinCapabilities> PI_PINS;


void PiGPIOPinCapabilities::Init() {
    unlink(PI_LOCKFILE);
    gpioCfgInterfaces(PI_DISABLE_FIFO_IF | PI_DISABLE_SOCK_IF);
    gpioInitialise();
    unlink(PI_LOCKFILE);
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-3", 2).setI2C(1));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-5", 3).setI2C(1));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-7", 4));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-8", 14));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-10", 15));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-11", 17));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-12", 18));
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
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-32", 12));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-33", 13));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-35", 19));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-36", 16));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-37", 26));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-38", 20));
    PI_PINS.push_back(PiGPIOPinCapabilities("P1-40", 21));
    
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

