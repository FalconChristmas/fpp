
#ifndef __FPPGPIOUTILS__
#define __FPPGPIOUTILS__

#include <string>
#include <vector>

#include <jsoncpp/json/json.h>

class PinCapabilities {
public:
    static void InitGPIO();
    
    PinCapabilities(const std::string &n, uint32_t k)
        : name(n), kernelGpio(k), pru(-1), pruPin(-1), pwm(-1), subPwm(-1), i2cBus(-1), gpioIdx(0), gpio(k) {}
    virtual ~PinCapabilities() {}
    
    std::string name;
    uint32_t kernelGpio;
    
    // on some platforms, the kernelGpio number may be split into multiple gpio address locations
    // these fields are the GPIO chip index and the gpio number on that chip
    uint8_t gpioIdx;
    uint8_t gpio;

    int8_t pru;
    int8_t pruPin;
    
    int8_t pwm;
    int8_t subPwm;
    
    int8_t i2cBus;
    
    std::string uart;
    

    virtual Json::Value toJSON() const;
    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const = 0;
    virtual bool supportsPullUp() const { return true; }
    virtual bool supportsPullDown() const { return true; }
    virtual bool supportsGpiod() const { return true; }

    virtual bool getValue() const = 0;
    virtual void setValue(bool i) const = 0;
    
    virtual bool setupPWM(int maxValueNS = 25500) const = 0;
    virtual void setPWMValue(int valueNS) const = 0;

    virtual bool supportPWM() const = 0;
    
    virtual int getPWMRegisterAddress() const { return 0; }

    virtual const PinCapabilities *ptr() const { return this; }
    
    static const PinCapabilities &getPinByName(const std::string &n);
    static const PinCapabilities &getPinByGPIO(int i);
    static const PinCapabilities &getPinByUART(const std::string &n);
    static std::vector<std::string> getPinNames();
protected:
    static void enableOledScreen(int i2cBus, bool enable);

};


template <class T>
class PinCapabilitiesFluent : public PinCapabilities {
public:
    PinCapabilitiesFluent(const std::string &n, uint8_t k) : PinCapabilities(n, k) {}
    virtual ~PinCapabilitiesFluent() {}

    T& setGPIO(int chip, int pin) {
        gpioIdx = chip;
        gpio = pin;
        return * (static_cast<T*>(this));
    }
    T& setPwm(int p, int sub) {
        pwm = p;
        subPwm = sub;
        return * (static_cast<T*>(this));
    }
    T& setI2C(int i2c) {
        i2cBus = i2c;
        return * (static_cast<T*>(this));
    }
    T& setPRU(int p, int pin) {
        pru = p;
        pruPin = pin;
        return * (static_cast<T*>(this));
    }
    T& setUART(const std::string &u) {
        uart = u;
        return * (static_cast<T*>(this));
    }
};

#endif
