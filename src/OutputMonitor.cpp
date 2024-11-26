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

#include "fpp-pch.h"

#include <cinttypes>
#include <cmath>
#include <fcntl.h>
#include <httpserver.hpp>
#include <list>
#include <map>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "Sequence.h"
#include "Timers.h"
#include "Warnings.h"
#include "common.h"
#include "gpio.h"
#include "log.h"
#include "settings.h"
#include "channeloutput/stringtesters/PixelCountStringTester.h"
#include "commands/Commands.h"
#include "sensors/Sensors.h"
#include "util/GPIOUtils.h"

#include "OutputMonitor.h"

OutputMonitor OutputMonitor::INSTANCE;

class CurrentMonitorBase {
public:
    CurrentMonitorBase() {}
    CurrentMonitorBase(const Json::Value& c) {
        currentMonitorScale = c["scale"].asFloat();
        if (c.isMember("offset")) {
            currentMonitorOffset = c["offset"].asFloat();
        }
    }
    virtual ~CurrentMonitorBase() {
    }

    float getValue() {
        float f = getRawValue();
        f -= currentMonitorOffset;
        if (f < 0) {
            f = 0;
        }
        f *= currentMonitorScale;
        return f;
    }

    virtual float getRawValue() = 0;

    float currentMonitorScale = 1.0f;
    float currentMonitorOffset = 0.0f;
};

class FileCurrentMonitor : public CurrentMonitorBase {
public:
    FileCurrentMonitor(const Json::Value& c) :
        CurrentMonitorBase(c) {
        currentMonitorFile = open(c["path"].asString().c_str(), O_NONBLOCK | O_RDONLY);
    }
    virtual ~FileCurrentMonitor() {
        if (currentMonitorFile >= 0) {
            close(currentMonitorFile);
        }
    }
    virtual float getRawValue() override {
        if (currentMonitorFile >= 0) {
            char buf[12] = { 0 };
            float f = 0;
            for (int x = 0; x < 3; x++) {
                lseek(currentMonitorFile, 0, SEEK_SET);
                read(currentMonitorFile, buf, sizeof(buf));
                f += atoi(buf);
            }
            f /= 3.0;
            return f;
        }
        return 0;
    }
    int currentMonitorFile = -1;
};
class SensorCurrentMonitor : public CurrentMonitorBase {
public:
    SensorCurrentMonitor(const Json::Value& c) :
        CurrentMonitorBase(c) {
        sensor = Sensors::INSTANCE.getSensorSource(c["sensor"].asString());
        channel = c["channel"].asInt();
        if (sensor) {
            sensor->enable(channel);
        }
    }
    virtual float getRawValue() override {
        if (sensor) {
            return sensor->getValue(channel);
        }
        return 0;
    }

    SensorSource* sensor = nullptr;
    int channel = 0;
};

class ReceiverInfo {
public:
    bool isOn = false;
    bool hasTriggered = false;
    bool enabled = false;
    int pixelCount = -1;
    int configuredCount = -1;
    float current = 0.0f;

    std::string warning;
    int retryCount = 0;
    int okCount = 0;
    long long firstTriggerTime = 0;
    long long nextRetryTime = 0;
};

class PortPinInfo {
public:
    PortPinInfo(const std::string& n, const Json::Value& c) :
        name(n), config(c) {
        setConfig(c);
    }
    ~PortPinInfo() {
        if (currentMonitor) {
            delete currentMonitor;
        }
    }

    void setConfig(const Json::Value& c) {
        config = c;
        if (config.isMember("row")) {
            row = config["row"].asInt();
        }
        if (config.isMember("col")) {
            col = config["col"].asInt();
        }
        if (config.isMember("bank")) {
            bankLabel = config["bank"].asString();
        }
    }

    std::string name;
    std::string bankLabel;
    Json::Value config;
    uint32_t group = 0;

    const PinCapabilities* enablePin = nullptr;
    bool highToEnable = true;
    const PinCapabilities* eFusePin = nullptr;
    int eFuseOKValue = 0;

    const PinCapabilities* eFuseInterruptPin = nullptr;
    CurrentMonitorBase* currentMonitor = nullptr;

    bool isSmartReceiver = false;
    ReceiverInfo receivers[6];

    int row = -1;
    int col = -1;

    void appendTo(Json::Value& result) {
        Json::Value v;
        v["name"] = name;

        if (isSmartReceiver) {
            v["smartReceivers"] = true;
            for (int x = 0; x < 6; x++) {
                if (receivers[x].enabled) {
                    std::string sri(1, (char)('A' + x));
                    v[sri]["ma"] = receivers[x].current;
                    if (receivers[x].hasTriggered) {
                        v[sri]["status"] = false;
                        v[sri]["enabled"] = false;
                    } else {
                        v[sri]["enabled"] = receivers[x].isOn;
                        v[sri]["status"] = receivers[x].enabled;
                    }
                    if (receivers[x].pixelCount >= 0 && receivers[x].pixelCount < 0xFFFF) {
                        v[sri]["pixelCount"] = receivers[x].pixelCount;
                    }
                }
            }
        } else {
            if (enablePin) {
                int pv = enablePin->getValue();
                v["enabled"] = (pv && highToEnable) || (!pv && !highToEnable);
            }
            if (receivers[0].isOn && eFusePin) {
                v["status"] = eFuseOKValue == eFusePin->getValue();
            } else {
                v["status"] = true;
            }
            if (currentMonitor) {
                float f = currentMonitor->getValue();
                int c = std::round(f);
                v["ma"] = c;
            }
            if (receivers[0].pixelCount >= 0) {
                v["pixelCount"] = receivers[0].pixelCount;
            }
        }

        if (row != -1) {
            v["row"] = row;
        }
        if (col != -1) {
            v["col"] = col;
        }
        if (!bankLabel.empty()) {
            v["bank"] = bankLabel;
        }
        result.append(v);
    }
};

class FPPEnableOutputsCommand : public Command {
public:
    static FPPEnableOutputsCommand INSTANCE;
    FPPEnableOutputsCommand() :
        Command("Outputs On") {
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        OutputMonitor::INSTANCE.EnableOutputs();
        return std::make_unique<Command::Result>("OK");
    }
};

class FPPDisableOutputsCommand : public Command {
public:
    static FPPDisableOutputsCommand INSTANCE;

    FPPDisableOutputsCommand() :
        Command("Outputs Off") {
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        OutputMonitor::INSTANCE.DisableOutputs();
        return std::make_unique<Command::Result>("OK");
    }
};

class FPPEnablePortCommand : public Command {
public:
    static FPPEnablePortCommand INSTANCE;
    FPPEnablePortCommand() :
        Command("Set Port Status") {
        args.push_back(CommandArg("Port", "string", "Port").setContentListUrl("api/fppd/ports/list"));
        args.push_back(CommandArg("on", "bool", "On"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() < 2) {
            return std::make_unique<Command::ErrorResult>("Required arguments not provided");
        }
        bool enable = args[1] == "true" || args[1] == "1";
        if (args[0] == "--ALL--") {
            if (enable) {
                OutputMonitor::INSTANCE.EnableOutputs();
            } else {
                OutputMonitor::INSTANCE.DisableOutputs();
            }
        } else {
            OutputMonitor::INSTANCE.SetOutput(args[0], enable);
        }
        return std::make_unique<Command::Result>("OK");
    }
};

class FPPCheckConfiguredPixelsCommand : public Command {
public:
    static FPPCheckConfiguredPixelsCommand INSTANCE;

    FPPCheckConfiguredPixelsCommand() :
        Command("Check Pixel Count") {
        args.push_back(CommandArg("Ports", "multistring", "Ports").setContentListUrl("api/fppd/ports/list", false).setDefaultValue("--ALL--"));
        args.push_back(CommandArg("Sensitivity", "int", "Sensitivity").setRange(0, 20).setDefaultValue("2"));
        args.push_back(CommandArg("Action", "string", "Action").setContentList({ "Warn", "Log" }).setDefaultValue("Warn"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        std::vector<std::string> nargs;
        nargs.push_back("50");
        nargs.push_back("Output Specific");
        nargs.push_back("--ALL--");
        nargs.push_back("999");
        CommandManager::INSTANCE.run("Test Start", nargs);
        Timers::INSTANCE.addPeriodicTimer("CheckPixelCount", 1000, [args]() {
            if (CurrentBasedPixelCountPixelStringTester::INSTANCE.getCurrentStatus() == CurrentBasedPixelCountPixelStringTester::Status::Complete) {
                OutputMonitor::INSTANCE.checkPixelCounts(args[0], args[2], std::atoi(args[1].c_str()));
                std::vector<std::string> args;
                CommandManager::INSTANCE.run("Test Stop", args);
                Timers::INSTANCE.stopPeriodicTimer("CheckPixelCount");
            }
        });

        return std::make_unique<Command::Result>("OK");
    }
};
FPPEnableOutputsCommand FPPEnableOutputsCommand::INSTANCE;
FPPDisableOutputsCommand FPPDisableOutputsCommand::INSTANCE;
FPPCheckConfiguredPixelsCommand FPPCheckConfiguredPixelsCommand::INSTANCE;
FPPEnablePortCommand FPPEnablePortCommand::INSTANCE;

OutputMonitor::OutputMonitor() {
}
OutputMonitor::~OutputMonitor() {
    for (auto pi : portPins) {
        delete pi;
    }
    portPins.clear();
    pullHighOutputPins.clear();
    pullLowOutputPins.clear();
    fusePins.clear();
}

void OutputMonitor::Initialize(std::map<int, std::function<bool(int)>>& callbacks) {
    eFuseRetryCount = getSettingInt("eFuseRetryCount", 0);
    eFuseRetryInterval = getSettingInt("eFuseRetryInterval", 100);

    CommandManager::INSTANCE.addCommand(&FPPEnableOutputsCommand::INSTANCE);
    CommandManager::INSTANCE.addCommand(&FPPDisableOutputsCommand::INSTANCE);
    if (!portPins.empty()) {
        CommandManager::INSTANCE.addCommand(&FPPCheckConfiguredPixelsCommand::INSTANCE);
        CommandManager::INSTANCE.addCommand(&FPPEnablePortCommand::INSTANCE);
    }
}
void OutputMonitor::Cleanup() {
    CommandManager::INSTANCE.removeCommand(&FPPEnableOutputsCommand::INSTANCE);
    CommandManager::INSTANCE.removeCommand(&FPPDisableOutputsCommand::INSTANCE);
    if (!portPins.empty()) {
        CommandManager::INSTANCE.removeCommand(&FPPCheckConfiguredPixelsCommand::INSTANCE);
        CommandManager::INSTANCE.removeCommand(&FPPEnablePortCommand::INSTANCE);
    }
    for (auto pi : portPins) {
        delete pi;
    }
    portPins.clear();
    pullHighOutputPins.clear();
    pullLowOutputPins.clear();
    fusePins.clear();
}

void OutputMonitor::checkPixelCounts(const std::string& portList, const std::string& action, int sensitivty) {
    std::set<std::string> ports;
    for (auto p : split(portList, ',')) {
        if (p == "--ALL--") {
            for (auto p : portPins) {
                ports.insert(p->name);
            }
        } else {
            ports.insert(p);
        }
    }
    for (auto p : portPins) {
        if (ports.find(p->name) != ports.end()) {
            for (int x = 0; x < 6; x++) {
                int diff = std::abs(p->receivers[x].configuredCount - p->receivers[x].pixelCount);
                std::string newWarn;
                if (diff > sensitivty) {
                    newWarn = p->name;
                    if (p->isSmartReceiver) {
                        p += (char)('A' + x);
                    }
                    newWarn += " configured for " + std::to_string(p->receivers[x].configuredCount) + " pixels but " + std::to_string(p->receivers[x].pixelCount) + " pixels detected.";
                }
                if (action == "Warn" && newWarn != p->receivers[x].warning) {
                    if (!p->receivers[x].warning.empty()) {
                        WarningHolder::RemoveWarning(p->receivers[x].warning);
                    }
                    p->receivers[x].warning = newWarn;
                    if (!p->receivers[x].warning.empty()) {
                        WarningHolder::AddWarning(p->receivers[x].warning);
                    }
                } else if (action == "Log") {
                    LogInfo(VB_CHANNELOUT, "%s\n", newWarn.c_str());
                }
            }
        }
    }
}

void OutputMonitor::EnableOutputs() {
    if (!pullHighOutputPins.empty() || !pullLowOutputPins.empty()) {
        LogDebug(VB_CHANNELOUT, "Enabling outputs\n");
    }
    std::unique_lock<std::mutex> lock(gpioLock);
    PinCapabilities::SetMultiPinValue(pullHighOutputPins, 1);
    PinCapabilities::SetMultiPinValue(pullLowOutputPins, 0);
    for (auto p : portPins) {
        for (auto& r : p->receivers) {
            if (r.enabled) {
                r.isOn = true;
                r.hasTriggered = false;
            }
        }
    }
    for (auto p : portPins) {
        if (p->eFusePin && p->eFusePin->getValue() == p->eFuseOKValue) {
            clearEFuseWarning(p, 0);
        }
    }
    lock.unlock();
    CommandManager::INSTANCE.TriggerPreset("OUTPUTS_ENABLED");
}
void OutputMonitor::DisableOutputs() {
    if (!pullHighOutputPins.empty() || !pullLowOutputPins.empty()) {
        LogDebug(VB_CHANNELOUT, "Disabling outputs\n");
    }
    std::unique_lock<std::mutex> lock(gpioLock);
    for (auto p : portPins) {
        for (auto& r : p->receivers) {
            r.isOn = false;
            if (p->isSmartReceiver) {
                r.current = 0;
            }
        }
    }
    PinCapabilities::SetMultiPinValue(pullHighOutputPins, 0);
    PinCapabilities::SetMultiPinValue(pullLowOutputPins, 1);
    lock.unlock();
    CommandManager::INSTANCE.TriggerPreset("OUTPUTS_DISABLED");
}

void OutputMonitor::SetOutput(const std::string& port, bool on) {
    std::unique_lock<std::mutex> lock(gpioLock);
    int pn = 0;
    for (auto p : portPins) {
        if (p->name == port && p->enablePin) {
            p->receivers[0].hasTriggered = false;
            p->receivers[0].isOn = on;
            int value = p->highToEnable ? on : !on;
            clearEFuseWarning(p, 0);
            p->enablePin->setValue(value);
            return;
        } else if (p->isSmartReceiver) {
            for (int x = 0; x < 6; x++) {
                if (port == std::string(p->name) + std::string(1, 'A' + x)) {
                    bool isOn = p->receivers[x].isOn && !p->receivers[x].hasTriggered;
                    if (p->receivers[x].hasTriggered) {
                        srCallback(pn, x, "ResetOutput");
                    } else if (on != isOn) {
                        srCallback(pn, x, "ToggleOutput");
                    }
                }
            }
        }
        pn++;
    }
}

void OutputMonitor::AddPortConfiguration(int port, const Json::Value& pinConfig, bool enabled) {
    std::string name = "Port " + std::to_string(port + 1);
    if (port < portPins.size() && portPins[port]->name == name) {
        portPins[port]->setConfig(pinConfig);
        return;
    }

    PortPinInfo* pi = new PortPinInfo(name, pinConfig);
    bool hasInfo = false;
    pi->receivers[0].enabled = true;
    if (pinConfig.isMember("enablePin")) {
        std::string ep = pinConfig.get("enablePin", "").asString();
        if (ep != "") {
            pi->enablePin = AddOutputPin(name, ep);
            if (!pi->enablePin) {
                enabled = false;
            }
            pi->highToEnable = (ep[0] != '!');
            if (!enabled) {
                pi->receivers[0].enabled = false;
                if (pi->enablePin) {
                    if (pi->highToEnable) {
                        pullHighOutputPins.pop_back();
                    } else {
                        pullLowOutputPins.pop_back();
                    }
                }
            }
            hasInfo = true;
        }
    }
    if (pinConfig.isMember("group")) {
        int g = pinConfig["group"].asInt();
        pi->group = g;
        if (g >= numGroups) {
            numGroups = g + 1;
        }
    }
    if (pinConfig.isMember("eFusePin")) {
        if (pinConfig.isMember("eFuseInterruptPin")) {
            std::string eFuseInterruptPin = pinConfig.get("eFuseInterruptPin", "").asString();
            bool eFuseInterruptHigh = false;
            if (eFuseInterruptPin[0] == '!') {
                eFuseInterruptPin = true;
                eFuseInterruptPin = eFuseInterruptPin.substr(1);
            }
            std::string postFix = "";
            if (eFuseInterruptPin[0] == '-') {
                postFix = "_pd";
                eFuseInterruptPin = eFuseInterruptPin.substr(1);
            } else if (eFuseInterruptPin[0] == '+') {
                postFix = "_pu";
                eFuseInterruptPin = eFuseInterruptPin.substr(1);
            }
            pi->eFuseInterruptPin = PinCapabilities::getPinByName(eFuseInterruptPin).ptr();
            if (!pi->eFuseInterruptPin) {
                LogWarn(VB_CHANNELOUT, "Could not find pin " + eFuseInterruptPin + " to handle fuse interrupts for output " + name + "\n");
                WarningHolder::AddWarning("Could not find pin " + eFuseInterruptPin + " to handle fuse interrupts for output " + name);
            } else {
                hasInfo = true;
                if (fusePins[eFuseInterruptPin] == nullptr) {
                    pi->eFuseInterruptPin->configPin("gpio" + postFix, false);
                    fusePins[eFuseInterruptPin] = pi->eFuseInterruptPin;
                    GPIOManager::INSTANCE.AddGPIOCallback(pi->eFuseInterruptPin, [this, pi](int v) {
                        // printf("\n\n\nInterrupt Pin!!!   %d   %d\n\n\n", v, pi->eFuseInterruptPin->getValue());
                        std::unique_lock<std::mutex> lock(gpioLock);
                        for (auto a : portPins) {
                            if (a->eFuseInterruptPin == pi->eFuseInterruptPin) {
                                int v = a->eFusePin->getValue();
                                if (v != a->eFuseOKValue) {
                                    if (a->enablePin) {
                                        // make sure the port is turned off
                                        a->enablePin->setValue(a->highToEnable ? 0 : 1);
                                    }
                                    if (a->receivers[0].isOn && !a->receivers[0].hasTriggered) {
                                        // Output SHOULD be on, but the fuse triggered.  That's a warning.
                                        a->receivers[0].hasTriggered = true;
                                        if (checkEFuseRetry(a)) {
                                            addEFuseWarning(a, 0);
                                        }
                                    }
                                }
                            }
                        }
                        return true;
                    });
                }
            }
        }
        std::string eFusePin = pinConfig.get("eFusePin", "").asString();
        bool eFuseHigh = false;
        if (eFusePin[0] == '!') {
            eFuseHigh = true;
            eFusePin = eFusePin.substr(1);
        }
        std::string postFix = "";
        if (eFusePin[0] == '-') {
            postFix = "_pd";
            eFusePin = eFusePin.substr(1);
        } else if (eFusePin[0] == '+') {
            postFix = "_pu";
            eFusePin = eFusePin.substr(1);
        }
        pi->eFusePin = PinCapabilities::getPinByName(eFusePin).ptr();
        pi->eFuseOKValue = eFuseHigh ? 1 : 0;
        if (pi->eFusePin == nullptr) {
            LogWarn(VB_CHANNELOUT, "Could not find pin " + eFusePin + " to handle fuse for output " + name + "\n");
            WarningHolder::AddWarning("Could not find pin " + eFusePin + " to handle fuse for output " + name);
        } else {
            pi->eFusePin->configPin("gpio" + postFix, false);
            if (pi->eFuseInterruptPin == nullptr) {
                GPIOManager::INSTANCE.AddGPIOCallback(pi->eFusePin, [this, pi](int v) {
                    std::unique_lock<std::mutex> lock(gpioLock);
                    // printf("eFuse for %s trigger: %d    %d\n", pi->name.c_str(), v, pi->eFusePin->getValue());
                    v = pi->eFusePin->getValue();
                    if (v != pi->eFuseOKValue) {
                        if (pi->enablePin) {
                            // make sure the port is turned off
                            pi->enablePin->setValue(pi->highToEnable ? 0 : 1);
                        }
                        if (pi->receivers[0].isOn && checkEFuseRetry(pi)) {
                            addEFuseWarning(pi, 0);
                        }
                    }
                    return true;
                });
            }
            hasInfo = true;
        }
    }
    if (pinConfig.isMember("currentSensor")) {
        if (pinConfig["currentSensor"].isMember("path")) {
            hasInfo = true;
            pi->currentMonitor = new FileCurrentMonitor(pinConfig["currentSensor"]);
        } else if (pinConfig["currentSensor"].isMember("sensor")) {
            hasInfo = true;
            pi->currentMonitor = new SensorCurrentMonitor(pinConfig["currentSensor"]);
        }
    }
    if (hasInfo) {
        portPins.push_back(pi);
    } else if (pinConfig.isMember("falconV5Listener")) {
        int mr = -1;
        bool hasSR = false;
        for (auto a : portPins) {
            mr = std::max(mr, a->row);
            hasSR |= a->isSmartReceiver;
        }

        int col = portPins.empty() ? 1 : portPins.back()->col;

        pi->isSmartReceiver = true;
        pi->group = -1;
        pi->receivers[0].enabled = false;
        portPins.push_back(pi);

        if (!hasSR || col == 8) {
            col = 1;
            mr += hasSR ? 8 : 1;
        } else {
            col = 5;
        }

        if (pi->row == -1) {
            pi->col = col;
            pi->row = mr;
        }
        int mc = pi->col;
        mr = pi->row;
        for (int x = 1; x < 4; x++) {
            std::string name = "Port " + std::to_string(port + 1 + x);
            pi = new PortPinInfo(name, pinConfig);
            if (pi->row == -1) {
                pi->row = mr;
                pi->col = mc + x;
            }
            pi->isSmartReceiver = true;
            pi->group = -1;
            portPins.push_back(pi);
        }
    } else {
        delete pi;
    }
}

const PinCapabilities* OutputMonitor::AddOutputPin(const std::string& name, const std::string& pinName) {
    std::string pin = pinName;
    bool highToEnable = true;
    if (pin[0] == '!') {
        pin = pin.substr(1);
        highToEnable = false;
    }
    auto& op = highToEnable ? pullHighOutputPins : pullLowOutputPins;
    for (auto& pc : op) {
        if (pc->name == pin) {
            return pc;
        }
    }

    const PinCapabilities* pc = PinCapabilities::getPinByName(pin).ptr();
    if (!pc) {
        LogWarn(VB_CHANNELOUT, "Could not find pin " + pin + " to enable output " + name + "\n");
        WarningHolder::AddWarning("Could not find pin " + pin + " to enable output " + name);
        return nullptr;
    }
    std::unique_lock<std::mutex> lock(gpioLock);
    op.push_back(pc);
    pc->configPin("gpio", true);
    pc->setValue(!highToEnable);
    return pc;
}

void OutputMonitor::AutoEnableOutputs() {
    int i = getSettingInt("AutoEnableOutputs", 1);
    if (i) {
        EnableOutputs();
    }
}
void OutputMonitor::AutoDisableOutputs() {
    int i = getSettingInt("AutoEnableOutputs", 1);
    if (i) {
        DisableOutputs();
    }
}

void OutputMonitor::lockToGroup(int i) {
    curGroup = i;
    Sensors::INSTANCE.lockToGroup(i);
}
bool OutputMonitor::isPortInGroup(int group, int port) {
    return ((port < portPins.size()) && portPins[port] && (group == portPins[port]->group));
}

std::vector<float> OutputMonitor::GetPortCurrentValues() {
    std::vector<float> ret;
    ret.reserve(portPins.size());
    Sensors::INSTANCE.updateSensorSources(true);
    for (auto a : portPins) {
        if (a->currentMonitor && ((curGroup == -1) || (curGroup == a->group))) {
            ret.push_back(a->currentMonitor->getValue());
        } else {
            ret.push_back(a->receivers[0].current);
        }
    }
    return ret;
}
void OutputMonitor::SetPixelCount(int port, int pc, int cc) {
    if (port < portPins.size() && portPins[port] && ((curGroup == -1) || (curGroup == portPins[port]->group))) {
        portPins[port]->receivers[0].pixelCount = pc;
        portPins[port]->receivers[0].configuredCount = cc;
    }
}
int OutputMonitor::GetPixelCount(int port) {
    if (port < portPins.size() && portPins[port]) {
        return portPins[port]->receivers[0].pixelCount;
    }
    return 0;
}

void OutputMonitor::GetCurrentPortStatusJson(Json::Value& result) {
    if (!portPins.empty()) {
        Sensors::INSTANCE.updateSensorSources();
        for (auto a : portPins) {
            a->appendTo(result);
        }
    }
}

void OutputMonitor::addEFuseWarning(PortPinInfo* pi, int rec) {
    if (pi && rec < 6) {
        std::string name = pi->name;
        if (pi->isSmartReceiver) {
            name += std::string(1, 'A' + rec);
        }
        std::string warn = "eFUSE Triggered for " + name;

        if (!pi->receivers[rec].warning.starts_with(warn)) {
            if (sequence->IsSequenceRunning()) {
                std::string seq = sequence->m_seqFilename;
                int sTime = sequence->m_seqMSElapsed / 1000;
                warn += " (" + seq + "/" + std::to_string(sTime / 60) + ":" + std::to_string(sTime % 60) + ")";
            }

            LogWarn(VB_CHANNELOUT, warn + "\n");
            // Output SHOULD be on, but the fuse triggered.  That's a warning.
            WarningHolder::AddWarning(warn);
            pi->receivers[rec].warning = warn;

            std::map<std::string, std::string> keywords;
            keywords["PORT"] = name;
            CommandManager::INSTANCE.TriggerPreset("EFUSE_TRIGGERED", keywords);
        }
    }
}
void OutputMonitor::clearEFuseWarning(PortPinInfo* port, int rec) {
    if (port && rec < 6) {
        if (!port->receivers[rec].warning.empty()) {
            WarningHolder::RemoveWarning(port->receivers[rec].warning);
            port->receivers[rec].warning.clear();
        }
        port->receivers[rec].retryCount = 0;
    }
}
bool OutputMonitor::checkEFuseRetry(PortPinInfo* port) {
    if (port && port->receivers[0].retryCount < eFuseRetryCount) {
        LogDebug(VB_CHANNELOUT, "eFuse triggered for %s.  Attempting reset retry #%d\n", port->name.c_str(), port->receivers[0].retryCount + 1);
        if (std::find(eFuseRetries.begin(), eFuseRetries.end(), port) == eFuseRetries.end()) {
            eFuseRetries.push_back(port);
        }
        if (!retryTimerRunning) {
            Timers::INSTANCE.addPeriodicTimer("eFuse Retry", 100, [this]() {
                processRetries();
            });
            retryTimerRunning = true;
        }
        return false;
    }
    return true;
}
void OutputMonitor::processRetries() {
    long long ft = GetTimeMS();
    std::list<PortPinInfo*> toRemove;
    for (auto p : eFuseRetries) {
        if (p->eFusePin->getValue() == p->eFuseOKValue) {
            p->receivers[0].okCount++;
            // must be good for at least two seconds
            long long resetTime = p->receivers[0].nextRetryTime;
            resetTime += 2000;
            if (ft > resetTime) {
                long long ft2 = ft % 20000;
                long long rt2 = resetTime % 20000;
                p->receivers[0].retryCount = 0;
                toRemove.push_back(p);
            }
        } else if (p->receivers[0].retryCount > eFuseRetryCount) {
            // still not good after all the retries, mark it bad and send the warning
            toRemove.push_back(p);
            addEFuseWarning(p);
        } else if (p->receivers[0].retryCount == 0 || ft > p->receivers[0].nextRetryTime) {
            p->receivers[0].enabled = true;
            p->receivers[0].hasTriggered = false;
            if (p->receivers[0].retryCount == 0) {
                p->receivers[0].firstTriggerTime = ft;
                p->receivers[0].nextRetryTime = ft;
            }
            p->receivers[0].nextRetryTime += eFuseRetryInterval;
            p->receivers[0].retryCount++;
            p->enablePin->setValue(p->highToEnable ? 0 : 1);
            p->enablePin->setValue(p->highToEnable ? 1 : 0);
        }
    }
    for (auto p : toRemove) {
        eFuseRetries.remove(p);
    }
    if (eFuseRetries.empty()) {
        Timers::INSTANCE.stopPeriodicTimer("eFuse Retry");
        retryTimerRunning = false;
    }
}
void OutputMonitor::setSmartReceiverInfo(int port, int index, bool enabled, bool tripped, int current, int pixelCount) {
    // printf("      %d  %c:     Current:  %d     PC:  %d    EN: %d    TR: %d\n", port + 1, (char)('A' + index), current, pixelCount, enabled, tripped);
    if (port < portPins.size()) {
        portPins[port]->receivers[index].enabled = true;
        portPins[port]->receivers[index].isOn = enabled;
        portPins[port]->receivers[index].hasTriggered = tripped;
        portPins[port]->receivers[index].pixelCount = pixelCount;
        portPins[port]->receivers[index].current = current;

        if (tripped) {
            // printf("      %d  %c:     Current:  %d     PC:  %d    EN: %d    TR: %d\n", port + 1, (char)('A' + index), current, pixelCount, enabled, tripped);
            addEFuseWarning(portPins[port], index);
        } else {
            clearEFuseWarning(portPins[port], index);
        }
    }
}

HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> OutputMonitor::render_GET(const httpserver::http_request& req) {
    int plen = req.get_path_pieces().size();
    if (plen > 1 && req.get_path_pieces()[1] == "ports") {
        if (plen > 2 && req.get_path_pieces()[2] == "list") {
            Json::Value result;
            result.append("--ALL--");
            for (auto a : portPins) {
                if (a->isSmartReceiver) {
                    for (int x = 0; x < 6; x++) {
                        if (a->receivers[x].enabled) {
                            result.append(a->name + std::string(1, 'A' + x));
                        }
                    }
                } else {
                    result.append(a->name);
                }
            }
            std::string resultStr = SaveJsonToString(result);
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
        }
        if (portPins.empty()) {
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("[]", 200, "application/json"));
        }
        if (plen > 2 && req.get_path_pieces()[2] == "pixelCount") {
            std::vector<std::string> args;
            args.push_back("50");
            args.push_back("Output Specific");
            args.push_back("--ALL--");
            args.push_back("999");
            CommandManager::INSTANCE.run("Test Start", args);
        }
        if (plen > 2 && req.get_path_pieces()[2] == "stop") {
            std::vector<std::string> args;
            CommandManager::INSTANCE.run("Test Stop", args);
        }
        Sensors::INSTANCE.updateSensorSources();
        Json::Value result = Json::arrayValue;
        GetCurrentPortStatusJson(result);
        std::string resultStr = SaveJsonToString(result);
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
}
