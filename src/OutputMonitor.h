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

#include <atomic>
#include <functional>
#include "fpp-json-fwd.h"
#include <list>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "fpphttp_types.h"

#include "util/GPIOUtils.h"

class PortPinInfo;

class OutputMonitor {
public:
    static OutputMonitor INSTANCE;

    void Initialize(std::map<int, std::function<bool(int)>>& callbacks);
    void Cleanup();

    void RemovePortConfiguration(int port, const Json::Value& config);
    void AddPortConfiguration(int port, const Json::Value& config, bool enabled = true);
    const PinCapabilities* AddOutputPin(const std::string& name, const std::string& pin, bool addToList = true);

    void EnableOutputs();
    void DisableOutputs();

    void SetOutput(const std::string& port, bool on);

    void AutoEnableOutputs();
    void AutoDisableOutputs();

    void GetCurrentPortStatusJson(Json::Value& result);

    HttpResponsePtr render_GET(const HttpRequestPtr& req);

    int getGroupCount() const { return numGroups; }
    void lockToGroup(int i);
    bool isPortInGroup(int group, int port);
    std::vector<float> GetPortCurrentValues();
    void SetPixelCount(int port, int pc, int cc = -1);
    int GetPixelCount(int port);

    void checkPixelCounts(const std::string& portList, const std::string& action, int sensitivy);

    void setSmartReceiverInfo(int port, int index, bool enabled, bool tripped, int current, int pixelCount);
    void setSmartReceiverEventCallback(std::function<void(int port, int index, const std::string& cmd)>&& f);

private:
    OutputMonitor();
    ~OutputMonitor();

    std::list<const PinCapabilities*> pullHighOutputPins;
    std::list<const PinCapabilities*> pullLowOutputPins;

    std::map<std::string, const PinCapabilities*> fusePins;
    std::vector<PortPinInfo*> portPins;
    std::list<PortPinInfo*> eFuseRetries;
    bool retryTimerRunning = false;
    // Whether the enable pins are currently supposed to be driven on.  Ports
    // are re-registered whenever a string config is reloaded, which happens
    // while output is running (an xLights auto-upload lands after "output to
    // lights" has already started the output thread), so AddPortConfiguration
    // has to bring a port up in the state the monitor is already in rather
    // than assume the pre-output state.
    std::atomic<bool> outputsEnabled{ false };
    // Guards portPins, the pull-high/low pin lists, fusePins, eFuseRetries,
    // pendingEFusePresets, and srCallback.  Writers (config reload, the eFuse
    // GPIO callbacks, retry processing, enable/disable) take it exclusive;
    // status readers (HTTP/MQTT port status, the pixel-count tester) take it
    // shared.  Presets and the smart-receiver callback must NOT be invoked
    // while it is held: command presets run synchronously and can re-enter
    // EnableOutputs/DisableOutputs/SetOutput on the same thread.
    std::shared_mutex gpioLock;
    int numGroups = 1;
    int curGroup = -1;

    int eFuseRetryCount = 0;
    int eFuseRetryInterval = 100;

    std::function<void(int port, int index, const std::string& cmd)> srCallback;
    // Port names whose EFUSE_TRIGGERED preset still needs to fire.  Queued by
    // addEFuseWarning() (called with gpioLock held) and drained by
    // triggerEFusePresets() after the lock is released.
    std::vector<std::string> pendingEFusePresets;

    const PinCapabilities* addOutputPinInternal(const std::string& name, const std::string& pin, bool addToList);
    void addEFuseWarning(PortPinInfo* port, int rec = 0);
    void clearEFuseWarning(PortPinInfo* port, int rec = 0);
    bool checkEFuseRetry(PortPinInfo* port);
    void processRetries();
    void triggerEFusePresets();
};
