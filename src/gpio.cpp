/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptionsE of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include <array>
#include <httpserver.hpp>
#include <list>
#include <map>
#include <string.h>
#include <string>
#include <vector>

#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"
#include "util/GPIOUtils.h"

#include "gpio.h"


GPIOManager GPIOManager::INSTANCE;

class FPPGPIOCommand : public Command {
public:
    FPPGPIOCommand() :
        Command("GPIO") {
        args.push_back(CommandArg("pin", "string", "Pin").setContentListUrl("api/gpio?list=true"));
        args.push_back(CommandArg("on", "string", "Action").setContentList({ "On", "Off", "Opposite" }));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() != 2) {
            return std::make_unique<Command::ErrorResult>("Invalid number of arguments. GPIO needs two arguments.");
        }
        std::string n = args[0];
        std::string v = args[1];
        const PinCapabilities& p = PinCapabilities::getPinByName(n);
        if (p.ptr()) {
            p.configPin();
            if (v == "On" || v == "on" || v == "true" || v == "True" || v == "1") {
                p.setValue(true);
                GPIOManager::INSTANCE.fppCommandLastValue[n] = true;
            } else if (v == "Off" || v == "off" || v == "false" || v == "False" || v == "0") {
                p.setValue(false);
                GPIOManager::INSTANCE.fppCommandLastValue[n] = false;
            } else if (v == "Opposite" || v == "opposite") {
                GPIOManager::INSTANCE.fppCommandLastValue[n] = !GPIOManager::INSTANCE.fppCommandLastValue[n];
                p.setValue(GPIOManager::INSTANCE.fppCommandLastValue[n]);
            } else {
                return std::make_unique<Command::ErrorResult>("Invalid Action" + v);
            }
            return std::make_unique<Command::Result>("OK");
        }
        return std::make_unique<Command::ErrorResult>("No Pin Named " + n);
    }
};

GPIOManager::GPIOManager() :
    checkDebounces(false) {
}
GPIOManager::~GPIOManager() {
}

void GPIOManager::Initialize(std::map<int, std::function<bool(int)>>& callbacks) {
    SetupGPIOInput(callbacks);
    std::vector<std::string> pins = PinCapabilities::getPinNames();
    if (!pins.empty()) {
        CommandManager::INSTANCE.addCommand(new FPPGPIOCommand());
    }
}
void GPIOManager::CheckGPIOInputs(void) {
    if (pollStates.empty() && !checkDebounces) {
        return;
    }
    long long tm = GetTime();
    for (auto& a : pollStates) {
        int val = a.pin->getValue();
        if (val != a.lastValue) {
            long long lastAllowedTime = tm - a.debounceTime; // usec's ago
            if ((a.lastTriggerTime < lastAllowedTime)) {
                a.doAction(val);
            }
        }
    }
    if (checkDebounces) {
        checkDebounces = false;
        for (auto& a : eventStates) {
            if (a.futureValue != a.lastValue) {
                long long lastAllowedTime = tm - a.debounceTime; // usec's ago
                if ((a.lastTriggerTime < lastAllowedTime)) {
                    int val = a.pin->getValue();
                    if (val != a.lastValue) {
                        a.doAction(val);
                    }
                } else {
                    // will need to check again
                    checkDebounces = true;
                }
            }
        }
    }
}
void GPIOManager::Cleanup() {
    for (auto& a : eventStates) {
        if (a.file != -1) {
            a.pin->releaseGPIOD();
        }
    }
    for (auto& a : pollStates) {
        if (a.file) {
            a.pin->releaseGPIOD();
        }
    }
}

HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> GPIOManager::render_GET(const httpserver::http_request& req) {
    int plen = req.get_path_pieces().size();
    std::string p1 = req.get_path_pieces()[0];
    if (p1 == "gpio") {
        if (plen <= 1) {
            bool simpleList = false;
            if (std::string(req.get_arg("list")) == "true") {
                simpleList = true;
            }
            std::vector<std::string> pins = PinCapabilities::getPinNames();
            if (pins.empty()) {
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("[]", 200, "application/json"));
            }
            Json::Value result;
            for (auto& a : pins) {
                if (simpleList) {
                    result.append(a);
                } else {
                    result.append(PinCapabilities::getPinByName(a).toJSON());
                }
            }
            std::string resultStr = SaveJsonToString(result);
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
        }
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
}

void GPIOManager::SetupGPIOInput(std::map<int, std::function<bool(int)>>& callbacks) {
    LogDebug(VB_GPIO, "SetupGPIOInput()\n");

    char settingName[32];
    int i = 0;
    int enabledCount = 0;

    std::string file = FPP_DIR_CONFIG("/gpio.json");
    if (FileExists(file)) {
        Json::Value root;
        if (LoadJsonFromFile(file, root)) {
            for (int x = 0; x < root.size(); x++) {
                Json::Value v = root[x];
                if (!v["enabled"].asBool()) {
                    continue;
                }
                std::string pin = v.isMember("pin") ? v["pin"].asString() : "";
                std::string mode = v.isMember("mode") ? v["mode"].asString() : "gpio";
                GPIOState state;
                state.pin = PinCapabilities::getPinByName(pin).ptr();
                if (v.isMember("debounceTime")) {
                    state.debounceTime = v["debounceTime"].asInt() * 1000;
                }
                if (state.pin) {
                    if (v.isMember("rising")) {
                        state.risingAction = v["rising"];
                        if (state.risingAction["command"].asString() == "OLED Navigation") {
                            state.risingAction["command"] = "";
                        }
                    }
                    if (v.isMember("falling")) {
                        state.fallingAction = v["falling"];
                        if (state.fallingAction["command"].asString() == "OLED Navigation") {
                            state.fallingAction["command"] = "";
                        }
                    }

                    if (state.risingAction["command"].asString() != "" || state.fallingAction["command"].asString() != "") {
                        state.pin->configPin(mode, false);

                        addState(state);
                        enabledCount++;
                    }
                }
            }
        }
    }
    LogDebug(VB_GPIO, "%d GPIO Input(s) enabled\n", enabledCount);
#ifdef HAS_GPIOD
    for (auto& a : eventStates) {
        std::function<bool(int)> f = [&a, this](int i) {
            struct gpiod_line_event event;
            int rc = gpiod_line_event_read_fd(i, &event);

            int v = event.event_type == GPIOD_LINE_EVENT_RISING_EDGE;
            if (v != a.lastValue) {
                long long lastAllowedTime = GetTime() - a.debounceTime; // usec's ago
                if (a.lastTriggerTime < lastAllowedTime) {
                    a.doAction(v);
                } else {
                    // we are within the debounce time, we'll record this as a last value
                    // and if we end up with a different value after the debounce time,
                    // we'll send the command then
                    a.futureValue = v;
                    checkDebounces = true;
                }
            }
            return false;
        };
        callbacks[a.file] = f;
    }
#endif
}
void GPIOManager::AddGPIOCallback(const PinCapabilities* pin, const std::function<bool(int)>& cb) {
    GPIOState state;
    state.pin = pin;
    state.callback = cb;
    state.hasCallback = true;
    addState(state);
}

void GPIOManager::addState(GPIOState& state) {
    // Set the time immediately to utilize the debounce code
    // from triggering our GPIOs on startup.
    state.lastTriggerTime = GetTime();
    state.lastValue = state.futureValue = state.pin->getValue();
    state.file = -1;

    if (state.pin->supportsGpiod()) {
        state.file = state.pin->requestEventFile(state.risingAction != "", state.fallingAction != "");
    } else {
        state.file = -1;
    }

    if (state.file > 0) {
        eventStates.push_back(state);
    } else {
        pollStates.push_back(state);
    }
}

void GPIOManager::GPIOState::doAction(int v) {
    LogDebug(VB_GPIO, "GPIO %s triggered.  Value:  %d\n", pin->name.c_str(), v);
    if (hasCallback) {
        callback(v);
    } else {
        std::string command = (v == 0) ? fallingAction["command"].asString() : risingAction["command"].asString();
        if (command != "") {
            CommandManager::INSTANCE.run((v == 0) ? fallingAction : risingAction);
        }
    }
    lastTriggerTime = GetTime();
    lastValue = v;
    futureValue = v;
}
