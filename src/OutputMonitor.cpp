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
        sensor->enable(channel);
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

class PortPinInfo {
public:
    PortPinInfo(const std::string& n, const Json::Value& c) :
        name(n), config(c) {}
    ~PortPinInfo() {
        if (currentMonitor) {
            delete currentMonitor;
        }
    }

    std::string name;
    Json::Value config;

    bool isOn = false;
    bool hasTriggered = false;
    bool enabled = true;
    uint32_t group = 0;

    const PinCapabilities* enablePin = nullptr;
    bool highToEnable = true;
    const PinCapabilities* eFusePin = nullptr;
    int eFuseOKValue = 0;

    const PinCapabilities* eFuseInterruptPin = nullptr;
    CurrentMonitorBase* currentMonitor = nullptr;

    int pixelCount = -1;
    int configuredCount = -1;
    std::string warning;

    void appendTo(Json::Value& result) {
        Json::Value v;
        v["name"] = name;
        if (enablePin) {
            int pv = enablePin->getValue();
            v["enabled"] = (pv && highToEnable) || (!pv && !highToEnable);
        }
        if (isOn && eFusePin) {
            v["status"] = eFuseOKValue == eFusePin->getValue();
        } else {
            v["status"] = true;
        }
        if (currentMonitor) {
            float f = currentMonitor->getValue();
            int c = std::round(f);
            v["ma"] = c;
        }
        if (config.isMember("row")) {
            v["row"] = config["row"];
        }
        if (config.isMember("col")) {
            v["col"] = config["col"];
        }
        if (pixelCount >= 0) {
            v["pixelCount"] = pixelCount;
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
    CommandManager::INSTANCE.addCommand(&FPPEnableOutputsCommand::INSTANCE);
    CommandManager::INSTANCE.addCommand(&FPPDisableOutputsCommand::INSTANCE);
    if (!portPins.empty()) {
        CommandManager::INSTANCE.addCommand(&FPPCheckConfiguredPixelsCommand::INSTANCE);
    }
}
void OutputMonitor::Cleanup() {
    CommandManager::INSTANCE.removeCommand(&FPPEnableOutputsCommand::INSTANCE);
    CommandManager::INSTANCE.removeCommand(&FPPDisableOutputsCommand::INSTANCE);
    if (!portPins.empty()) {
        CommandManager::INSTANCE.removeCommand(&FPPCheckConfiguredPixelsCommand::INSTANCE);
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
            int diff = std::abs(p->configuredCount - p->pixelCount);
            std::string newWarn;
            if (diff > sensitivty) {
                newWarn = p->name + " configured for " + std::to_string(p->configuredCount) + " pixels but " + std::to_string(p->pixelCount) + " pixels detected.";
            }
            if (action == "Warn" && newWarn != p->warning) {
                if (!p->warning.empty()) {
                    WarningHolder::RemoveWarning(p->warning);
                }
                p->warning = newWarn;
                if (!p->warning.empty()) {
                    WarningHolder::AddWarning(p->warning);
                }
            } else if (action == "Log") {
                LogInfo(VB_CHANNELOUT, "%s\n", newWarn.c_str());
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
        if (p->enabled) {
            p->isOn = true;
            p->hasTriggered = false;
        }
    }
    for (auto p : portPins) {
        if (p->eFusePin && p->eFusePin->getValue() == p->eFuseOKValue) {
            WarningHolder::RemoveWarning("eFUSE Triggered for " + p->name);
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
        p->isOn = false;
    }
    PinCapabilities::SetMultiPinValue(pullHighOutputPins, 0);
    PinCapabilities::SetMultiPinValue(pullLowOutputPins, 1);
    lock.unlock();
    CommandManager::INSTANCE.TriggerPreset("OUTPUTS_DISABLED");
}
void OutputMonitor::AddPortConfiguration(const std::string& name, const Json::Value& pinConfig, bool enabled) {
    PortPinInfo* pi = new PortPinInfo(name, pinConfig);
    bool hasInfo = false;
    if (pinConfig.isMember("enablePin")) {
        std::string ep = pinConfig.get("enablePin", "").asString();
        if (ep != "") {
            pi->enablePin = AddOutputPin(name, ep);
            pi->highToEnable = (ep[0] != '!');
            if (!enabled) {
                pi->enabled = false;
                if (pi->highToEnable) {
                    pullHighOutputPins.pop_back();
                } else {
                    pullLowOutputPins.pop_back();
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
                                    if (a->isOn && !a->hasTriggered) {
                                        // Output SHOULD be on, but the fuse triggered.  That's a warning.
                                        LogWarn(VB_CHANNELOUT, "eFUSE Triggered for " + a->name + "\n");
                                        WarningHolder::AddWarning("eFUSE Triggered for " + a->name);
                                        a->hasTriggered = true;
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
                    //printf("eFuse for %s trigger: %d    %d\n", pi->name.c_str(), v, pi->eFusePin->getValue());
                    v = pi->eFusePin->getValue();
                    if (v != pi->eFuseOKValue) {
                        if (pi->enablePin) {
                            // make sure the port is turned off
                            pi->enablePin->setValue(pi->highToEnable ? 0 : 1);
                        }
                        if (pi->isOn) {
                            LogWarn(VB_CHANNELOUT, "eFUSE Triggered for " + pi->name + "\n");
                            // Output SHOULD be on, but the fuse triggered.  That's a warning.
                            WarningHolder::AddWarning("eFUSE Triggered for " + pi->name);

                            std::map<std::string, std::string> keywords;
                            keywords["PORT"] = pi->name;
                            CommandManager::INSTANCE.TriggerPreset("EFUSE_TRIGGERED", keywords);
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
    op.push_back(pc);
    std::unique_lock<std::mutex> lock(gpioLock);
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
    return ((port < portPins.size()) && (group == portPins[port]->group));
}

std::vector<float> OutputMonitor::GetPortCurrentValues() {
    std::vector<float> ret;
    ret.reserve(portPins.size());
    Sensors::INSTANCE.updateSensorSources(true);
    for (auto a : portPins) {
        if (a->currentMonitor && ((curGroup == -1) || (curGroup == a->group))) {
            ret.push_back(a->currentMonitor->getValue());
        } else {
            ret.push_back(0);
        }
    }
    return ret;
}
void OutputMonitor::SetPixelCount(int port, int pc, int cc) {
    if (port < portPins.size() && ((curGroup == -1) || (curGroup == portPins[port]->group))) {
        portPins[port]->pixelCount = pc;
        portPins[port]->configuredCount = cc;
    }
}
int OutputMonitor::GetPixelCount(int port) {
    if (port < portPins.size()) {
        return portPins[port]->pixelCount;
    }
    return 0;
}

HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> OutputMonitor::render_GET(const httpserver::http_request& req) {
    int plen = req.get_path_pieces().size();
    if (plen > 1 && req.get_path_pieces()[1] == "ports") {
        if (plen > 2 && req.get_path_pieces()[2] == "list") {
            Json::Value result;
            result.append("--ALL--");
            for (auto a : portPins) {
                result.append(a->name);
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
        Json::Value result;
        for (auto a : portPins) {
            a->appendTo(result);
        }
        std::string resultStr = SaveJsonToString(result);
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
}
