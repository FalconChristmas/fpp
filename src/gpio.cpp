/*
 *   GPIO Input/Output handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "events.h"
#include "log.h"
#include "settings.h"
#include "Plugins.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <pwd.h>
#include <gpiod.h>

#include "gpio.h"
#include "commands/Commands.h"
#include "util/GPIOUtils.h"

#define GPIO_DEBOUNCE_TIME 100000


/*
 * Setup given GPIO for external use
 */
int SetupExtGPIO(int gpio, char *mode)
{
	int retval = 0;

	if (!strcmp(mode, "Output")) {
        LogDebug(VB_GPIO, "GPIO %d set to Output mode\n", gpio);
        PinCapabilities::getPinByGPIO(gpio).configPin("gpio", true);
	} else if (!strcmp(mode, "Input")) {
		LogDebug(VB_GPIO, "GPIO %d set to Input mode\n", gpio);
        PinCapabilities::getPinByGPIO(gpio).configPin("gpio", false);
	} else if (!strcmp(mode, "SoftPWM")) {
		LogDebug(VB_GPIO, "GPIO %d set to SoftPWM mode\n", gpio);
        const PinCapabilities &pin = PinCapabilities::getPinByGPIO(gpio);
        if (pin.supportPWM()) {
            pin.setupPWM(10000);
        } else {
            LogDebug(VB_GPIO, "GPIO %d does not support PWM\n", gpio);
            retval = 1;
        }
	} else {
		LogDebug(VB_GPIO, "GPIO %d invalid mode %s\n", gpio, mode);
		retval = 1;
	}

	return retval;
}

/*
 * Set the given GPIO as requested
 */
int ExtGPIO(int gpio, char *mode, int value)
{
	int retval = 0;
    const PinCapabilities &pin = PinCapabilities::getPinByGPIO(gpio);
	if (!strcmp(mode, "Output")) {
        pin.setValue(value);
	} else if (!strcmp(mode, "Input")) {
		retval = pin.getValue();
	} else if (!strcmp(mode, "SoftPWM")) {
        pin.setPWMValue(value * 100);
	} else {
		LogDebug(VB_GPIO, "GPIO %d invalid mode %s\n", gpio, mode);
		retval = -1;
	}

	return retval;
}


class GPIOCommand : public Command {
public:
    GPIOCommand(std::vector<std::string> &pins) : Command("GPIO") {
        args.push_back(CommandArg("pin", "string", "Pin").setContentList(pins));
        args.push_back(CommandArg("on", "bool", "On"));
    }
    virtual ~GPIOCommand() {
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() != 2) {
            return std::make_unique<Command::ErrorResult>("Invalid number of arguments. GPIO needs two arguments.");
        }
        std::string n = args[0];
        std::string v = args[1];
        const PinCapabilities &p = PinCapabilities::getPinByName(n);
        if (p.ptr()) {
            p.configPin();
            p.setValue(v == "true" || v == "1");
            std::make_unique<Command::Result>("OK");
        }
        return std::make_unique<Command::ErrorResult>("No Pin Named " + n);
    }
};

GPIOManager GPIOManager::INSTANCE;

GPIOManager::GPIOManager() : checkDebounces(false) {
    for (auto &a : gpiodChips) {
        a = nullptr;
    }
}
GPIOManager::~GPIOManager() {
}

void GPIOManager::Initialize(std::map<int, std::function<bool(int)>> &callbacks) {
    SetupGPIOInput(callbacks);
    std::vector<std::string> pins = PinCapabilities::getPinNames();
    if (!pins.empty()) {
        CommandManager::INSTANCE.addCommand(new GPIOCommand(pins));
    }
}
void GPIOManager::CheckGPIOInputs(void) {
    if (pollStates.empty() && !checkDebounces) {
        return;
    }
    long long tm = GetTime();
    for (auto &a : pollStates) {
        int val = a.pin->getValue();
        if (val != a.lastValue) {
            long long lastAllowedTime = tm - GPIO_DEBOUNCE_TIME; // usec's ago
            if ((a.lastTriggerTime < lastAllowedTime)) {
                a.doAction(val);
            }
        }
    }
    if (checkDebounces) {
        checkDebounces = false;
        for (auto &a : eventStates) {
            if (a.futureValue != a.lastValue) {
                long long lastAllowedTime = tm - GPIO_DEBOUNCE_TIME; // usec's ago
                if ((a.lastTriggerTime < lastAllowedTime)) {
                    int val = a.pin->getValue();
                    if (val != a.lastValue) {
                        a.doAction(val);
                    }
                } else {
                    //will need to check again
                    checkDebounces = true;
                }
            }
        }
    }
}
void GPIOManager::Cleanup() {
    for (auto &a : eventStates) {
        if (a.gpiodLine) {
            gpiod_line_release(a.gpiodLine);
        }
    }
    for (auto &a : pollStates) {
        if (a.gpiodLine) {
            gpiod_line_release(a.gpiodLine);
        }
    }
    for (auto a : gpiodChips) {
        if (a) {
            gpiod_chip_close(a);
        }
    }
}

const std::shared_ptr<httpserver::http_response> GPIOManager::render_GET(const httpserver::http_request &req) {
    int plen = req.get_path_pieces().size();
    std::string p1 = req.get_path_pieces()[0];
    if (p1 == "gpio") {
        if (plen <= 1) {
            std::vector<std::string> pins = PinCapabilities::getPinNames();
            if (pins.empty()) {
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("[]", 200, "application/json"));
            }
            Json::Value result;
            for (auto & a : pins) {
                result.append(PinCapabilities::getPinByName(a).toJSON());
            }
            std::string resultStr = SaveJsonToString(result);
            
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
        }
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
}

void GPIOManager::ConvertOldSettings() {
    FILE *file = fopen("/home/fpp/media/settings", "r");
    std::list<char *> lines;
    std::map<std::string, std::string> gpioSettings;
    
    if (file != NULL) {
        char * line = (char*)calloc(256, 1);
        size_t len = 256;
        ssize_t read;
        int sIndex = 0;

        while ((read = getline(&line, &len, file)) != -1) {
            if (( ! line ) || ( ! read ) || ( read == 1 ))
                continue;
            
            char *dup = strdup(line);
            char *token = strtok(dup, "=");
            if ( !token ) {
                lines.push_back(line);
                line = (char*)calloc(256, 1);
                free(dup);
                continue;
            }
            
            char *key = trimwhitespace(token);
            if (!strlen(key)) {
                free(key);
                free(dup);
                lines.push_back(line);
                line = (char*)calloc(256, 1);
                continue;
            }
            if (strncmp(key, "GPIO", 4) == 0) {
                token = strtok(NULL, "=");
                if (!token) {
                    free(key);
                    free(dup);
                    lines.push_back(line);
                    line = (char*)calloc(256, 1);
                    continue;
                }
                char *value = trimwhitespace(token);
                gpioSettings[key] = value;
                
                free(key);
                free(value);
            } else {
                lines.push_back(line);
                line = (char*)calloc(256, 1);
            }
            free(dup);
        }
    }
    fclose(file);
    
    Json::Value val;
    bool found = false;
    for (int i = 0; i < 255; i++) {
        char settingName[32];
        sprintf(settingName, "GPIOInput%03dEnabled", i);
        bool hasSettings = false;
        bool enabled = false;
        Json::Value gp;
        gp["gpio"] = i;
        
        if (gpioSettings[settingName] == "1") {
            hasSettings = true;
            enabled = true;
        }
        gp["enabled"] = true;
        sprintf(settingName, "GPIOInput%03dPullUpDown", i);
        std::string s = gpioSettings[settingName];
        std::string mode = "gpio";
        if (s != "") {
            hasSettings = true;
        }
        if (s == "1") {
            mode = "gpio_pu";
        } else if (s == "2") {
            mode = "gpio_pd";
        }
        gp["mode"] = mode;


        sprintf(settingName, "GPIOInput%03dEventFalling", i);
        std::string fallingAction = gpioSettings[settingName];
        sprintf(settingName, "GPIOInput%03dEventRising", i);
        std::string risingAction = gpioSettings[settingName];
        if (fallingAction != "") {
            hasSettings = true;
            gp["falling"]["command"] = "Trigger Event";
            gp["falling"]["args"][0] = std::to_string(std::stoi(fallingAction.substr(0, 2)));
            gp["falling"]["args"][1] = std::to_string(std::stoi(fallingAction.substr(3, 2)));
        }
        if (risingAction != "") {
            hasSettings = true;
            gp["rising"]["command"] = "Trigger Event";
            gp["rising"]["args"][0] = std::to_string(std::stoi(risingAction.substr(0, 2)));
            gp["rising"]["args"][1] = std::to_string(std::stoi(risingAction.substr(3, 2)));
        }
        if (hasSettings) {
            const PinCapabilities *pin = PinCapabilities::getPinByGPIO(i).ptr();
            if (pin) {
                gp["pin"] = pin->name;
            }
            val.append(gp);
            found = true;
        }
    }
    
    if (found) {
        SaveJsonToFile(val, "/home/fpp/media/config/gpio.json");
        
        file = fopen("/home/fpp/media/settings", "w");
        for (auto l : lines) {
            fwrite(l, strlen(l), 1, file);
            free(l);
        }
        fclose(file);
    }
}


void GPIOManager::SetupGPIOInput(std::map<int, std::function<bool(int)>> &callbacks)
{
    LogDebug(VB_GPIO, "SetupGPIOInput()\n");

    char settingName[32];
    int i = 0;
    int enabledCount = 0;
    
    std::string file = "/home/fpp/media/config/gpio.json";
    if (FileExists(file)) {
        Json::Value root;
        if (LoadJsonFromFile(file, root)) {
            for (int x = 0; x < root.size(); x++) {
                Json::Value v = root[x];
                if (!v["enabled"].asBool()) {
                    continue;
                }
                int gpio = v.isMember("gpio") ? v["gpio"].asInt() : -1;
                std::string pin = v.isMember("pin") ? v["pin"].asString() : "";
                std::string mode = v.isMember("mode") ? v["mode"].asString() : "gpio";
                GPIOState state;
                state.pin = PinCapabilities::getPinByName(pin).ptr();
                if (!state.pin && gpio != -1) {
                    state.pin = PinCapabilities::getPinByGPIO(gpio).ptr();
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

                        // Set the time immediately to utilize the debounce code
                        // from triggering our GPIOs on startup.
                        state.lastTriggerTime = GetTime();
                        state.lastValue = state.futureValue = state.pin->getValue();
                        
                        if ((state.pin->supportsGpiod()) &&
                            (!gpiodChips[state.pin->gpioIdx])) {
                            gpiodChips[state.pin->gpioIdx] = gpiod_chip_open_by_number(state.pin->gpioIdx);
                        }
                        
                        if ((state.pin->supportsGpiod()) &&
                            (state.pin->gpio < gpiod_chip_num_lines(gpiodChips[state.pin->gpioIdx]))) {
                            state.gpiodLine = gpiod_chip_get_line(gpiodChips[state.pin->gpioIdx], state.pin->gpio);
                            
                            struct gpiod_line_request_config lineConfig;
                            lineConfig.consumer = "FPPD";
                            lineConfig.request_type = GPIOD_LINE_REQUEST_DIRECTION_INPUT;
                            lineConfig.flags = 0;
                            if (gpiod_line_request(state.gpiodLine, &lineConfig, 0) == -1) {
                                LogDebug(VB_GPIO, "Could not config line as input\n");
                            }
                            gpiod_line_release(state.gpiodLine);
                            
                            if (state.risingAction != "" && state.fallingAction != "") {
                                lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_BOTH_EDGES;
                            } else if (state.risingAction != "") {
                                lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_RISING_EDGE;
                            } else {
                                lineConfig.request_type = GPIOD_LINE_REQUEST_EVENT_FALLING_EDGE;
                            }
                            lineConfig.flags = 0;
                            if (gpiod_line_request(state.gpiodLine, &lineConfig, 0) == -1) {
                                LogDebug(VB_GPIO, "Could not config line edge for %s, will poll\n", state.pin->name.c_str());
                            } else {
                                state.file = gpiod_line_event_get_fd(state.gpiodLine);
                            }
                        } else {
                            state.gpiodLine = nullptr;
                            state.file = -1;
                        }

                        if (state.file > 0) {
                            eventStates.push_back(state);
                        } else {
                            if (state.gpiodLine) {
                                gpiod_line_release(state.gpiodLine);
                            }
                            state.gpiodLine = nullptr;
                            pollStates.push_back(state);
                        }
                        enabledCount++;
                    }
                }
            }
        }
    }
    LogDebug(VB_GPIO, "%d GPIO Input(s) enabled\n", enabledCount);
    
    
    for (auto &a : eventStates) {
        std::function<bool(int)> f = [&a, this](int i) {
            struct gpiod_line_event event;
            int rc = gpiod_line_event_read_fd(i, &event);

            int v = event.event_type == GPIOD_LINE_EVENT_RISING_EDGE;
            if (v != a.lastValue) {
                long long lastAllowedTime = GetTime() - GPIO_DEBOUNCE_TIME; // usec's ago
                if (a.lastTriggerTime < lastAllowedTime) {
                    a.doAction(v);
                } else {
                    //we are within the debounce time, we'll record this as a last value
                    //and if we end up witha different value after the debounce time,
                    //we'll send the command then
                    a.futureValue = v;
                    checkDebounces = true;
                }
            }
            return false;
        };
        callbacks[a.file] = f;
    }
}


void GPIOManager::GPIOState::doAction(int v) {
    LogDebug(VB_GPIO, "GPIO %s triggered.  Value:  %d\n", pin->name.c_str(), v);
    std::string command = (v == 0) ? fallingAction["command"].asString() : risingAction["command"].asString();
    if (command != "") {
        CommandManager::INSTANCE.run((v == 0) ? fallingAction : risingAction);
    }
    lastTriggerTime = GetTime();
    lastValue = v;
    futureValue = v;
}
