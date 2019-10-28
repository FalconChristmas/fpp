
#ifndef __FPP_PiGPIO_GPIO__
#define __FPP_PiGPIO_GPIO__

#include <vector>

#include "GPIOUtils.h"

class PiGPIOPinCapabilities : public PinCapabilitiesFluent<PiGPIOPinCapabilities> {
public:
    PiGPIOPinCapabilities(const std::string &n, uint32_t kg);
    
    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const;
    
    virtual bool getValue() const;
    virtual void setValue(bool i) const;
    
    virtual bool setupPWM(int maxValueNS = 25500) const;
    virtual void setPWMValue(int valueNS) const;
    
    virtual int getPWMRegisterAddress() const { return 0;};
    virtual bool supportPWM() const { return true; };
    
    static void Init();
    static const PinCapabilities &getPinByName(const std::string &name);
    static const PinCapabilities &getPinByGPIO(int i);
    
    static std::vector<std::string> getPinNames();

};

#endif
