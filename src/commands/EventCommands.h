#pragma once
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

#include "Commands.h"

class TriggerPresetCommand : public Command {
public:
    TriggerPresetCommand() :
        Command("Trigger Command Preset") {
        args.push_back(CommandArg("name", "datalist", "Preset Name").setContentListUrl("api/commandPresets?names=true"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class TriggerRemotePresetCommand : public Command {
public:
    TriggerRemotePresetCommand() :
        Command("Remote Trigger Command Preset") {
        args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
        args.push_back(CommandArg("name", "string", "Preset Name"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class TriggerPresetSlotCommand : public Command {
public:
    TriggerPresetSlotCommand() :
        Command("Trigger Command Preset Slot") {
        args.push_back(CommandArg("slot", "int", "Preset Slot").setRange(1, 255));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class TriggerRemotePresetSlotCommand : public Command {
public:
    TriggerRemotePresetSlotCommand() :
        Command("Remote Trigger Command Preset Slot") {
        args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
        args.push_back(CommandArg("slot", "int", "Preset Slot").setRange(1, 255));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class TriggerMultiplePresetSlotsCommand : public Command {
public:
    TriggerMultiplePresetSlotsCommand() :
        Command("Trigger Multiple Command Preset Slots") {
        args.push_back(CommandArg("SlotA", "int", "Preset Slot A").setRange(1, 255));
        args.push_back(CommandArg("SlotB", "int", "Preset Slot B").setRange(1, 255));
        args.push_back(CommandArg("SlotC", "int", "Preset Slot C").setRange(1, 255));
        args.push_back(CommandArg("SlotD", "int", "Preset Slot D").setRange(1, 255));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class RunScriptEvent : public Command {
public:
    RunScriptEvent() :
        Command("Run Script") {
        args.push_back(CommandArg("script", "string", "Script Name").setContentListUrl("api/scripts"));
        args.push_back(CommandArg("args", "string", "Script Arguments").setAdjustable());
        args.push_back(CommandArg("env", "string", "Environment Variables"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class RunRemoteScriptEvent : public Command {
public:
    RunRemoteScriptEvent() :
        Command("Remote Run Script") {
        args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
        args.push_back(CommandArg("script", "string", "Script Name"));
        args.push_back(CommandArg("args", "string", "Script Arguments").setAdjustable());
        args.push_back(CommandArg("env", "string", "Environment Variables"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartEffectCommand : public Command {
public:
    StartEffectCommand() :
        Command("Effect Start") {
        args.push_back(CommandArg("effect", "string", "Effect Name").setContentListUrl("api/effects"));
        args.push_back(CommandArg("startChannel", "int", "Start Channel"));
        args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
        args.push_back(CommandArg("bg", "bool", "Background"));
        args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StartFSEQAsEffectCommand : public Command {
public:
    StartFSEQAsEffectCommand() :
        Command("FSEQ Effect Start") {
        args.push_back(CommandArg("effect", "string", "FSEQ Name").setContentListUrl("api/sequence"));
        args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
        args.push_back(CommandArg("bg", "bool", "Background"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StopEffectCommand : public Command {
public:
    StopEffectCommand() :
        Command("Effect Stop", "Stop the specified effect.") {
        args.push_back(CommandArg("effect", "datalist", "Effect Name").setContentListUrl("api/effects/ALL"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StopAllEffectsCommand : public Command {
public:
    StopAllEffectsCommand() :
        Command("Effects Stop", "Stop all running effects.") {
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StopFSEQAsEffectCommand : public Command {
public:
    StopFSEQAsEffectCommand() :
        Command("FSEQ Effect Stop") {
        args.push_back(CommandArg("effect", "string", "FSEQ Name").setContentListUrl("api/sequence"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartRemoteEffectCommand : public Command {
public:
    StartRemoteEffectCommand() :
        Command("Remote Effect Start") {
        args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
        args.push_back(CommandArg("effect", "string", "Effect Name"));
        args.push_back(CommandArg("startChannel", "int", "Start Channel"));
        args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
        args.push_back(CommandArg("bg", "bool", "Background"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StartRemoteFSEQEffectCommand : public Command {
public:
    StartRemoteFSEQEffectCommand() :
        Command("Remote FSEQ Effect Start") {
        args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
        args.push_back(CommandArg("fseq", "string", "FSEQ Name"));
        args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
        args.push_back(CommandArg("bg", "bool", "Background"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
class StopRemoteEffectCommand : public Command {
public:
    StopRemoteEffectCommand() :
        Command("Remote Effect Stop") {
        args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
        args.push_back(CommandArg("effect", "string", "Effect Name"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class StartRemotePlaylistCommand : public Command {
public:
    StartRemotePlaylistCommand() :
        Command("Remote Playlist Start") {
        args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
        args.push_back(CommandArg("playlist", "string", "Playlist Name"));
        args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
        args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};

class AllLightsOffCommand : public Command {
public:
    AllLightsOffCommand() :
        Command("All Lights Off", "Turn all lights off.") {
    }

    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override;
};
