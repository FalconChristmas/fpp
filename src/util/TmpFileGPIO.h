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

#include "GPIOUtils.h"
#include <vector>

class TmpFilePinCapabilities : public PinCapabilitiesFluent<TmpFilePinCapabilities> {
public:
    TmpFilePinCapabilities(const std::string& n, uint32_t kg);

    virtual int configPin(const std::string& mode = "gpio",
                          bool directionOut = true) const override;

    virtual bool getValue() const override;
    virtual void setValue(bool i) const override;

    virtual bool setupPWM(int maxValueNS = 25500) const override { return false; };
    virtual void setPWMValue(int valueNS) const override{};

    virtual bool supportsPullDown() const override { return false; }
    virtual bool supportsPullUp() const override { return false; }
    virtual bool supportsGpiod() const override { return false; }

    virtual int getPWMRegisterAddress() const override { return 0; };
    virtual bool supportPWM() const override { return false; };

    std::string filename;
};
class TmpFilePinProvider : public PinCapabilitiesProvider {
public:
    TmpFilePinProvider() {}
    virtual ~TmpFilePinProvider() {}

    void Init();
    const PinCapabilities& getPinByName(const std::string& name);
    const PinCapabilities& getPinByGPIO(int i);
    const PinCapabilities& getPinByUART(const std::string& n);

    std::vector<std::string> getPinNames();
};
