#ifndef __BBB_UTILS__
#define __BBB_UTILS__

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
    BBBPinCapabilities(const std::string &n, uint32_t k);
    
    
    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;

    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;
    virtual int openValueForPoll() const;
    
    virtual bool setupPWM(int maxValueNS = 25500) const override;
    virtual void setPWMValue(int valueNS) const override;
    virtual int getPWMRegisterAddress() const override;
    
    virtual bool supportPWM() const override;
    
    static void Init();
    static const BBBPinCapabilities &getPinByName(const std::string &name);
    static const BBBPinCapabilities &getPinByGPIO(int i);
    static const BBBPinCapabilities &getPinByUART(const std::string &n);

    static std::vector<std::string> getPinNames();
};


#endif
