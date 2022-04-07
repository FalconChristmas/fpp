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

#include "fpp-pch.h"

#include "MCP23x17Utils.h"
#include "PiFaceUtils.h"
#include "SPIUtils.h"

//PiFace is just a strangely configured MCP23x17
// the first 8 pins on the MCP23x17 are outputs
// the second 8 pins are inputs
// It's only exposed as 8 GPIO's

PiFacePinCapabilities::PiFacePinCapabilities(const std::string& n, uint32_t kg,
                                             const PinCapabilities& read,
                                             const PinCapabilities& write) :
    PinCapabilitiesFluent(n, kg),
    readPin(read),
    writePin(write) {
}

int PiFacePinCapabilities::configPin(const std::string& mode,
                                     bool directionOut) const {
    if (mode == "pwm" || mode == "uart") {
        return 0;
    }
    if (!directionOut) {
        readPin.configPin(mode, false);
    }
    return 0;
}

bool PiFacePinCapabilities::getValue() const {
    return readPin.getValue();
}

void PiFacePinCapabilities::setValue(bool i) const {
    writePin.setValue(i);
}

static std::vector<PiFacePinCapabilities> PIFACE_PINS;
static std::vector<PiFacePinCapabilities> PIFACE_PINS_HIDDEN;

void PiFacePinCapabilities::Init() {
    //the old wiringPi put the MCP23x17 pins 16 above the PiFace pins
    MCP23x17PinCapabilities::Init(216);
    if (MCP23x17PinCapabilities::getPinByGPIO(216).ptr()) {
        //MCP23x17 detected, add our pins
        for (int x = 0; x < 8; x++) {
            const PinCapabilities& write = MCP23x17PinCapabilities::getPinByGPIO(216 + x);
            const PinCapabilities& read = MCP23x17PinCapabilities::getPinByGPIO(216 + 8 + x);

            write.configPin("gpio", true);
            read.configPin("gpio_pu", false);

            std::string name = "PiFace-" + std::to_string(x + 1);
            PIFACE_PINS.push_back(PiFacePinCapabilities(name, 200 + x, read, write));
            PIFACE_PINS_HIDDEN.push_back(PiFacePinCapabilities(name, 200 + 8 + x, read, write));
        }
    }
}
void PiFacePinCapabilities::getPinNames(std::vector<std::string>& ret) {
    MCP23x17PinCapabilities::getPinNames(ret);
    for (auto& a : PIFACE_PINS) {
        ret.push_back(a.name);
    }
}

const PinCapabilities& PiFacePinCapabilities::getPinByName(const std::string& name) {
    for (auto& a : PIFACE_PINS) {
        if (a.name == name) {
            return a;
        }
    }
    return MCP23x17PinCapabilities::getPinByName(name);
}
const PinCapabilities& PiFacePinCapabilities::getPinByGPIO(int i) {
    for (auto& a : PIFACE_PINS) {
        if (a.kernelGpio == i) {
            return a;
        }
    }
    for (auto& a : PIFACE_PINS_HIDDEN) {
        if (a.kernelGpio == i) {
            return a;
        }
    }
    return MCP23x17PinCapabilities::getPinByGPIO(i);
}
const PinCapabilities& PiFacePinCapabilities::getPinByUART(const std::string& n) {
    return MCP23x17PinCapabilities::getPinByUART(n);
}
