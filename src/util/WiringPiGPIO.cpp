#include <fcntl.h>
#include <unistd.h>
#include <stropts.h>

#include <vector>
#include <wiringPi.h>
#include <piFace.h>
#include <softPwm.h>

#include "WiringPiGPIO.h"


WPPinCapabilities::WPPinCapabilities(const std::string &n, uint32_t kg)
: PinCapabilitiesFluent(n, kg) {
}


int WPPinCapabilities::configPin(const std::string& mode,
                                 bool directionOut) const {
    if (mode == "pwm" || mode == "uart") {
        return 0;
    }

    pinMode(kernelGpio, directionOut ? OUTPUT : INPUT);
    if (mode == "gpio_pu") {
        pullUpDnControl(kernelGpio, PUD_UP);
    } else if (mode == "gpio_pd") {
        pullUpDnControl(kernelGpio, PUD_DOWN);
    } else {
        pullUpDnControl(kernelGpio, PUD_OFF);
    }
    return 0;
}

bool WPPinCapabilities::getValue() const {
    return digitalRead(kernelGpio);
}
void WPPinCapabilities::setValue(bool i) const {
    digitalWrite(kernelGpio, i);
}

bool WPPinCapabilities::setupPWM(int maxValueNS) const {
    softPwmCreate(kernelGpio, 0, maxValueNS/100);
    return true;
}
void WPPinCapabilities::setPWMValue(int valueNS) const {
    softPwmWrite(kernelGpio, valueNS / 100);
}

class NullWPPinCapabilities : public WPPinCapabilities {
public:
    NullWPPinCapabilities() : WPPinCapabilities("-none-", 0) {}
    virtual const PinCapabilities *ptr() const override { return nullptr; }
};
static NullWPPinCapabilities NULL_WP_INSTANCE;



static std::vector<WPPinCapabilities> PI_PINS;


void WPPinCapabilities::Init() {
    wiringPiSetupGpio();
    int fd = -1;
    if ((fd = open("/dev/spidev0.0", O_RDWR)) >= 0) {
        close(fd);
        piFaceSetup(200); // PiFace inputs 1-8 == wiringPi 200-207
    }
    for (int x = 1; x < 41; x++) {
        int gpio = physPinToGpio(x);
        if (gpio != -1) {
            std::string p = "P1-" + std::to_string(x);
            PI_PINS.push_back(WPPinCapabilities(p, gpio));
            
            switch (x) {
                case 3:
                case 5:
                    PI_PINS.back().setI2C(1);
                    break;
                case 27:
                case 28:
                    PI_PINS.back().setI2C(0);
                    break;
            }
        }
    }
}
const WPPinCapabilities &WPPinCapabilities::getPinByName(const std::string &name) {
    for (auto &a : PI_PINS) {
        if (a.name == name) {
            return a;
        }
    }
    return NULL_WP_INSTANCE;
}
const WPPinCapabilities &WPPinCapabilities::getPinByGPIO(int i) {
    for (auto &a : PI_PINS) {
        if (a.kernelGpio == i) {
            return a;
        }
    }
    return NULL_WP_INSTANCE;
}
static const WPPinCapabilities &WPPinCapabilities::getPinByUART(const std::string &n) {
    return NULL_WP_INSTANCE;
}

