#ifndef __BBB_UTILS__
#define __BBB_UTILS__

#include <string>

#include <BBBPruUtils.h>



int configBBBPin(const std::string &name,
                 int gpio, int pin,
                 const std::string &mode,
                 const std::string &direction);

inline int configBBBPin(const std::string &name,
                 int gpio, int pin,
                 const std::string &mode) {
    return configBBBPin(name, gpio, pin, mode, "out");
}

int configBBBPin(int kgpio,
                 const std::string& mode,
                 const std::string &direction);
inline int configBBBPin(int kgpio, const std::string& mode) {
    return configBBBPin(kgpio, mode, "out");
}


enum BeagleBoneType {
    Black,
    BlackWireless,
    Green,
    GreenWireless,
    PocketBeagle,
    SanCloudEnhanced,
    Unknown,
};

BeagleBoneType getBeagleBoneType();


class PinCapabilities {
public:
    PinCapabilities(const std::string &n, uint8_t k);
    PinCapabilities(const std::string &n, uint8_t k, uint8_t pru);
    std::string name;
    uint8_t kernelGpio;
    uint8_t gpio;
    uint8_t pin;
    int8_t pruout;
    
    int8_t pwm;
    int8_t subPwm;
    
    PinCapabilities& setPwm(int pwm, int sub);
};

const PinCapabilities &getBBBPinByName(const std::string &name);
const PinCapabilities &getBBBPinKgpio(int i);

bool getBBBPinValue(int kio);
void setBBBPinValue(int kio, bool v);

bool supportsPWMOnBBBPin(int kio);
bool setupBBBPinPWM(int kio);
void setBBBPinPWMValue(int kio, int value);
#endif
