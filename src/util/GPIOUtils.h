#pragma once
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

#include <list>
#include <string>
#include <vector>

#if __has_include(<gpiod.hpp>)
#include <gpiod.hpp>
#define HASGPIOD
#endif

class PinCapabilitiesProvider;

class PinCapabilities {
public:
    static void InitGPIO(const std::string& processName, PinCapabilitiesProvider* provider);
    static void SetMultiPinValue(const std::list<const PinCapabilities*>& pins, int v);

    PinCapabilities(const std::string& n, uint32_t k) :
        name(n),
        kernelGpio(k),
        pwm(-1),
        subPwm(-1),
        i2cBus(-1),
        gpioIdx(0),
        gpio(k) {}
    virtual ~PinCapabilities() {}

    std::string name;
    uint32_t kernelGpio;

    // on some platforms, the kernelGpio number may be split into multiple gpio address locations
    // these fields are the GPIO chip index and the gpio number on that chip
    uint8_t gpioIdx;
    uint8_t gpio;

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

    virtual const PinCapabilities* ptr() const { return this; }

    virtual bool isGPIOD() const { return false; }
    virtual void releaseGPIOD() const {}

    // on some platforms (beagles), the gpiod index may not match how
    // the gpio's are layed out memory.  Allow subclasses to remap
    // the indexes
    virtual int mappedGPIOIdx() const { return gpioIdx; }
    virtual int mappedGPIO() const { return gpio; }

    static const PinCapabilities& getPinByName(const std::string& n);
    static const PinCapabilities& getPinByGPIO(int i);
    static const PinCapabilities& getPinByUART(const std::string& n);
    static std::vector<std::string> getPinNames();

protected:
    static void enableOledScreen(int i2cBus, bool enable);
    static PinCapabilitiesProvider* pinProvider;
};

template<class T>
class PinCapabilitiesFluent : public PinCapabilities {
public:
    PinCapabilitiesFluent(const std::string& n, uint8_t k) :
        PinCapabilities(n, k) {}
    virtual ~PinCapabilitiesFluent() {}

    T& setGPIO(int chip, int pin) {
        gpioIdx = chip;
        gpio = pin;
        return *(static_cast<T*>(this));
    }
    T& setPwm(int p, int sub) {
        pwm = p;
        subPwm = sub;
        return *(static_cast<T*>(this));
    }
    T& setI2C(int i2c) {
        i2cBus = i2c;
        return *(static_cast<T*>(this));
    }
    T& setUART(const std::string& u) {
        uart = u;
        return *(static_cast<T*>(this));
    }
};

class GPIODCapabilities : public PinCapabilitiesFluent<GPIODCapabilities> {
public:
    GPIODCapabilities(const std::string& n, uint32_t kg, const std::string& gn = "") :
        PinCapabilitiesFluent(n, kg), gpioName(gn) {}

    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;
    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;

    virtual bool supportsPullUp() const override;
    virtual bool supportsPullDown() const override;

    virtual bool setupPWM(int maxValueNS = 25500) const override { return false; }
    virtual void setPWMValue(int valueNS) const override {}

    virtual int getPWMRegisterAddress() const override { return 0; };
    virtual bool supportPWM() const override { return false; };

    virtual bool isGPIOD() const override { return true; }
    virtual void releaseGPIOD() const override;

    std::string gpioName;
#ifdef HASGPIOD
    mutable gpiod::chip* chip = nullptr;
    mutable gpiod::line line;
    mutable int lastRequestType = 0;
#endif
};

class PinCapabilitiesProvider {
public:
    PinCapabilitiesProvider() {}
    virtual ~PinCapabilitiesProvider() {}

    virtual void Init() = 0;

    virtual const PinCapabilities& getPinByName(const std::string& n) = 0;
    virtual const PinCapabilities& getPinByGPIO(int i) = 0;
    virtual const PinCapabilities& getPinByUART(const std::string& n) = 0;
    virtual std::vector<std::string> getPinNames() = 0;
};

// No platform information on how to control pins
class NoPinCapabilities : public PinCapabilitiesFluent<NoPinCapabilities> {
public:
    NoPinCapabilities(const std::string& n, uint32_t kg) :
        PinCapabilitiesFluent(n, kg) {}

    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override { return 0; }

    virtual bool getValue() const override { return false; }
    virtual void setValue(bool i) const override {}

    virtual bool setupPWM(int maxValueNS = 25500) const override { return false; }
    virtual void setPWMValue(int valueNS) const override {}

    virtual int getPWMRegisterAddress() const override { return 0; };
    virtual bool supportPWM() const override { return false; };

    virtual const PinCapabilities* ptr() const override { return nullptr; }
};

class NoPinCapabilitiesProvider : public PinCapabilitiesProvider {
public:
    NoPinCapabilitiesProvider() {}
    virtual ~NoPinCapabilitiesProvider() {}

    void Init() {}
    const NoPinCapabilities& getPinByName(const std::string& name);
    const NoPinCapabilities& getPinByGPIO(int i);
    const NoPinCapabilities& getPinByUART(const std::string& n);
    std::vector<std::string> getPinNames() {
        return std::vector<std::string>();
    }
};
