
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
                          bool directionOut = true) const override;
    
    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;
    
    virtual bool setupPWM(int maxValueNS = 25500) const override {return false;}
    virtual void setPWMValue(int valueNS) const override {}
    
    virtual bool supportsPullUp() const override { return true; }
    virtual bool supportsPullDown() const override { return false; }

    virtual int getPWMRegisterAddress() const override { return 0;};
    virtual bool supportPWM() const override { return false; };
    
    static void Init();
    static void getPinNames(std::vector<std::string> &r);
    static const PinCapabilities &getPinByName(const std::string &name);
    static const PinCapabilities &getPinByGPIO(int i);
    static const PinCapabilities &getPinByUART(const std::string &n);

};

#endif

