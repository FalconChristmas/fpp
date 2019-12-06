
#include "EventCommands.h"
#include "events.h"
#include "common.h"

#include "effects.h"
#include "scripts.h"
#include "Sequence.h"

std::unique_ptr<Command::Result> TriggerEventCommand::run(const std::vector<std::string> &args) {
    if (args.size() != 2) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    int maj = std::atoi(args[0].c_str());
    int min = std::atoi(args[1].c_str());
    TriggerEvent(maj, min);
    return std::make_unique<Command::Result>("Event Triggered");
}
std::unique_ptr<Command::Result> TriggerRemoteEventCommand::run(const std::vector<std::string> &args) {
    if (args.size() != 3) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Trigger Event", newargs);
}

std::unique_ptr<Command::Result> TriggerMultipleEventsCommand::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    for (auto &a : args) {
        if (a.length() > 0) {
            TriggerEventByID(a.c_str());
        }
    }
    return std::make_unique<Command::Result>("Events Triggered");
}

std::unique_ptr<Command::Result> RunScriptEvent::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::string script = args[0];
    std::string scriptArgs = "";
    std::vector<std::pair<std::string,std::string>> envVars;
    if (args.size() > 1) {
        scriptArgs = args[1];
    }
    if (args.size() > 2) {
       std::vector<std::string> s = splitWithQuotes(args[2], ',');
        for (auto &a : s) {
            int idx = a.find('=');
            envVars.push_back(std::pair<std::string,std::string>(a.substr(0, idx), a.substr(idx + 1)));
        }
    }
    
    RunScript(script, scriptArgs, envVars);
    return std::make_unique<Command::Result>("Script Started");
}


std::unique_ptr<Command::Result> StartEffectCommand::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int startChannel = 0;
    bool loop = false;
    bool bg = false;
    
    if (args.size() > 1) {
        startChannel = std::atoi(args[1].c_str());
    }
    if (args.size() > 2) {
        loop = args[2] == "true" || args[2] == "1";
    }
    if (args.size() > 3) {
        bg = args[3] == "true" || args[3] == "1";
    }
    StartEffect(args[0], startChannel, loop, bg);
    return std::make_unique<Command::Result>("Effect Started");
}
std::unique_ptr<Command::Result> StartFSEQAsEffectCommand::run(const std::vector<std::string> &args) {
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

std::unique_ptr<Command::Result> StopEffectCommand::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    StopEffect(args[0]);
    return std::make_unique<Command::Result>("Effect Stopped");
}
std::unique_ptr<Command::Result> StopFSEQAsEffectCommand::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    StopEffect(args[0]);
    return std::make_unique<Command::Result>("Effect Stopped");
}

std::unique_ptr<Command::Result> StartRemoteEffectCommand::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Effect Start", newargs);
}
std::unique_ptr<Command::Result> StartRemoteFSEQEffectCommand::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "FSEQ Effect Start", newargs);
}
std::unique_ptr<Command::Result> StopRemoteEffectCommand::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Effect Stop", newargs);
}
std::unique_ptr<Command::Result> RunRemoteScriptEvent::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    std::vector<std::string> newargs;
    for (int x = 1; x < args.size(); x++) {
        newargs.push_back(args[x]);
    }
    return CommandManager::INSTANCE.runRemoteCommand(args[0], "Run Script", newargs);
}

std::unique_ptr<Command::Result> AllLightsOffCommand::run(const std::vector<std::string> &args) {
    sequence->SendBlankingData();
    return std::make_unique<Command::Result>("All Lights Off");
}

