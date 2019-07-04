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

#include "util/GPIOUtils.h"

#define MAX_GPIO_INPUTS  255
#define GPIO_DEBOUNCE_TIME 200000

class GPIOState {
public:
    GPIOState() : pin(nullptr), lastValue(0), lastTriggerTime(0) {}
    const PinCapabilities *pin;
    int lastValue;
    long long lastTriggerTime;
};

GPIOState inputStates[MAX_GPIO_INPUTS];
extern PluginCallbackManager pluginCallbackManager;

/*
 * Setup pins for configured GPIO Inputs
 */
int SetupGPIOInput(void)
{
	LogDebug(VB_GPIO, "SetupGPIOInput()\n");

	char settingName[32];
	int i = 0;
	int enabled = 0;
	int enabledCount = 0;

	for (i = 0; i < MAX_GPIO_INPUTS; i++) {
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
            
            inputStates[i].pin = PinCapabilities::getPinByGPIO(i).ptr();
            if (inputStates[i].pin) {
                inputStates[i].pin->configPin(mode, false);

                // Set the time immediately to utilize the debounce code
                // from triggering our GPIOs on startup.
                inputStates[i].lastTriggerTime = GetTime();
                
                inputStates[i].lastValue = inputStates[i].pin->getValue();
            }
			enabledCount++;
		}
	}

	LogDebug(VB_GPIO, "%d GPIO Input(s) enabled\n", enabledCount);

	return enabled;
}

/*
 * Check configured GPIO Inputs
 */
// FIXME, how do we handle a second trigger while first is active
void CheckGPIOInputs(void)
{
	char settingName[24];
	int i = 0;
	int nc = 0;
	long long lastAllowedTime = GetTime() - GPIO_DEBOUNCE_TIME; // usec's ago

	for (i = 0; i < MAX_GPIO_INPUTS; i++) {
		if (inputStates[i].pin)	{
			int val = inputStates[i].pin->getValue();
			if (val != inputStates[i].lastValue) {
				if ((inputStates[i].lastTriggerTime < lastAllowedTime)) {
					LogDebug(VB_GPIO, "GPIO%d triggered\n", i);
					sprintf(settingName, "GPIOInput%03dEvent%s", i, (val == 0 ? "Falling" : "Rising"));
					if (strlen(getSetting(settingName))) {
						pluginCallbackManager.eventCallback(getSetting(settingName), "GPIO");
						TriggerEventByID(getSetting(settingName));
					}

					inputStates[i].lastTriggerTime = GetTime();
				}

				inputStates[i].lastValue = val;
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

