/*
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
#ifndef __FPP_EVENT_COMMMANDS__
#define __FPP_EVENT_COMMMANDS__

#include "Commands.h"

class TriggerEventCommand : public Command {
public:
    TriggerEventCommand() : Command("Trigger Event") {
        args.push_back(CommandArg("major", "int", "Event Major").setRange(1, 25));
        args.push_back(CommandArg("minor", "int", "Event Minor").setRange(1, 25));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};

class TriggerMultipleEventsCommand : public Command {
public:
    TriggerMultipleEventsCommand() : Command("Trigger Multiple Events") {
        args.push_back(CommandArg("Event1", "string", "Event 1").setContentListUrl("api/events/ids", true));
        args.push_back(CommandArg("Event2", "string", "Event 2").setContentListUrl("api/events/ids", true));
        args.push_back(CommandArg("Event3", "string", "Event 3").setContentListUrl("api/events/ids", true));
        args.push_back(CommandArg("Event4", "string", "Event 4").setContentListUrl("api/events/ids", true));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};

class RunScriptEvent : public Command {
public:
    RunScriptEvent() : Command("Run Script") {
        args.push_back(CommandArg("script", "string", "Script Name").setContentListUrl("api/scripts"));
        args.push_back(CommandArg("args", "string", "Script Arguments"));
        args.push_back(CommandArg("env", "string", "Environment Variables"));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};

class StartEffectCommand : public Command {
public:
    StartEffectCommand() : Command("Effect Start") {
        args.push_back(CommandArg("effect", "string", "Effect Name").setContentListUrl("api/effects"));
        args.push_back(CommandArg("startChannel", "int", "Start Channel"));
        args.push_back(CommandArg("loop", "bool", "Loop Effect"));
        args.push_back(CommandArg("bg", "bool", "Background"));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
class StopEffectCommand : public Command {
public:
    StopEffectCommand() : Command("Effect Stop") {
        args.push_back(CommandArg("effect", "string", "Effect Name").setContentListUrl("api/effects"));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override;
};
#endif
