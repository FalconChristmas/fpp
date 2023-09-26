/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2023 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "OLEDCommands.h"
#include "command.h"
#include "../log.h"
#include <curl/curl.h>

OLEDMenuCommand::OLEDMenuCommand() :
    Command("OLED Menu Controls", "Sends actions to the OLED Menu, standard actions are: Up, Down, Enter & Back") {
    args.push_back(CommandArg("buttonaction", "string", "Button Action").setContentListUrl("api/oled/action/options"));
}
std::unique_ptr<Command::Result> OLEDMenuCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    LogInfo(VB_COMMAND, "OLED Command Received for: %s\n", args[0].c_str());

    // Process sub commands for OLED menu input
    std::string CmdPlusArg = "SetOLEDCommand," + args[0];
    char* c = CmdPlusArg.data();
    std::string Cmd_resp = args[0] + " command processed";
    char* r = Cmd_resp.data();
    ProcessCommand(c, r);
    printf("Passing OLED Menu Control FPP Command input to specific command for action required\n");
    ////////////////////////////////////////////////////////////////////

    return std::make_unique<Command::Result>("OLED Menu Control Executed");
}