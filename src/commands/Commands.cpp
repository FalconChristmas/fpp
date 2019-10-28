#include <thread>

#include "Commands.h"
#include "log.h"

#include "PlaylistCommands.h"
#include "EventCommands.h"
#include "MediaCommands.h"
#include "util/GPIOUtils.h"

CommandManager CommandManager::INSTANCE;


Json::Value Command::getDescription() {
    Json::Value cmd;
    cmd["name"] = name;
    for (auto &ar : args) {
        Json::Value a;
        a["name"] = ar.name;
        a["type"] = ar.type;
        a["description"] = ar.description;
        a["optional"] = ar.optional;
        
        if (ar.min != ar.max) {
            a["min"] = ar.min;
            a["max"] = ar.max;
        }
        if (ar.contentListUrl != "") {
            a["contentListUrl"] = ar.contentListUrl;
            a["allowBlanks"] = ar.allowBlanks;
        } else if (!ar.contentList.empty()) {
            for (auto &x : ar.contentList) {
                a["contents"].append(x);
            }
        }

        cmd["args"].append(a);
    }
    return cmd;
}


CommandManager::CommandManager() {
}

class GPIOCommand : public Command {
public:
    GPIOCommand(std::vector<std::string> &pins) : Command("GPIO") {
        args.push_back(CommandArg("pin", "string", "Pin").setContentList(pins));
        args.push_back(CommandArg("on", "bool", "On"));
    }
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
        if (args.size() != 2) {
            return std::make_unique<Command::ErrorResult>("Invalid number of arguments. GPIO needs two arguments.");
        }
        std::string n = args[0];
        std::string v = args[1];
        const PinCapabilities &p = PinCapabilities::getPinByName(n);
        if (p.ptr()) {
            p.configPin();
            p.setValue(v == "true" || v == "1");
            std::make_unique<Command::Result>("OK");
        }
        return std::make_unique<Command::ErrorResult>("No Pin Named " + n);
    }
};

void CommandManager::Init() {
    addCommand(new StopPlaylistCommand());
    addCommand(new StopGracefullyPlaylistCommand());
    addCommand(new RestartPlaylistCommand());
    addCommand(new NextPlaylistCommand());
    addCommand(new PrevPlaylistCommand());
    addCommand(new StartPlaylistCommand());
    addCommand(new StartPlaylistAtCommand());
    addCommand(new StartPlaylistAtRandomCommand());
    addCommand(new TriggerEventCommand());
    addCommand(new TriggerMultipleEventsCommand());
    addCommand(new RunScriptEvent());
    addCommand(new RunEffectEvent());
    addCommand(new SetVolumeCommand());
    addCommand(new AdjustVolumeCommand());
    addCommand(new IncreaseVolumeCommand());
    addCommand(new DecreaseVolumeCommand());
    addCommand(new URLCommand());
    
    std::vector<std::string> pins = PinCapabilities::getPinNames();
    if (!pins.empty()) {
        addCommand(new GPIOCommand(pins));
    }
}
CommandManager::~CommandManager() {
}

void CommandManager::addCommand(Command *cmd) {
    commands[cmd->name] = cmd;
}
void CommandManager::removeCommand(Command *cmd) {
    commands.erase(commands.find(cmd->name));
}

Json::Value CommandManager::getDescriptions() {
    Json::Value  ret;
    for (auto &a : commands) {
        ret.append(a.second->getDescription());
    }
    return ret;
}
std::unique_ptr<Command::Result> CommandManager::run(const std::string &command, const std::vector<std::string> &args) {
    auto f = commands.find(command);
    if (f != commands.end()) {
        LogDebug(VB_COMMAND, "Running command \"%s\"\n", command.c_str());
        return f->second->run(args);
    }
    LogWarn(VB_COMMAND, "No command found for \"%s\"\n", command.c_str());
    return std::make_unique<Command::ErrorResult>("No Command: " + command);
}


const httpserver::http_response CommandManager::render_GET(const httpserver::http_request &req) {
    int plen = req.get_path_pieces().size();
    std::string p1 = req.get_path_pieces()[0];
    if (p1 == "commands") {
        if (plen > 1) {
            std::string command = req.get_path_pieces()[1];
            auto f = commands.find(command);
            if (f != commands.end()) {
                Json::Value result = f->second->getDescription();
                Json::StyledWriter fastWriter;
                std::string resultStr = fastWriter.write(result);
                return httpserver::http_response_builder(resultStr, 200, "application/json").string_response();
            }
        } else {
            Json::Value result = getDescriptions();
            Json::StyledWriter fastWriter;
            std::string resultStr = fastWriter.write(result);
            return httpserver::http_response_builder(resultStr, 200, "application/json").string_response();
        }
    } else if (p1 == "command" && plen > 1) {
        std::string command = req.get_path_pieces()[1];
        std::vector<std::string> args;
        for (int x = 2; x < plen; x++) {
            args.push_back(req.get_path_pieces()[x]);
        }
        auto f = commands.find(command);
        if (f != commands.end()) {
            LogDebug(VB_COMMAND, "Running command \"%s\"\n", command.c_str());
            std::unique_ptr<Command::Result> r = f->second->run(args);
            int count = 0;
            while (!r->isDone() && count < 1000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                count++;
            }
            if (r->isDone()) {
                if (r->isError()) {
                    return httpserver::http_response_builder(r->get(), 500, "text/plain");
                }
                return httpserver::http_response_builder(r->get(), 200, "text/plain");
            } else {
                return httpserver::http_response_builder("Timeout running command", 500, "text/plain");
            }
        }
        return httpserver::http_response_builder("Not Found", 404, "text/plain");
    }
    return httpserver::http_response_builder("Not Found", 404, "text/plain").string_response();
}

