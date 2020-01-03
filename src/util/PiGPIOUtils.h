
#ifndef __FPP_PiGPIO_GPIO__
#define __FPP_PiGPIO_GPIO__

#include <vector>

#include "GPIOUtils.h"

class PiGPIOPinCapabilities : public PinCapabilitiesFluent<PiGPIOPinCapabilities> {
public:
    PiGPIOPinCapabilities(const std::string &n, uint32_t kg);
    
    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;
    
    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;
    
    virtual bool setupPWM(int maxValueNS = 25500) const override;
    virtual void setPWMValue(int valueNS) const override;
    
    virtual int getPWMRegisterAddress() const override { return 0;};
    virtual bool supportPWM() const override { return true; };
    
    static void Init();
    static const PinCapabilities &getPinByName(const std::string &name);
    static const PinCapabilities &getPinByGPIO(int i);
    
    static std::vector<std::string> getPinNames();

};

#endif
