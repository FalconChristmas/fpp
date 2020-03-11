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

#include "common.h"
#include "log.h"
#include "scripts.h"
#include "settings.h"

/*
 * Fork and run a script with accompanying args
 */
pid_t RunScript(std::string script, std::string scriptArgs)
{
	std::vector<std::pair<std::string,std::string>> envVars;

	return RunScript(script, scriptArgs, envVars);
}

pid_t RunScript(std::string script, std::string scriptArgs, std::vector<std::pair<std::string, std::string>> envVars)
{
	pid_t pid = 0;
	char  userScript[1024];
	char  eventScript[1024];

    LogDebug(VB_EVENT, "Script %s:  Args: %s\n", script.c_str(), scriptArgs.c_str());
    
	// Setup the script from our user
	strcpy(userScript, getScriptDirectory());
	strcat(userScript, "/");
	strncat(userScript, script.c_str(), 1024 - strlen(userScript));
	userScript[1023] = '\0';

	// Setup the wrapper
	memcpy(eventScript, getFPPDirectory(), sizeof(eventScript));
	strncat(eventScript, "/scripts/eventScript", sizeof(eventScript)-strlen(eventScript)-1);

    pid = fork();

	if (pid == 0) // Event Script process
	{
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
				if (startsWith(parts[p], "\""))
				{
					quote = "\"";

					tmpPart = parts[p].substr(1);
				}
				else if (startsWith(parts[p], "'"))
				{
					quote = "'";
					tmpPart = parts[p].substr(1);
				}
				else
				{
					args[i] = strdup(parts[p].c_str());
					i++;
				}

				if ((tmpPart != "") && (quote != "") && (endsWith(tmpPart, quote)))
				{
					args[i] = strdup(tmpPart.c_str());

					// Chop off the ending quote
					args[i][strlen(args[i])-1] = 0;

					i++;

					quote = "";
					tmpPart = "";
				}
			}
			else
			{
				tmpPart += " ";
				tmpPart += parts[p];

				if (endsWith(parts[p], quote))
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

		setenv("FPP_SCRIPT", script.c_str(), 0);
		setenv("FPP_SCRIPTARGS", scriptArgs.c_str(), 0);

		for (int ev = 0; ev < envVars.size(); ev++)
		{
			setenv(envVars[ev].first.c_str(), envVars[ev].second.c_str(), 0);
		}

        if (chdir(getScriptDirectory()))
        {
            LogErr(VB_EVENT, "Unable to change directory to %s: %s\n",
                getScriptDirectory(), strerror(errno));
            exit(EXIT_FAILURE);
        }
        for (int x = 0; x < i; x++) {
            LogExcess(VB_EVENT, "Script Arg %d:  %s\n", x, args[x]);
        }
        execvp(eventScript, args);

        LogErr(VB_EVENT, "RunScript(), ERROR, we shouldn't be here, "
            "this means that execvp() failed trying to run '%s %s': %s\n",
            eventScript, args[0], strerror(errno));
        exit(EXIT_FAILURE);
    }
    return pid;
}

