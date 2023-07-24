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

#include <functional>

#include "../Sequence.h"
#include "../Timers.h"
#include "../common.h"
#include "../effects.h"
#include "../log.h"
#include "../overlays/PixelOverlay.h"
#include "../overlays/PixelOverlayModel.h"
#include "../scripts.h"

#include "EventCommands.h"

TriggerPresetCommand::TriggerPresetCommand() :
    Command("Trigger Command Preset") {
    args.push_back(CommandArg("name", "datalist", "Preset Name").setContentListUrl("api/commandPresets?names=true"));
}

std::unique_ptr<Command::Result> TriggerPresetCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    if (CommandManager::INSTANCE.TriggerPreset(args[0]))
        return std::make_unique<Command::Result>("Preset Triggered");
    else
        return std::make_unique<Command::ErrorResult>("Not found");
}

TriggerPresetInFutureCommand::TriggerPresetInFutureCommand() :
    Command("Trigger Command Preset In Future") {
    args.push_back(CommandArg("id", "string", "Identifier"));
    args.push_back(CommandArg("ms", "int", "MS In Future").setRange(0, 86400000));
    args.push_back(CommandArg("name", "datalist", "Preset Name").setContentListUrl("api/commandPresets?names=true"));
}
std::unique_ptr<Command::Result> TriggerPresetInFutureCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 3) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    long long t = GetTimeMS();
    int val = std::atoi(args[1].c_str());
    long long tv = t + val;
    Timers::INSTANCE.addTimer(args[0], tv, args[2]);
    return std::make_unique<Command::Result>("Timer Started");
}

TriggerRemotePresetCommand::TriggerRemotePresetCommand() :
    Command("Remote Trigger Command Preset") {
    args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
    args.push_back(CommandArg("name", "string", "Preset Name"));
}
std::unique_ptr<Command::Result> TriggerRemotePresetCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Trigger Command Preset", newargs);
}

TriggerPresetSlotCommand::TriggerPresetSlotCommand() :
    Command("Trigger Command Preset Slot") {
    args.push_back(CommandArg("slot", "int", "Preset Slot").setRange(1, 255));
}
std::unique_ptr<Command::Result> TriggerPresetSlotCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    int val = std::atoi(args[0].c_str());
    if (CommandManager::INSTANCE.TriggerPreset(val))
        return std::make_unique<Command::Result>("Preset Triggered");
    else
        return std::make_unique<Command::ErrorResult>("Not found");
}

TriggerRemotePresetSlotCommand::TriggerRemotePresetSlotCommand() :
    Command("Remote Trigger Command Preset Slot") {
    args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
    args.push_back(CommandArg("slot", "int", "Preset Slot").setRange(1, 255));
}
std::unique_ptr<Command::Result> TriggerRemotePresetSlotCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 2) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Trigger Command Preset Slot", newargs);
}

TriggerMultiplePresetSlotsCommand::TriggerMultiplePresetSlotsCommand() :
    Command("Trigger Multiple Command Preset Slots") {
    args.push_back(CommandArg("SlotA", "int", "Preset Slot A").setRange(1, 255));
    args.push_back(CommandArg("SlotB", "int", "Preset Slot B").setRange(1, 255));
    args.push_back(CommandArg("SlotC", "int", "Preset Slot C").setRange(1, 255));
    args.push_back(CommandArg("SlotD", "int", "Preset Slot D").setRange(1, 255));
}
std::unique_ptr<Command::Result> TriggerMultiplePresetSlotsCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    for (auto& a : args) {
        if (a.length() > 0) {
            CommandManager::INSTANCE.TriggerPreset(std::atoi(a.c_str()));
        }
    }
    return std::make_unique<Command::Result>("Presets Triggered");
}

TriggerMultiplePresetsCommand::TriggerMultiplePresetsCommand() :
    Command("Trigger Multiple Command Presets") {
    args.push_back(CommandArg("NameA", "datalist", "Preset Name 1").setContentListUrl("api/commandPresets?names=true"));
    args.push_back(CommandArg("NameB", "datalist", "Preset Name 2").setContentListUrl("api/commandPresets?names=true"));
    args.push_back(CommandArg("NameC", "datalist", "Preset Name 3").setContentListUrl("api/commandPresets?names=true"));
    args.push_back(CommandArg("NameD", "datalist", "Preset Name 4").setContentListUrl("api/commandPresets?names=true"));
    args.push_back(CommandArg("NameE", "datalist", "Preset Name 5").setContentListUrl("api/commandPresets?names=true"));
    args.push_back(CommandArg("NameF", "datalist", "Preset Name 6").setContentListUrl("api/commandPresets?names=true"));
}
std::unique_ptr<Command::Result> TriggerMultiplePresetsCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    for (auto& a : args) {
        if (!a.empty()) {
            CommandManager::INSTANCE.TriggerPreset(a);
        }
    }
    return std::make_unique<Command::Result>("Presets Triggered");
}

RunScriptEvent::RunScriptEvent() :
    Command("Run Script") {
    args.push_back(CommandArg("script", "string", "Script Name").setContentListUrl("api/scripts"));
    args.push_back(CommandArg("args", "string", "Script Arguments").setAdjustable());
    args.push_back(CommandArg("env", "string", "Environment Variables"));
}
std::unique_ptr<Command::Result> RunScriptEvent::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::string script = args[0];
    std::string scriptArgs = "";
    std::vector<std::pair<std::string, std::string>> envVars;
    if (args.size() > 1) {
        scriptArgs = args[1];
    }
    if (args.size() > 2) {
        std::vector<std::string> s = splitWithQuotes(args[2], ',');
        for (auto& a : s) {
            int idx = a.find('=');
            envVars.push_back(std::pair<std::string, std::string>(a.substr(0, idx), a.substr(idx + 1)));
        }
    }

    RunScript(script, scriptArgs, envVars);
    return std::make_unique<Command::Result>("Script Started");
}

StartEffectCommand::StartEffectCommand() :
    Command("Effect Start") {
    args.push_back(CommandArg("effect", "string", "Effect Name").setContentListUrl("api/effects"));
    args.push_back(CommandArg("startChannel", "int", "Start Channel"));
    args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
    args.push_back(CommandArg("bg", "bool", "Background"));
    args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
    args.push_back(CommandArg("Model", "string", "Model").setContentListUrl("api/models?simple=true", ""));
}
std::unique_ptr<Command::Result> StartEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int startChannel = 0;
    bool loop = false;
    bool bg = false;
    bool iNR = false;
    bool isRunning = false;
    std::string Model = "";

    if (args.size() > 1) {
        startChannel = std::atoi(args[1].c_str());
    }
    if (args.size() > 2) {
        loop = args[2] == "true" || args[2] == "1";
    }
    if (args.size() > 3) {
        bg = args[3] == "true" || args[3] == "1";
    }
    if (args.size() > 4) {
        iNR = args[4] == "true" || args[4] == "1";
    }
    if (args.size() > 5) {
        Model = args[5].c_str();
        if (Model != "") {
            auto m_model = PixelOverlayManager::INSTANCE.getModel(Model);
            if (!m_model) {
                LogErr(VB_COMMAND, "Invalid Pixel Overlay Model: '%s'\n", Model.c_str());
            } else {
                LogDebug(VB_COMMAND, "Overlay Model start channel modified to be : '%i'\n", m_model->getStartChannel() + startChannel);
                startChannel = m_model->getStartChannel() + startChannel;
            }
        }
    }

    const Json::Value RunningEffects = GetRunningEffectsJson();

    if (iNR) {
        for (int x = 0; x < RunningEffects.size(); x++) {
            Json::Value v = RunningEffects[x];
            if (v["name"].asString() == args[0]) {
                isRunning = true;
                LogDebug(VB_COMMAND, "Effect Already running, configured not to start it again\n");
                return std::make_unique<Command::Result>("Effect NOT Started");
            }
        }
    }

    StartEffect(args[0], startChannel, loop, bg);
    return std::make_unique<Command::Result>("Effect Started");
}

StartFSEQAsEffectCommand::StartFSEQAsEffectCommand() :
    Command("FSEQ Effect Start") {
    args.push_back(CommandArg("effect", "string", "FSEQ Name").setContentListUrl("api/sequence"));
    args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
    args.push_back(CommandArg("bg", "bool", "Background"));
}
std::unique_ptr<Command::Result> StartFSEQAsEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    bool loop = false;
    bool bg = false;

    if (args.size() > 1) {
        loop = args[1] == "true" || args[1] == "1";
    }
    if (args.size() > 2) {
        bg = args[2] == "true" || args[2] == "1";
    }
    StartFSEQAsEffect(args[0], loop, bg);
    return std::make_unique<Command::Result>("Effect Started");
}

StopEffectCommand::StopEffectCommand() :
    Command("Effect Stop", "Stop the specified effect.") {
    args.push_back(CommandArg("effect", "datalist", "Effect Name").setContentListUrl("api/effects/ALL"));
}
std::unique_ptr<Command::Result> StopEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    StopEffect(args[0]);
    return std::make_unique<Command::Result>("Effect Stopped");
}

StopAllEffectsCommand::StopAllEffectsCommand() :
    Command("Effects Stop", "Stop all running effects.") {
}
std::unique_ptr<Command::Result> StopAllEffectsCommand::run(const std::vector<std::string>& args) {
    StopAllEffects();
    return std::make_unique<Command::Result>("Effects Stopped");
}

StopFSEQAsEffectCommand::StopFSEQAsEffectCommand() :
    Command("FSEQ Effect Stop") {
    args.push_back(CommandArg("effect", "string", "FSEQ Name").setContentListUrl("api/sequence"));
}
std::unique_ptr<Command::Result> StopFSEQAsEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    StopEffect(args[0]);
    return std::make_unique<Command::Result>("Effect Stopped");
}

StartRemoteEffectCommand::StartRemoteEffectCommand() :
    Command("Remote Effect Start") {
    args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
    args.push_back(CommandArg("effect", "string", "Effect Name"));
    args.push_back(CommandArg("startChannel", "int", "Start Channel"));
    args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
    args.push_back(CommandArg("bg", "bool", "Background"));
}
std::unique_ptr<Command::Result> StartRemoteEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Effect Start", newargs);
}

StartRemoteFSEQEffectCommand::StartRemoteFSEQEffectCommand() :
    Command("Remote FSEQ Effect Start") {
    args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
    args.push_back(CommandArg("fseq", "string", "FSEQ Name"));
    args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
    args.push_back(CommandArg("bg", "bool", "Background"));
}
std::unique_ptr<Command::Result> StartRemoteFSEQEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "FSEQ Effect Start", newargs);
}

StopRemoteEffectCommand::StopRemoteEffectCommand() :
    Command("Remote Effect Stop") {
    args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
    args.push_back(CommandArg("effect", "string", "Effect Name"));
}
std::unique_ptr<Command::Result> StopRemoteEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Effect Stop", newargs);
}

StartRemotePlaylistCommand::StartRemotePlaylistCommand() :
    Command("Remote Playlist Start") {
    args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
    args.push_back(CommandArg("playlist", "string", "Playlist Name"));
    args.push_back(CommandArg("loop", "bool", "Loop Effect").setDefaultValue("true"));
    args.push_back(CommandArg("ifNotRunning", "bool", "If Not Running", true).setDefaultValue("false"));
}
std::unique_ptr<Command::Result> StartRemotePlaylistCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Start Playlist", newargs);
}

RunRemoteScriptEvent::RunRemoteScriptEvent() :
    Command("Remote Run Script") {
    args.push_back(CommandArg("remote", "datalist", "Remote IP").setContentListUrl("api/remotes"));
    args.push_back(CommandArg("script", "string", "Script Name"));
    args.push_back(CommandArg("args", "string", "Script Arguments").setAdjustable());
    args.push_back(CommandArg("env", "string", "Environment Variables"));
}
std::unique_ptr<Command::Result> RunRemoteScriptEvent::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Run Script", newargs);
}

AllLightsOffCommand::AllLightsOffCommand() :
    Command("All Lights Off", "Turn all lights off.") {
}
std::unique_ptr<Command::Result> AllLightsOffCommand::run(const std::vector<std::string>& args) {
    sequence->SendBlankingData();
    return std::make_unique<Command::Result>("All Lights Off");
}
