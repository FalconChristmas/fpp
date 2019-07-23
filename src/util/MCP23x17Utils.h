
#ifndef __FPP_MCP23x17_GPIO__
#define __FPP_MCP23x17_GPIO__

#include "GPIOUtils.h"

class MCP23x17PinCapabilities : public PinCapabilitiesFluent<MCP23x17PinCapabilities> {
public:
    MCP23x17PinCapabilities(const std::string &n, uint32_t kg, int base);
    int gpioBase;
    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const;
    
    virtual bool getValue() const;
    virtual void setValue(bool i) const;
    
    virtual bool setupPWM(int maxValueNS = 25500) const {return false;}
    virtual void setPWMValue(int valueNS) const {}
    
    virtual int getPWMRegisterAddress() const { return 0;};
    virtual bool supportPWM() const { return false; };
    
    static void Init(int base);
    static const PinCapabilities &getPinByName(const std::string &name);
    static const PinCapabilities &getPinByGPIO(int i);
};

#endif

