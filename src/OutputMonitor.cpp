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

#include "OutputMonitor.h"
#include "gpio.h"

OutputMonitor OutputMonitor::INSTANCE;

class FusePinInfo {
public:
    const PinCapabilities *pin;
    bool pinIsHigh;

    std::map<std::string, const PinCapabilities *> eFusesHigh;
    std::map<std::string, const PinCapabilities *> eFusesLow;

    void AddPin(const std::string &name, const std::string &pin, bool highIsTriggered) {
        if (highIsTriggered) {
            eFusesHigh[name] = PinCapabilities::getPinByName(pin).ptr();
            if (!eFusesHigh[name]) {
                WarningHolder::AddWarning("Could not find pin " + pin + " to enable eFuse for " + name);
                return;
            }
            eFusesHigh[name]->configPin(highIsTriggered ? "gpio_pd" : "gpio_pu");
        } else {
            eFusesLow[name] = PinCapabilities::getPinByName(pin).ptr();
            if (!eFusesLow[name]) {
                WarningHolder::AddWarning("Could not find pin " + pin + " to enable eFuse for " + name);
                return;
            }
            eFusesLow[name]->configPin(highIsTriggered ? "gpio_pd" : "gpio_pu");
        }
    }
};


class FPPEnableOutputsCommand : public Command {
public:
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
    FPPDisableOutputsCommand() :
        Command("Outputs Off") {
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        OutputMonitor::INSTANCE.DisableOutputs();
        return std::make_unique<Command::Result>("OK");
    }
};

void OutputMonitor::Initialize(std::map<int, std::function<bool(int)>>& callbacks) {
    if (!pullHighOutputPins.empty() || !pullLowOutputPins.empty()) {
        CommandManager::INSTANCE.addCommand(new FPPEnableOutputsCommand());
        CommandManager::INSTANCE.addCommand(new FPPDisableOutputsCommand());
    }
}

void OutputMonitor::EnableOutputs() {
    for (auto &p : pullHighOutputPins) {
        if (p.second) {
            p.second->setValue(1);
        }
    }
    for (auto &p : pullLowOutputPins) {
        if (p.second) {
            p.second->setValue(0);
        }
    }
}
void OutputMonitor::DisableOutputs() {
    for (auto &p : pullHighOutputPins) {
        if (p.second) {
            p.second->setValue(0);
        }
    }
    for (auto &p : pullLowOutputPins) {
        if (p.second) {
            p.second->setValue(1);
        }
    }
}

void OutputMonitor::AddOutputPin(const std::string &name, const std::string &pin, bool highToEnable) {
    auto &op = highToEnable ? pullHighOutputPins : pullLowOutputPins;

    if (op[pin] == nullptr) {
        const PinCapabilities *pc = PinCapabilities::getPinByName(pin).ptr();
        if (!pc) {
            WarningHolder::AddWarning("Could not find pin " + pin + " to enable output " + name);
            return;
        }
        op[pin] = pc;
        pc->configPin("gpio", true);
        pc->setValue(!highToEnable);
    }
    outputPinMaps[pin].push_back(name);
}
void OutputMonitor::AddFusePin(const std::string &name, const std::string &pin, bool highIsTriggered, const std::string &interruptPin, bool interruptPinIsHigh) {
    std::string ip = interruptPin;
    bool ipIsHigh = interruptPinIsHigh;
    if (ip == "") {
        ip = pin;
        ipIsHigh = highIsTriggered;
    }
    if (fusePins[ip] == nullptr) {
        fusePins[ip] = new FusePinInfo();
        fusePins[ip]->pin = PinCapabilities::getPinByName(pin).ptr();
        if (fusePins[ip]->pin) {
            fusePins[ip]->pin->configPin(ipIsHigh ? "gpio_pd" : "gpio_pu", false);
            fusePins[ip]->pinIsHigh = ipIsHigh;
            GPIOManager::INSTANCE.AddGPIOCallback(fusePins[ip]->pin, [this, ip](int v) {
                for (auto &a : fusePins[ip]->eFusesHigh) {
                    if (a.second->getValue() == 1) {
                        WarningHolder::AddWarning("eFUSE Triggered for " + a.first);
                    }
                }
                for (auto &a : fusePins[ip]->eFusesLow) {
                    if (a.second->getValue() == 0) {
                        WarningHolder::AddWarning("eFUSE Triggered for " + a.first);
                    }
                }
                return false;
            });
        }
    }
    if (!fusePins[ip]->pin) {
        WarningHolder::AddWarning("Could not find pin " + ip + " to handle fuse for output " + name);
        return;
    }
    fusePins[ip]->AddPin(name, pin, highIsTriggered);
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
