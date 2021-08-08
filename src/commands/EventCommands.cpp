
#include "fpp-pch.h"

#include "EventCommands.h"
#include "effects.h"
#include "scripts.h"

std::unique_ptr<Command::Result> TriggerPresetCommand::run(const std::vector<std::string>& args) {
    if (args.size() != 1) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    if (CommandManager::INSTANCE.TriggerPreset(args[0]))
        return std::make_unique<Command::Result>("Preset Triggered");
    else
        return std::make_unique<Command::ErrorResult>("Not found");
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

std::unique_ptr<Command::Result> StartEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int startChannel = 0;
    bool loop = false;
    bool bg = false;
    bool iNR = false;
    bool isRunning = false;

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

std::unique_ptr<Command::Result> StopEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    StopEffect(args[0]);
    return std::make_unique<Command::Result>("Effect Stopped");
}

std::unique_ptr<Command::Result> StopAllEffectsCommand::run(const std::vector<std::string>& args) {
    StopAllEffects();
    return std::make_unique<Command::Result>("Effects Stopped");
}
std::unique_ptr<Command::Result> StopFSEQAsEffectCommand::run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    StopEffect(args[0]);
    return std::make_unique<Command::Result>("Effect Stopped");
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

std::unique_ptr<Command::Result> AllLightsOffCommand::run(const std::vector<std::string>& args) {
    sequence->SendBlankingData();
    return std::make_unique<Command::Result>("All Lights Off");
}
