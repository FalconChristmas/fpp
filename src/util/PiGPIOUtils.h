#pragma once

#include <vector>

#include "GPIOUtils.h"

class PiGPIOPinCapabilities : public PinCapabilitiesFluent<PiGPIOPinCapabilities> {
public:
    PiGPIOPinCapabilities(const std::string& n, uint32_t kg);

    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;

    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;

    virtual bool setupPWM(int maxValueNS = 25500) const override;
    virtual void setPWMValue(int valueNS) const override;

    virtual int getPWMRegisterAddress() const override { return 0; };
    virtual bool supportPWM() const {
        return pwm != -1;
    }
};

class PiGPIOPinProvider : public PinCapabilitiesProvider {
public:
    PiGPIOPinProvider() {}
    virtual ~PiGPIOPinProvider() {}

    void Init();
    const PinCapabilities& getPinByName(const std::string& name);
    const PinCapabilities& getPinByGPIO(int i);
    const PinCapabilities& getPinByUART(const std::string& n);

    std::vector<std::string> getPinNames();
};
