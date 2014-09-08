/*
 *   GPIO Input/Output handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifdef USEWIRINGPI
#   include "wiringPi.h"
#else
#   define pinMode(a, b)
#   define digitalRead(a)        1
#   define pullUpDnControl(a,b)
#   define LOW                   0
#   define PUD_UP                2
#endif

#define MAX_GPIO_INPUTS  255
#define GPIO_DEBOUNCE_TIME 200000

int inputConfigured[MAX_GPIO_INPUTS];
int inputLastState[MAX_GPIO_INPUTS];
int inputNormallyClosed[MAX_GPIO_INPUTS];
long long inputLastTriggerTime[MAX_GPIO_INPUTS];

/*
 * Setup pins for configured GPIO Inputs
 */
int SetupGPIOInput(void)
{
	LogDebug(VB_GPIO, "SetupGPIOInput()\n");

	char settingName[24];
	int i = 0;
	int enabled = 0;
	int enabledCount = 0;

	bzero(inputConfigured, sizeof(inputConfigured));
	bzero(inputLastState, sizeof(inputLastState));
	bzero(inputNormallyClosed, sizeof(inputNormallyClosed));
	bzero(inputLastTriggerTime, sizeof(inputLastTriggerTime));

	for (i = 0; i < MAX_GPIO_INPUTS; i++)
	{
		sprintf(settingName, "GPIOInput%03dEnabled", i);

		if (getSettingInt(settingName))
		{
			LogDebug(VB_GPIO, "Enabling GPIO %d for Input\n", i);
			if (!enabled)
			{
				enabled = 1;
			}

			pinMode(i, INPUT);

			if ((i >= 200) && (i <= 207))
				pullUpDnControl(i, PUD_UP);

			inputConfigured[i] = 1;

			sprintf(settingName, "GPIOInput%03dNC", i);
			if (getSettingInt(settingName))
				inputNormallyClosed[i] = 1;

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

	for (i = 0; i < MAX_GPIO_INPUTS; i++)
	{
		if (inputConfigured[i])
		{
			int val = digitalRead(i);
			if (val != inputLastState[i])
			{
				nc = inputNormallyClosed[i];

				if ((inputLastTriggerTime[i] < lastAllowedTime) &&
					((!nc && (val == LOW)) ||
					 (nc && (val != LOW))))
				{
					LogDebug(VB_GPIO, "GPIO%d triggered\n", i);
					sprintf(settingName, "GPIOInput%03dEvent", i);
					TriggerEventByID(getSetting(settingName));

					inputLastTriggerTime[i] = GetTime();
				}

				inputLastState[i] = val;
			}
		}
	}
}

