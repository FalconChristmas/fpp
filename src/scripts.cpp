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

#include <cstring>
#include <errno.h>
#include <memory>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"

#include "scripts.h"

/*
 * Fork and run a script with accompanying args
 */
pid_t RunScript(std::string script, std::string scriptArgs) {
    std::vector<std::pair<std::string, std::string>> envVars;

    return RunScript(script, scriptArgs, envVars);
}

pid_t RunScript(std::string script, std::string scriptArgs, std::vector<std::pair<std::string, std::string>> envVars) {
    pid_t pid = 0;
    char userScript[1024];
    char eventScript[1024];

    LogDebug(VB_COMMAND, "Script %s:  Args: %s\n", script.c_str(), scriptArgs.c_str());

    // Setup the script from our user
    strcpy(userScript, FPP_DIR_SCRIPT("/").c_str());
    strncat(userScript, script.c_str(), 1024 - strlen(userScript));
    userScript[1023] = '\0';

    // Setup the wrapper
    strcpy(eventScript, FPP_DIR.c_str());
    strcat(eventScript, "/scripts/eventScript");

    pid = fork();

    if (pid == 0) // Event Script process
    {
        CloseOpenFiles();

        char* args[128];
        char* token = strtok(userScript, " ");
        int i = 1;

        args[0] = strdup(userScript);
        while (token && i < 126) {
            args[i] = strdup(token);
            i++;

            token = strtok(NULL, " ");
        }

        std::vector<std::string> parts = split(scriptArgs, ' ');
        std::string tmpPart = "";
        std::string quote = "";
        for (int p = 0; p < parts.size(); p++) {
            if (tmpPart == "") {
                if (startsWith(parts[p], "\"")) {
                    quote = "\"";

                    tmpPart = parts[p].substr(1);
                } else if (startsWith(parts[p], "'")) {
                    quote = "'";
                    tmpPart = parts[p].substr(1);
                } else {
                    args[i] = strdup(parts[p].c_str());
                    i++;
                }

                if ((tmpPart != "") && (quote != "") && (endsWith(tmpPart, quote))) {
                    args[i] = strdup(tmpPart.c_str());

                    // Chop off the ending quote
                    args[i][strlen(args[i]) - 1] = 0;

                    i++;

                    quote = "";
                    tmpPart = "";
                }
            } else {
                tmpPart += " ";
                tmpPart += parts[p];

                if (endsWith(parts[p], quote)) {
                    args[i] = strdup(tmpPart.c_str());

                    // Chop off the ending quote
                    args[i][strlen(args[i]) - 1] = 0;

                    i++;

                    quote = "";
                    tmpPart = "";
                }
            }
        }

        args[i] = NULL;

        setenv("FPP_SCRIPT", script.c_str(), 0);
        setenv("FPP_SCRIPTARGS", scriptArgs.c_str(), 0);

        for (int ev = 0; ev < envVars.size(); ev++) {
            setenv(envVars[ev].first.c_str(), envVars[ev].second.c_str(), 0);
        }

        if (chdir(FPP_DIR_SCRIPT("").c_str())) {
            LogErr(VB_COMMAND, "Unable to change directory to %s: %s\n",
                   FPP_DIR_SCRIPT("").c_str(), strerror(errno));
            exit(EXIT_FAILURE);
        }
        for (int x = 0; x < i; x++) {
            LogExcess(VB_COMMAND, "Script Arg %d:  %s\n", x, args[x]);
        }
        execvp(eventScript, args);

        LogErr(VB_COMMAND, "RunScript(), ERROR, we shouldn't be here, "
                           "this means that execvp() failed trying to run '%s %s': %s\n",
               eventScript, args[0], strerror(errno));
        exit(EXIT_FAILURE);
    }
    return pid;
}
