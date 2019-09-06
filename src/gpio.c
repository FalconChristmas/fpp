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

#include <gpiod.h>

#include "util/GPIOUtils.h"

#define GPIO_DEBOUNCE_TIME 200000

class GPIOState {
public:
    GPIOState() : pin(nullptr), lastValue(0), lastTriggerTime(0) {}
    const PinCapabilities *pin;
    int lastValue;
    long long lastTriggerTime;
    
    struct gpiod_line * gpiodLine = nullptr;
    int file;
    std::string fallingAction;
    std::string risingAction;
};

std::array<gpiod_chip*, 5> GPIOD_CHIPS;
std::vector<GPIOState> POLL_STATES;
std::vector<GPIOState> EVENT_STATES;

void CleanupGPIOInput() {
    for (auto &a : EVENT_STATES) {
        if (a.gpiodLine) {
            gpiod_line_release(a.gpiodLine);
        }
    }
    for (auto a : GPIOD_CHIPS) {
        if (a) {
            gpiod_chip_close(a);
        }
    }
}

/*
 * Setup pins for configured GPIO Inputs
 */
int SetupGPIOInput(std::map<int, std::function<bool(int)>> &callbacks)
{
	LogDebug(VB_GPIO, "SetupGPIOInput()\n");

    for (auto &a : GPIOD_CHIPS) {
        a = nullptr;
    }

	char settingName[32];
	int i = 0;
	int enabled = 0;
	int enabledCount = 0;

	for (i = 0; i < 255; i++) {
		sprintf(settingName, "GPIOInput%03dEnabled", i);

		if (getSettingInt(settingName)) {
			LogDebug(VB_GPIO, "Enabling GPIO %d for Input\n", i);
			if (!enabled) {
				enabled = 1;
			}
            sprintf(settingName, "GPIOInput%03dPullUpDown", i);
            int pu = getSettingInt(settingName, -1);
            if ((pu == -1) && (i >= 200) && (i <= 207)) {
                pu = 1;
            }
            if (pu == -1) {
                pu = 0;
            }
            std::string mode = "gpio";
            if (pu == 1) {
                mode = "gpio_pu";
            } else if (pu == 2) {
                mode = "gpio_pd";
            }
            GPIOState state;
            state.pin = PinCapabilities::getPinByGPIO(i).ptr();
            if (state.pin) {
                state.pin->configPin(mode, false);

                sprintf(settingName, "GPIOInput%03dEventFalling", i);
                state.fallingAction = getSetting(settingName);
                sprintf(settingName, "GPIOInput%03dEventRising", i);
                state.risingAction = getSetting(settingName);

                // Set the time immediately to utilize the debounce code
                // from triggering our GPIOs on startup.
                state.lastTriggerTime = GetTime();
                state.lastValue = state.pin->getValue();
                
                if (!GPIOD_CHIPS[state.pin->gpioIdx]) {
                    GPIOD_CHIPS[state.pin->gpioIdx] = gpiod_chip_open_by_number(state.pin->gpioIdx);
                }
                
                state.gpiodLine = gpiod_chip_get_line(GPIOD_CHIPS[state.pin->gpioIdx], state.pin->gpio);
                
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
                    LogDebug(VB_GPIO, "Could not config line edge\n");
                }
                state.file = gpiod_line_event_get_fd(state.gpiodLine);
                if (state.file > 0) {
                    EVENT_STATES.push_back(state);
                } else {
                    gpiod_line_release(state.gpiodLine);
                    state.gpiodLine = nullptr;
                    POLL_STATES.push_back(state);
                }

            }
			enabledCount++;
		}
	}

	LogDebug(VB_GPIO, "%d GPIO Input(s) enabled\n", enabledCount);
    
    for (auto &a : EVENT_STATES) {
        std::function<bool(int)> f = [&a](int i) {
            struct gpiod_line_event event;
            int rc = gpiod_line_event_read_fd(i, &event);

            long long lastAllowedTime = GetTime() - GPIO_DEBOUNCE_TIME; // usec's ago
            if (a.lastTriggerTime < lastAllowedTime) {
                int v = event.event_type == GPIOD_LINE_EVENT_RISING_EDGE;
                
                LogDebug(VB_GPIO, "GPIO %s triggered.  Value:  %d\n", a.pin->name.c_str(), v);

                std::string event = (v == 0) ? a.fallingAction : a.risingAction;
                if (event != "") {
                    PluginManager::INSTANCE.eventCallback(event.c_str(), "GPIO");
                    TriggerEventByID(event.c_str());
                    a.lastTriggerTime = GetTime();
                    a.lastValue = v;
                }
            }
            return false;
        };
        callbacks[a.file] = f;
    }

	return enabled;
}

/*
 * Check configured GPIO Inputs
 */
void CheckGPIOInputs(void)
{
    for (auto &a : POLL_STATES) {
        int val = a.pin->getValue();
        if (val != a.lastValue) {
            long long lastAllowedTime = GetTime() - GPIO_DEBOUNCE_TIME; // usec's ago
            if ((a.lastTriggerTime < lastAllowedTime)) {
                LogDebug(VB_GPIO, "GPIO %s triggered\n", a.pin->name.c_str());
                std::string event = (val == 0) ? a.fallingAction : a.risingAction;
                PluginManager::INSTANCE.eventCallback(event.c_str(), "GPIO");
                TriggerEventByID(event.c_str());
                
                a.lastTriggerTime = GetTime();
                a.lastValue = val;
            }
        }
    }
}

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

