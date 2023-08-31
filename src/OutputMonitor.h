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
#include <mutex>
#include <string>

#include <httpserver.hpp>

#include "util/GPIOUtils.h"

class PortPinInfo;

class OutputMonitor : public httpserver::http_resource {
public:
    static OutputMonitor INSTANCE;

    void Initialize(std::map<int, std::function<bool(int)>>& callbacks);
    void Cleanup();

    void AddPortConfiguration(const std::string& name, const Json::Value& config, bool enabled = true);
    const PinCapabilities* AddOutputPin(const std::string& name, const std::string& pin);

    void EnableOutputs();
    void DisableOutputs();

    void AutoEnableOutputs();
    void AutoDisableOutputs();

    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request& req) override;

    int getGroupCount() const { return numGroups; }
    void lockToGroup(int i);
    bool isPortInGroup(int group, int port);
    std::vector<float> GetPortCurrentValues();
    void SetPixelCount(int port, int pc, int cc = -1);
    int GetPixelCount(int port);

    void checkPixelCounts(const std::string& portList, const std::string& action, int sensitivy);

private:
    OutputMonitor();
    ~OutputMonitor();

    std::list<const PinCapabilities*> pullHighOutputPins;
    std::list<const PinCapabilities*> pullLowOutputPins;

    std::map<std::string, const PinCapabilities*> fusePins;
    std::vector<PortPinInfo*> portPins;
    std::mutex gpioLock;
    int numGroups = 1;
    int curGroup = -1;
};
