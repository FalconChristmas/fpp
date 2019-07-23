
#ifndef __FPP_PIFACE_GPIO__
#define __FPP_PIFACE_GPIO__

#include "GPIOUtils.h"

class PiFacePinCapabilities : public PinCapabilitiesFluent<PiFacePinCapabilities> {
public:
    PiFacePinCapabilities(const std::string &n, uint32_t kg,
                          const PinCapabilities &read,
                          const PinCapabilities &write);
    
    const PinCapabilities &readPin;
    const PinCapabilities &writePin;
    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const;
    
    virtual bool getValue() const;
    virtual void setValue(bool i) const;
    
    virtual bool setupPWM(int maxValueNS = 25500) const {return false;}
    virtual void setPWMValue(int valueNS) const {}
    
    virtual int getPWMRegisterAddress() const { return 0;};
    virtual bool supportPWM() const { return false; };
    
    static void Init();
    static const PinCapabilities &getPinByName(const std::string &name);
    static const PinCapabilities &getPinByGPIO(int i);
};

#endif

