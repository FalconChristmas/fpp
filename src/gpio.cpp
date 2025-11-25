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

#include "EPollManager.h"
#include "Events.h"
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
        args.push_back(CommandArg("pin", "string", "Pin").setContentListUrl("api/options/GPIOLIST"));
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
    for (auto a : pollStates) {
        int val = a->pin->getValue();
        if (val != a->lastValue) {
            long long lastAllowedTime = tm - a->debounceTime; // usec's ago
            if ((a->lastTriggerTime < lastAllowedTime)) {
                a->doAction(val);
            }
        }
    }
    if (checkDebounces) {
        checkDebounces = false;
        for (auto a : eventStates) {
            if (a->futureValue != a->lastValue) {
                long long lastAllowedTime = tm - a->debounceTime; // usec's ago
                if ((a->lastTriggerTime < lastAllowedTime)) {
                    int val = a->pin->getValue();
                    if (val != a->lastValue) {
                        a->doAction(val);
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
    for (auto a : eventStates) {
        if (a->file != -1) {
            a->pin->releaseGPIOD();
        }
        delete a;
    }
    for (auto a : pollStates) {
        if (a->file) {
            a->pin->releaseGPIOD();
        }
        delete a;
    }
    eventStates.clear();
    pollStates.clear();
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
        } else if (plen == 2) {
            // Handle reading individual GPIO pin value: /gpio/{pin_name}
            std::string pinName = req.get_path_pieces()[1];
            Json::Value result;
            
            // Check if we have a tracked value for this pin FIRST (from previous SET operations)
            // This avoids touching the pin hardware which could interfere with output pins
            auto it = GPIOManager::INSTANCE.fppCommandLastValue.find(pinName);
            if (it != GPIOManager::INSTANCE.fppCommandLastValue.end()) {
                // Return the last set value without touching the pin
                LogDebug(VB_HTTP, "GPIO GET: Returning cached value for %s: %d\n", pinName.c_str(), it->second ? 1 : 0);
                result["pin"] = pinName;
                result["value"] = it->second ? 1 : 0;
                result["Status"] = "OK";
                result["respCode"] = 200;
                
                std::string resultStr = SaveJsonToString(result);
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
            }
            
            // No tracked value - pin hasn't been set via API/commands yet
            // Return error rather than trying to read it which could interfere with output pins
            LogWarn(VB_HTTP, "GPIO GET: No cached value for pin %s - cannot read output pins safely\n", pinName.c_str());
            
            result["pin"] = pinName;
            result["Status"] = "ERROR";
            result["respCode"] = 400;
            result["Message"] = "Pin has no cached value. For output pins, you must SET a value before you can GET it. For input pins, configure them via the GPIO Input configuration page.";
            
            std::string resultStr = SaveJsonToString(result);
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 400, "application/json"));
        }
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
}

HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> GPIOManager::render_POST(const httpserver::http_request& req) {
    int plen = req.get_path_pieces().size();
    std::string p1 = req.get_path_pieces()[0];
    
    if (p1 == "gpio" && plen == 2) {
        // Handle setting individual GPIO pin value: POST /gpio/{pin_name}
        std::string pinName = req.get_path_pieces()[1];
        const PinCapabilities& pin = PinCapabilities::getPinByName(pinName);
        
        if (pin.ptr()) {
            Json::Value data;
            Json::Value result;
            
            // Parse POST data
            if (req.get_content() != "") {
                if (!LoadJsonFromString(std::string(req.get_content()), data)) {
                    Json::Value errorResult;
                    errorResult["pin"] = pinName;
                    errorResult["Status"] = "ERROR";
                    errorResult["respCode"] = 400;
                    errorResult["Message"] = "Error parsing POST content";
                    
                    std::string errorStr = SaveJsonToString(errorResult);
                    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(errorStr, 400, "application/json"));
                }
            }
            
            // Check for required 'value' field
            if (!data.isMember("value")) {
                Json::Value errorResult;
                errorResult["pin"] = pinName;
                errorResult["Status"] = "ERROR";
                errorResult["respCode"] = 400;
                errorResult["Message"] = "'value' field not specified. Use {\"value\": 0} or {\"value\": 1}";
                
                std::string errorStr = SaveJsonToString(errorResult);
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(errorStr, 400, "application/json"));
            }
            
            try {
                // Configure the pin for output
                pin.configPin("gpio", true, "GPIO Output");
                
                // Set the value (convert to boolean)
                bool value = data["value"].asBool() || (data["value"].isInt() && data["value"].asInt() != 0);
                pin.setValue(value);
                
                // Update the last value tracking (for Opposite command functionality and GET requests)
                GPIOManager::INSTANCE.fppCommandLastValue[pinName] = value;
                LogDebug(VB_HTTP, "GPIO POST: Set pin %s to %d, cached value\n", pinName.c_str(), value ? 1 : 0);
                
                result["pin"] = pinName;
                result["value"] = value ? 1 : 0;
                result["Status"] = "OK";
                result["respCode"] = 200;
                result["Message"] = "GPIO pin value set successfully";
                
                std::string resultStr = SaveJsonToString(result);
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
            } catch (const std::exception& e) {
                Json::Value errorResult;
                errorResult["pin"] = pinName;
                errorResult["Status"] = "ERROR";
                errorResult["respCode"] = 500;
                errorResult["Message"] = std::string("Error setting GPIO pin: ") + e.what();
                
                std::string errorStr = SaveJsonToString(errorResult);
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(errorStr, 500, "application/json"));
            }
        } else {
            Json::Value errorResult;
            errorResult["pin"] = pinName;
            errorResult["Status"] = "ERROR";
            errorResult["respCode"] = 404;
            errorResult["Message"] = "GPIO pin not found: " + pinName;
            
            std::string errorStr = SaveJsonToString(errorResult);
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(errorStr, 404, "application/json"));
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
                const PinCapabilities* pc = PinCapabilities::getPinByName(pin).ptr();
                if (pc) {
                    GPIOState* state = new GPIOState();
                    if (v.isMember("debounceTime")) {
                        state->debounceTime = v["debounceTime"].asInt() * 1000;
                    }
                    state->pin = pc;
                    if (v.isMember("rising")) {
                        state->risingAction = v["rising"];
                        if (state->risingAction["command"].asString() == "OLED Navigation") {
                            state->risingAction["command"] = "";
                        }
                    }
                    if (v.isMember("falling")) {
                        state->fallingAction = v["falling"];
                        if (state->fallingAction["command"].asString() == "OLED Navigation") {
                            state->fallingAction["command"] = "";
                        }
                    }

                    if (state->risingAction["command"].asString() != "" || state->fallingAction["command"].asString() != "") {
                        state->pin->configPin(mode, false, "GPIO Input");

                        addState(state);
                        enabledCount++;
                    } else {
                        delete state;
                    }
                }
            }
        }
    }
    LogDebug(VB_GPIO, "%d GPIO Input(s) enabled\n", enabledCount);
}

void GPIOManager::addGPIOCallback(GPIOState* a) {
#ifdef HAS_GPIOD
    std::function<bool(int)> f = [a, this](int i) {
        struct gpiod_line_event event;
        int rc = gpiod_line_event_read_fd(i, &event);

        int v = event.event_type == GPIOD_LINE_EVENT_RISING_EDGE;
        if (v != a->lastValue) {
            long long lastAllowedTime = GetTime() - a->debounceTime; // usec's ago
            if (a->lastTriggerTime < lastAllowedTime) {
                a->doAction(v);
            } else {
                // we are within the debounce time, we'll record this as a last value
                // and if we end up with a different value after the debounce time,
                // we'll send the command then
                a->futureValue = v;
                checkDebounces = true;
            }
        }
        return false;
    };
    EPollManager::INSTANCE.addFileDescriptor(a->file, f);
#endif
}

void GPIOManager::AddGPIOCallback(const PinCapabilities* pin, const std::function<bool(int)>& cb) {
    GPIOState* state = new GPIOState();
    state->pin = pin;
    state->callback = cb;
    state->hasCallback = true;
    addState(state);
}
void GPIOManager::RemoveGPIOCallback(const PinCapabilities* pin) {
    for (auto it = eventStates.begin(); it != eventStates.end(); ++it) {
        GPIOState* a = *it;
        if (a->pin == pin) {
            if (a->file != -1) {
                EPollManager::INSTANCE.removeFileDescriptor(a->file);
                a->pin->releaseGPIOD();
            }
            eventStates.erase(it);
            delete a;
            return;
        }
    }
    for (auto it = pollStates.begin(); it != pollStates.end(); ++it) {
        GPIOState* a = *it;
        if (a->pin == pin) {
            pollStates.erase(it);
            delete a;
            return;
        }
    }
}

void GPIOManager::addState(GPIOState* state) {
    // Set the time immediately to utilize the debounce code
    // from triggering our GPIOs on startup.
    state->lastTriggerTime = GetTime();
    state->lastValue = state->futureValue = state->pin->getValue();
    state->file = -1;

    if (state->pin->supportsGpiod()) {
        state->file = state->pin->requestEventFile(state->risingAction != "", state->fallingAction != "");
    } else {
        state->file = -1;
    }

    if (state->file > 0) {
        eventStates.push_back(state);
        addGPIOCallback(state);
    } else {
        pollStates.push_back(state);
    }
}

void GPIOManager::GPIOState::doAction(int v) {
    LogDebug(VB_GPIO, "GPIO %s triggered.  Value:  %d\n", pin->name.c_str(), v);
    
    // Publish GPIO edge events to MQTT
    if (v == 1) {
        std::string risingTopic = "gpio/" + pin->name + "/rising";
        Events::Publish(risingTopic, 1);
    } else {
        std::string fallingTopic = "gpio/" + pin->name + "/falling";
        Events::Publish(fallingTopic, 0);
    }
    
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
