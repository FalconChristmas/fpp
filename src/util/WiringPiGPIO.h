
#ifndef __FPP_WIRING_PI_GPIO__
#define __FPP_WIRING_PI_GPIO__

#include "GPIOUtils.h"
#include <vector>

class WPPinCapabilities : public PinCapabilitiesFluent<WPPinCapabilities> {
public:
    WPPinCapabilities(const std::string &n, uint32_t kg);

    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;
    
    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;
    
    virtual bool setupPWM(int maxValueNS = 25500) const override;
    virtual void setPWMValue(int valueNS) const override;
    
    virtual int getPWMRegisterAddress() const override { return 0;};
    virtual bool supportPWM() const override { return true; };
    
    static void Init();
    static const WPPinCapabilities &getPinByName(const std::string &name);
    static const WPPinCapabilities &getPinByGPIO(int i);
    static const WPPinCapabilities &getPinByUART(const std::string &n);

    static std::vector<std::string> getPinNames() { return std::vector<std::string>(); }
};

#endif
