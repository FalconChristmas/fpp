#ifndef __BBB_UTILS__
#define __BBB_UTILS__

#include <string>


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
    BBBPinCapabilities(const std::string &n, uint32_t k);
    
    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const;

    virtual bool getValue() const;
    virtual void setValue(bool i) const;
    virtual int openValueForPoll() const;
    
    virtual bool setupPWM(int maxValueNS = 25500) const;
    virtual void setPWMValue(int valueNS) const;
    virtual int getPWMRegisterAddress() const;
    
    virtual bool supportPWM() const;
    
    static void Init();
    static const BBBPinCapabilities &getPinByName(const std::string &name);
    static const BBBPinCapabilities &getPinByGPIO(int i);
};


#endif
