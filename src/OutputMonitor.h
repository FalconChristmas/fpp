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

#include <httpserver.hpp>

#include "util/GPIOUtils.h"

class PortPinInfo;

class OutputMonitor : public httpserver::http_resource {
public:
    static OutputMonitor INSTANCE;

    void Initialize(std::map<int, std::function<bool(int)>>& callbacks);

    void AddPortConfiguration(const std::string &name, const Json::Value &config, bool enabled = true);
    const PinCapabilities *AddOutputPin(const std::string &name, const std::string &pin);

    void EnableOutputs();
    void DisableOutputs();

    void AutoEnableOutputs();
    void AutoDisableOutputs();

    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request& req) override;
private:
    OutputMonitor();
    ~OutputMonitor();

    std::list<const PinCapabilities *> pullHighOutputPins;
    std::list<const PinCapabilities *> pullLowOutputPins;

    std::map<std::string, const PinCapabilities *> fusePins;
    std::list<PortPinInfo*> portPins;
};