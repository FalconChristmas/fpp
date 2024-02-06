#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

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

class BBBPinCapabilities : public GPIODCapabilities {
public:
    BBBPinCapabilities(const std::string& n, uint32_t k, uint32_t offset);

    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;

    virtual bool setupPWM(int maxValueNS = 25500) const override;
    virtual void setPWMValue(int valueNS) const override;
    virtual int getPWMRegisterAddress() const override;

    virtual bool supportPWM() const override;
    virtual Json::Value toJSON() const override;

    BBBPinCapabilities& setPRU(int p, int pin, uint8_t inm, uint8_t outm) {
        pru = p;
        pruPin = pin;
        pruInMode = inm;
        pruOutMode = outm;
        return *this;
    }

    BBBPinCapabilities& setGPIO(int chip, int pin) {
        gpioIdx = chip;
        gpio = pin;
        return *this;
    }
    BBBPinCapabilities& setPwm(int p, int sub, int8_t mode) {
        pwm = p;
        subPwm = sub;
        pwmMode = mode;
        return *this;
    }
    BBBPinCapabilities& setI2C(int i2c, int8_t mode) {
        i2cBus = i2c;
        i2cMode = mode;
        return *this;
    }
    BBBPinCapabilities& setUART(const std::string& u, int mode) {
        uart = u;
        uartMode = mode;
        return *this;
    }

    int8_t pruPin;

private:
    int8_t pru;
    int8_t pruInMode;
    int8_t pruOutMode;

    int8_t pwmMode;
    int8_t uartMode;
    int8_t i2cMode;

    uint8_t defaultConfig;
    uint32_t configOffset;
};
class BBBPinProvider : public PinCapabilitiesProvider {
    void Init();
    const PinCapabilities& getPinByName(const std::string& name);
    const PinCapabilities& getPinByGPIO(int i);
    const PinCapabilities& getPinByUART(const std::string& n);

    std::vector<std::string> getPinNames();
};
