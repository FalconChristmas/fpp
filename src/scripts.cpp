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

#include "Warnings.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "commands/Commands.h"

#include "scripts.h"

extern char** environ;

/*
 * Fork and run a script with accompanying args
 */
pid_t RunScript(std::string script, std::string scriptArgs) {
    std::vector<std::pair<std::string, std::string>> envVars;

    return RunScript(script, scriptArgs, envVars);
}

// Refused a script path outside the scripts directory.  Timed: it is raised
// per attempt, so it ages out once nothing asks for that path any more.
static constexpr int WARNING_ID_SCRIPT_OUTSIDE_DIR = 70;

pid_t RunScript(std::string script, std::string scriptArgs, std::vector<std::pair<std::string, std::string>> envVars) {
    pid_t pid = 0;
    char userScript[1024];
    char eventScript[1024];

    LogDebug(VB_COMMAND, "Script %s:  Args: %s\n", script.c_str(), scriptArgs.c_str());

    // Only scripts in the scripts directory run.  The name is appended to that
    // directory, so a ".." component was the one way out of it -- and names
    // arrive from dynamic playlist sources (a remote URL) and from commands,
    // not just from the UI's script picker.  Everything up to the first space
    // is the path; the rest are arguments.
    std::string scriptPath = script.substr(0, script.find(' '));
    for (auto& part : split(scriptPath, '/')) {
        if (part == "..") {
            LogErr(VB_COMMAND, "RunScript: refusing script outside the scripts directory: %s\n", script.c_str());
            WarningHolder::AddWarningTimeout(60, WARNING_ID_SCRIPT_OUTSIDE_DIR, "Refused to run a script outside the scripts directory: " + scriptPath);
            return -1;
        }
    }

    // Setup the script from our user
    int n = snprintf(userScript, sizeof(userScript), "%s%s", FPP_DIR_SCRIPT("/").c_str(), script.c_str());
    if (n < 0 || n >= (int)sizeof(userScript)) {
        // Truncated: running a prefix of the requested path is never right.
        LogErr(VB_COMMAND, "RunScript: script path too long: %s\n", script.c_str());
        return -1;
    }

    // Setup the wrapper
    n = snprintf(eventScript, sizeof(eventScript), "%s/scripts/eventScript", FPP_DIR.c_str());
    if (n < 0 || n >= (int)sizeof(eventScript)) {
        LogWarn(VB_COMMAND, "RunScript: eventScript path truncated\n");
        eventScript[sizeof(eventScript) - 1] = '\0';
    }

    // Everything the child needs is built here, before fork().  fppd is
    // multithreaded, and after fork() the child may only make async-signal-
    // safe calls: it used to read a setting (a lock), log (locks), strdup and
    // setenv (malloc) and exit() (static destructors) -- if another thread
    // held any of those locks at the fork, the child deadlocked before exec,
    // and a blocking script entry waited on it forever.
    std::vector<std::string> argStrs;
    {
        std::string userScriptStr = userScript;
        argStrs.push_back(userScriptStr.substr(0, userScriptStr.find(' ')));
        for (auto& t : split(userScriptStr, ' ')) {
            if (!t.empty() && argStrs.size() < 126)
                argStrs.push_back(t);
        }

        std::vector<std::string> parts = split(scriptArgs, ' ');
        std::string tmpPart = "";
        std::string quote = "";
        for (int p = 0; p < parts.size(); p++) {
            if (argStrs.size() >= 127) {
                LogWarn(VB_COMMAND, "RunScript: too many args, truncating\n");
                break;
            }
            if (tmpPart == "") {
                if (startsWith(parts[p], "\"")) {
                    quote = std::string("\"");
                    tmpPart = parts[p].substr(1);
                } else if (startsWith(parts[p], "'")) {
                    quote = std::string("'");
                    tmpPart = parts[p].substr(1);
                } else {
                    argStrs.push_back(parts[p]);
                }

                if ((tmpPart != "") && (quote != "") && (endsWith(tmpPart, quote))) {
                    // Chop off the ending quote
                    argStrs.push_back(tmpPart.substr(0, tmpPart.size() - 1));
                    quote = "";
                    tmpPart = "";
                }
            } else {
                tmpPart += " ";
                tmpPart += parts[p];

                if (endsWith(parts[p], quote)) {
                    argStrs.push_back(tmpPart.substr(0, tmpPart.size() - 1));
                    quote = "";
                    tmpPart = "";
                }
            }
        }
    }
    for (int x = 0; x < argStrs.size(); x++) {
        LogExcess(VB_COMMAND, "Script Arg %d:  %s\n", x, argStrs[x].c_str());
    }

    // The child's environment: ours, plus FPP_SCRIPT, FPP_SCRIPTARGS and the
    // caller's variables -- each only if not already set, as setenv(..., 0)
    // did in the child.
    std::vector<std::string> envStrs;
    for (char** e = environ; e && *e; e++) {
        envStrs.push_back(*e);
    }
    auto addEnv = [&envStrs](const std::string& k, const std::string& v) {
        std::string prefix = k + "=";
        for (auto& e : envStrs) {
            if (e.compare(0, prefix.size(), prefix) == 0)
                return;
        }
        envStrs.push_back(prefix + v);
    };
    addEnv("FPP_SCRIPT", script);
    addEnv("FPP_SCRIPTARGS", scriptArgs);
    for (auto& ev : envVars) {
        addEnv(ev.first, ev.second);
    }

    std::vector<char*> argv;
    argv.reserve(argStrs.size() + 1);
    for (auto& a : argStrs)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    std::vector<char*> envp;
    envp.reserve(envStrs.size() + 1);
    for (auto& e : envStrs)
        envp.push_back(const_cast<char*>(e.c_str()));
    envp.push_back(nullptr);

    const std::string scriptDir = FPP_DIR_SCRIPT("");
    const int daemonize = getSettingInt("daemonize");
    static const char chdirFailed[] = "RunScript: unable to change to the scripts directory\n";
    static const char execFailed[] = "RunScript: execve() of the eventScript wrapper failed\n";

    pid = fork();

    if (pid == 0) // Event Script process: async-signal-safe calls only
    {
        CloseOpenFiles(daemonize);
        if (chdir(scriptDir.c_str())) {
            if (!daemonize)
                (void)!write(2, chdirFailed, sizeof(chdirFailed) - 1);
            _exit(EXIT_FAILURE);
        }
        execve(eventScript, argv.data(), envp.data());
        if (!daemonize)
            (void)!write(2, execFailed, sizeof(execFailed) - 1);
        _exit(EXIT_FAILURE);
    }
    if (pid < 0) {
        LogErr(VB_COMMAND, "RunScript: fork() failed: %s\n", FPPstrerror(errno));
    }
    return pid;
}
