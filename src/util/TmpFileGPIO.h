
#ifndef __FPP_TMPFILE_PI_GPIO__
#define __FPP_TMPFILE_PI_GPIO__

#include "GPIOUtils.h"
#include <vector>

class TmpFilePinCapabilities : public PinCapabilitiesFluent<TmpFilePinCapabilities> {
public:
    TmpFilePinCapabilities(const std::string &n, uint32_t kg);

    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;
    
    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;
   
    virtual bool setupPWM(int maxValueNS = 25500) const override { return false; };
    virtual void setPWMValue(int valueNS) const override {};

    virtual bool supportsPullDown() const override { return false; }
    virtual bool supportsPullUp() const override { return false; }
    virtual bool supportsGpiod() const override { return false; }

    virtual int getPWMRegisterAddress() const override { return 0;};
    virtual bool supportPWM() const override { return false; };
    
    static void Init();
    static const TmpFilePinCapabilities &getPinByName(const std::string &name);
    static const TmpFilePinCapabilities &getPinByGPIO(int i);
    static const TmpFilePinCapabilities &getPinByUART(const std::string &n);

    static std::vector<std::string> getPinNames();

    std::string filename;
};

#endif
