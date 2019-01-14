/*
 *   Script functions for Falcon Player (FPP)
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boost/algorithm/string/predicate.hpp>

#include "common.h"
#include "log.h"
#include "settings.h"

/*
 * Fork and run a script with accompanying args
 */
void RunScript(std::string script, std::string scriptArgs, int blocking)
{
	pid_t pid = 0;
	char  userScript[1024];
	char  eventScript[1024];

	// Setup the script from our user
	strcpy(userScript, getScriptDirectory());
	strcat(userScript, "/");
	strncat(userScript, script.c_str(), 1024 - strlen(userScript));
	userScript[1023] = '\0';

	// Setup the wrapper
	memcpy(eventScript, getFPPDirectory(), sizeof(eventScript));
	strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

	// FIXME, add blocking support
	blocking = 0;

	if (!blocking)
		pid = fork();

	if (pid == 0) // Event Script process
	{
		if (!blocking)
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

		std::vector<std::string> parts = split(scriptArgs, ' ');
		std::string tmpPart = "";
		std::string quote = "";
		for (int p = 0; p < parts.size(); p++)
		{
			if (tmpPart == "")
			{
				if (boost::starts_with(parts[p], "\""))
				{
					quote = "\"";

					// Skip the beginning quote
					tmpPart = parts[p].substr(1);
				}
				else if (boost::starts_with(parts[p], "'"))
				{
					quote = "'";
					tmpPart = parts[p];
				}
				else
				{
					args[i] = strdup(parts[p].c_str());
					i++;
				}
			}
			else
			{
				tmpPart += " ";
				tmpPart += parts[p];

				if (boost::ends_with(parts[p], quote))
				{
					args[i] = strdup(tmpPart.c_str());

					// Chop off the ending quote
					args[i][strlen(args[i])-1] = 0;

					i++;

					quote = "";
					tmpPart = "";
				}
			}
		}

		args[i] = NULL;

		if (chdir(getScriptDirectory()))
		{
			LogErr(VB_EVENT, "Unable to change directory to %s: %s\n",
				getScriptDirectory(), strerror(errno));
			exit(EXIT_FAILURE);
		}

		setenv("FPP_SCRIPT", script.c_str(), 0);
		setenv("FPP_SCRIPTARGS", scriptArgs.c_str(), 0);

		if (blocking)
		{
			// FIXME, add blocking support
		}
		else
		{
			execvp(eventScript, args);

			LogErr(VB_EVENT, "RunScript(), ERROR, we shouldn't be here, "
				"this means that execvp() failed trying to run '%s %s': %s\n",
				eventScript, args[0], strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
}

