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

#include <vector>

#include "GPIOUtils.h"

class PiGPIOPinProvider : public PinCapabilitiesProvider {
public:
    PiGPIOPinProvider() {}
    virtual ~PiGPIOPinProvider() {}

    void Init();
    const PinCapabilities& getPinByName(const std::string& name);
    const PinCapabilities& getPinByGPIO(int chip, int gpio);
    const PinCapabilities& getPinByUART(const std::string& n);

    std::vector<std::string> getPinNames();
};
