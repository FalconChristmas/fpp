
#ifndef __FPP_WIRING_PI_GPIO__
#define __FPP_WIRING_PI_GPIO__

#include "GPIOUtils.h"

class WPPinCapabilities : public PinCapabilitiesFluent<WPPinCapabilities> {
public:
    WPPinCapabilities(const std::string &n, uint32_t kg);

    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const;
    
    virtual bool getValue() const;
    virtual void setValue(bool i) const;
    
    virtual bool setupPWM(int maxValueNS = 25500) const;
    virtual void setPWMValue(int valueNS) const;
    
    virtual int getPWMRegisterAddress() const { return 0;};
    virtual bool supportPWM() const { return true; };
    
    static void Init();
    static const WPPinCapabilities &getPinByName(const std::string &name);
    static const WPPinCapabilities &getPinByGPIO(int i);
};

#endif
