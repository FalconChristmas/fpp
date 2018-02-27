/*
 *   Events handler for Falcon Player (FPP)
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

#include <errno.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "common.h"
#include "controlrecv.h"
#include "controlsend.h"
#include "effects.h"
#include "events.h"
#include "log.h"
#include "settings.h"
#include "Plugins.h"

extern PluginCallbackManager pluginCallbackManager;

/*
 * Free a FPPevent structure pointer
 */
void FreeEvent(FPPevent *e)
{
	free(e->name);

	if (e->effect)
		free(e->effect);

	if (e->script)
		free(e->script);

	free(e);
}

/*
 * Load an event file into a FPPevent
 */
FPPevent* LoadEvent(const char *id)
{
	FPPevent *event = NULL;
	FILE     *file;
	char      filename[1024];


	if (snprintf(filename, 1024, "%s/%s.fevt", getEventDirectory(), id) >= 1024)
	{
		LogErr(VB_EVENT, "Unable to open Event file: %s, filename too long\n",
			filename);
		return NULL;
	}

	if ((!FileExists(filename)) &&
		(getFPPmode() == REMOTE_MODE))
		return NULL;

	file = fopen(filename, "r");
	if (!file)
	{
		LogErr(VB_EVENT, "Unable to open Event file %s\n", filename);
		return NULL;
	}

	event = (FPPevent*)malloc(sizeof(FPPevent));

	if (!event)
	{
		LogErr(VB_EVENT, "Unable to allocate memory for new Event %s\n", filename);
		return NULL;
	}

	bzero(event, sizeof(FPPevent));

	char     *line = NULL;
	size_t    len = 0;
	ssize_t   read;
	while ((read = getline(&line, &len, file)) != -1)
	{
		if (( ! line ) || ( ! read ) || ( read == 1 ))
			continue;

		char *token = strtok(line, "=");
		if ( ! token )
			continue;

		token = trimwhitespace(token);
		if (!strlen(token))
		{
			free(token);
			continue;
		}

		char *key = token;
		token = trimwhitespace(strtok(NULL, "="));

		if (token && strlen(token))
		{
			if (!strcmp(key, "majorID"))
			{
				int id = atoi(token);
				if (id < 1)
				{
					FreeEvent(event);
					free(token);
					free(key);
					return NULL;
				}
				event->majorID = id;
			}
			else if (!strcmp(key, "minorID"))
			{
				int id = atoi(token);
				if (id < 1)
				{
					FreeEvent(event);
					free(token);
					free(key);
					return NULL;
				}
				event->minorID = id;
			}
			else if (!strcmp(key, "name"))
			{
				if (strlen(token))
				{
					if (token[0] == '\'')
					{
						event->name = strdup(token + 1);
						if (event->name[strlen(event->name) - 1] == '\'')
							event->name[strlen(event->name) - 1] = '\0';
					}
					else
						event->name = strdup(token);
				}
			}
			else if (!strcmp(key, "effect"))
			{
				if (strlen(token) && strcmp(token, "''"))
				{
					char *c = strstr(token, ".eseq");
					if (c)
					{
						if ((c == (token + strlen(token) - 5)) ||
						    (c == (token + strlen(token) - 6)))
							*c = '\0';

						if (token[0] == '\'')
							event->effect = strdup(token + 1);
						else
							event->effect = strdup(token);
					}
				}
			}
			else if (!strcmp(key, "startChannel"))
			{
				int ch = atoi(token);
				if (ch < 1)
				{
					FreeEvent(event);
					free(token);
					free(key);
					return NULL;
				}
				event->startChannel = ch;
			}
			else if (!strcmp(key, "script"))
			{
				if (strlen(token) && strcmp(token, "''"))
				{
					if (token[0] == '\'')
					{
						event->script = strdup(token + 1);
						if (event->script[strlen(event->script) - 1] == '\'')
							event->script[strlen(event->script) - 1] = '\0';
					}
					else
						event->script = strdup(token);
				}
			}
		}

		if (token)
			free(token);
		free(key);
	}

	if (!event->effect && !event->script)
	{
		FreeEvent(event);
		return NULL;
	}

	LogDebug(VB_EVENT, "Event Loaded:\n");
	if (event->name)
		LogDebug(VB_EVENT, "Event Name  : %s\n", event->name);
	else
		LogDebug(VB_EVENT, "Event Name  : ERROR, no name defined in event file\n");

	LogDebug(VB_EVENT, "Event ID    : %d/%d\n", event->majorID, event->minorID);

	if (event->script)
		LogDebug(VB_EVENT, "Event Script: %s\n", event->script);

	if (event->effect)
	{
		LogDebug(VB_EVENT, "Event Effect: %s\n", event->effect);
		LogDebug(VB_EVENT, "Event St.Ch.: %d\n", event->startChannel);
	}

	return event;
}

/*
 * Fork and run an event script
 */
int RunEventScript(FPPevent *e)
{
	pid_t pid = 0;
	char  userScript[1024];
	char  eventScript[1024];

	// Setup the script from our user
	strcpy(userScript, getScriptDirectory());
	strcat(userScript, "/");
	strncat(userScript, e->script, 1024 - strlen(userScript));
	userScript[1023] = '\0';

	// Setup the wrapper
	memcpy(eventScript, getFPPDirectory(), sizeof(eventScript));
	strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

	pid = fork();
	if (pid == 0) // Event Script process
	{
#ifndef NOROOT
		struct sched_param param;
		param.sched_priority = 0;
		if (sched_setscheduler(0, SCHED_OTHER, &param) != 0)
		{
			perror("sched_setscheduler");
			exit(EXIT_FAILURE);
		}
#endif

		CloseOpenFiles();

		char *args[128];
		char *token = strtok(userScript, " ");
		int   i = 1;

		args[0] = strdup(userScript);
		while (token && i < 126)
		{
			args[i] = strdup(token);
			i++;

			token = strtok(NULL, " ");
		}
		args[i] = NULL;

		if (chdir(getScriptDirectory()))
		{
			LogErr(VB_EVENT, "Unable to change directory to %s: %s\n",
				getScriptDirectory(), strerror(errno));
			exit(EXIT_FAILURE);
		}

		char majorID[3];
		char minorID[3];

		sprintf(majorID, "%d", e->majorID);
		sprintf(minorID, "%d", e->minorID);

		setenv("FPP_EVENT_MAJOR_ID", majorID, 0);
		setenv("FPP_EVENT_MINOR_ID", minorID, 0);
		setenv("FPP_EVENT_NAME", e->name, 0);
		setenv("FPP_EVENT_SCRIPT", e->script, 0);

		execvp(eventScript, args);

		LogErr(VB_EVENT, "RunEventScript(), ERROR, we shouldn't be here, "
			"this means that execvp() failed trying to run '%s %s': %s\n",
			eventScript, args[0], strerror(errno));
		exit(EXIT_FAILURE);
	}

	return 1;
}

/*
 * Trigger an event by major/minor number
 */
int TriggerEvent(const char major, const char minor)
{
	LogDebug(VB_EVENT, "TriggerEvent(%d, %d)\n", (unsigned char)major, (unsigned char)minor);

	if ((major > 25) || (major < 1) || (minor > 25) || (minor < 1))
		return 0;

	char id[6];

	sprintf(id, "%02d_%02d", major, minor);

	pluginCallbackManager.eventCallback(id, "sequence");
	return TriggerEventByID(id);
}

/*
 * Trigger an event
 */
int TriggerEventByID(const char *id)
{
	LogDebug(VB_EVENT, "TriggerEventByID(%s)\n", id);

	if (getFPPmode() == MASTER_MODE)
		SendEventPacket(id);

	FPPevent *event = LoadEvent(id);

	if (!event)
	{
		if (getFPPmode() != REMOTE_MODE)
			LogWarn(VB_EVENT, "Unable to load event %s\n", id);

		return 0;
	}

	if (event->effect)
		StartEffect(event->effect, event->startChannel);

	if (event->script)
		RunEventScript(event);

	FreeEvent(event);

	return 1;
}

