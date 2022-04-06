#pragma once

#include "GPIOUtils.h"
#include <vector>

class TmpFilePinCapabilities : public PinCapabilitiesFluent<TmpFilePinCapabilities> {
public:
    TmpFilePinCapabilities(const std::string& n, uint32_t kg);

    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;

    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;

    virtual bool setupPWM(int maxValueNS = 25500) const override { return false; };
    virtual void setPWMValue(int valueNS) const override{};

    virtual bool supportsPullDown() const override { return false; }
    virtual bool supportsPullUp() const override { return false; }
    virtual bool supportsGpiod() const override { return false; }

    virtual int getPWMRegisterAddress() const override { return 0; };
    virtual bool supportPWM() const override { return false; };

    std::string filename;
};
class TmpFilePinProvider : public PinCapabilitiesProvider {
public:
    TmpFilePinProvider() {}
    virtual ~TmpFilePinProvider() {}

    void Init();
    const PinCapabilities& getPinByName(const std::string& name);
    const PinCapabilities& getPinByGPIO(int i);
    const PinCapabilities& getPinByUART(const std::string& n);

    std::vector<std::string> getPinNames();
};
