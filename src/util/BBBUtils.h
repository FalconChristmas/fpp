#pragma once

#include <string>
#include <vector>

#include "GPIOUtils.h"

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

class BBBPinCapabilities : public PinCapabilitiesFluent<BBBPinCapabilities> {
public:
    BBBPinCapabilities(const std::string& n, uint32_t k);

    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;

    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;
    virtual int openValueForPoll() const;

    virtual bool setupPWM(int maxValueNS = 25500) const override;
    virtual void setPWMValue(int valueNS) const override;
    virtual int getPWMRegisterAddress() const override;

    virtual bool supportPWM() const override;
};
class BBBPinProvider : public PinCapabilitiesProvider {
    void Init();
    const PinCapabilities& getPinByName(const std::string& name);
    const PinCapabilities& getPinByGPIO(int i);
    const PinCapabilities& getPinByUART(const std::string& n);

    std::vector<std::string> getPinNames();
};
