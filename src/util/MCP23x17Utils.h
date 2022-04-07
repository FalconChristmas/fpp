#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "GPIOUtils.h"

class MCP23x17PinCapabilities : public PinCapabilitiesFluent<MCP23x17PinCapabilities> {
public:
    MCP23x17PinCapabilities(const std::string& n, uint32_t kg, int base);
    int gpioBase;

    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;

    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;

    virtual bool setupPWM(int maxValueNS = 25500) const override { return false; }
    virtual void setPWMValue(int valueNS) const override {}

    virtual int getPWMRegisterAddress() const override { return 0; };
    virtual bool supportPWM() const override { return false; };

    virtual bool supportsPullUp() const override { return true; }
    virtual bool supportsPullDown() const override { return false; }

    static void Init(int base);
    static void getPinNames(std::vector<std::string>& r);
    static const PinCapabilities& getPinByName(const std::string& name);
    static const PinCapabilities& getPinByGPIO(int i);
    static const PinCapabilities& getPinByUART(const std::string& n);
};
