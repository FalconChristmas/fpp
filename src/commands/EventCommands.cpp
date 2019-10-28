
#include "EventCommands.h"
#include "events.h"
#include "common.h"

#include "effects.h"
#include "scripts.h"

std::unique_ptr<Command::Result> TriggerEventCommand::run(const std::vector<std::string> &args) {
    if (args.size() != 2) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }
    int maj = std::stoi(args[0]);
    int min = std::stoi(args[1]);
    TriggerEvent(maj, min);
    return std::make_unique<Command::Result>("Event Triggered");
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
    
    RunScript(script, "", envVars);
    return std::make_unique<Command::Result>("Script Started");
}


std::unique_ptr<Command::Result> RunEffectEvent::run(const std::vector<std::string> &args) {
    if (args.empty()) {
        return std::make_unique<Command::ErrorResult>("Not found");
    }

    int startChannel = 0;
    bool loop = false;
    
    if (args.size() > 1) {
        startChannel = std::stoi(args[1]);
    }
    if (args.size() > 2) {
        loop = args[2] == "true" || args[2] == "1";
    }
    StartEffect(args[0], startChannel, loop);
    return std::make_unique<Command::Result>("Effect Started");
}
