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

#include <functional>
#include <list>
#include <map>
#include <string>

#include "util/GPIOUtils.h"

class FusePinInfo;

class OutputMonitor {
public:
    static OutputMonitor INSTANCE;

    void Initialize(std::map<int, std::function<bool(int)>>& callbacks);

    void AddOutputPin(const std::string &name, const std::string &pin, bool highToEnable = true);
    void AddFusePin(const std::string &name, const std::string &pin, bool highIsTriggered = true, const std::string &interruptPin = "", bool interruptPinIsHigh = true);

    void EnableOutputs();
    void DisableOutputs();

    void AutoEnableOutputs();
    void AutoDisableOutputs();

private:
    std::map<std::string, const PinCapabilities *> pullHighOutputPins;
    std::map<std::string, const PinCapabilities *> pullLowOutputPins;
    std::map<std::string, std::list<std::string>> outputPinMaps;

    std::map<std::string, FusePinInfo*> fusePins;
};