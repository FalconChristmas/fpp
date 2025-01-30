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
    BeaglePlay,
    Unknown,
};

BeagleBoneType getBeagleBoneType();

class BBBPinCapabilities : public GPIODCapabilities {
public:
    BBBPinCapabilities(const std::string& n, uint32_t k);
    BBBPinCapabilities(const std::string& n, uint32_t g, uint32_t offset);

    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;

    virtual bool setupPWM(int maxValueNS = 25500) const override;
    virtual void setPWMValue(int valueNS) const override;
    virtual int getPWMRegisterAddress() const override;

    virtual int mappedGPIOIdx() const override;
    virtual int mappedGPIO() const override;

    virtual bool supportPWM() const override;
    virtual Json::Value toJSON() const override;

    BBBPinCapabilities& setPRU(int p, int pin) {
        pru = p;
        pruPin = pin;
        return *this;
    }

    BBBPinCapabilities& setGPIO(int chip, int pin) {
        gpioIdx = chip;
        gpio = pin;
        return *this;
    }
    BBBPinCapabilities& setPwm(int p, int sub) {
        pwm = p;
        subPwm = sub;
        return *this;
    }
    BBBPinCapabilities& setI2C(int i2c) {
        i2cBus = i2c;
        return *this;
    }
    BBBPinCapabilities& setUART(const std::string& u) {
        uart = u;
        return *this;
    }

    int8_t pruPin;

private:
    int8_t pru;
};
class BBBPinProvider : public PinCapabilitiesProvider {
    void Init();
    const PinCapabilities& getPinByName(const std::string& name);
    const PinCapabilities& getPinByGPIO(int i);
    const PinCapabilities& getPinByUART(const std::string& n);

    std::vector<std::string> getPinNames();
};
