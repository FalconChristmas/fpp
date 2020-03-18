#include <thread>

#include "Commands.h"
#include "log.h"
#include "common.h"

#include "PlaylistCommands.h"
#include "EventCommands.h"
#include "MediaCommands.h"
#include "MultiSync.h"

CommandManager CommandManager::INSTANCE;
Command::Command(const std::string &n) : name(n) {
}

Command::~Command() {
}

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
        if (ar.defaultValue != "") {
            a["default"] = ar.defaultValue;
        }
        if (ar.adjustable) {
            a["adjustable"] = true;
            if (ar.adjustableGetValueURL != "") {
                a["adjustableGetValueURL"] = ar.adjustableGetValueURL;
            }
        }

        cmd["args"].append(a);
    }
    return cmd;
}


CommandManager::CommandManager() {
}


void CommandManager::Init() {
    addCommand(new StopPlaylistCommand());
    addCommand(new StopGracefullyPlaylistCommand());
    addCommand(new RestartPlaylistCommand());
    addCommand(new NextPlaylistCommand());
    addCommand(new PrevPlaylistCommand());
    addCommand(new StartPlaylistCommand());
    addCommand(new StartPlaylistAtCommand());
    addCommand(new StartPlaylistAtRandomCommand());
    addCommand(new InsertPlaylistCommand());
    addCommand(new TriggerEventCommand());
    addCommand(new TriggerMultipleEventsCommand());
    addCommand(new RunScriptEvent());
    addCommand(new StartEffectCommand());
    addCommand(new StartFSEQAsEffectCommand());
    addCommand(new StopFSEQAsEffectCommand());
    addCommand(new StopEffectCommand());
    addCommand(new SetVolumeCommand());
    addCommand(new AdjustVolumeCommand());
    addCommand(new IncreaseVolumeCommand());
    addCommand(new DecreaseVolumeCommand());
    addCommand(new URLCommand());
    addCommand(new AllLightsOffCommand());
    
    addCommand(new TriggerRemoteEventCommand());
    addCommand(new StartRemoteEffectCommand());
    addCommand(new StopRemoteEffectCommand());
    addCommand(new RunRemoteScriptEvent());
    addCommand(new StartRemoteFSEQEffectCommand());
}
CommandManager::~CommandManager() {
    Cleanup();
}
void CommandManager::Cleanup() {
    for (auto &a : commands) {
        delete a.second;
    }
    commands.clear();
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
        if (!a.second->hidden()) {
            ret.append(a.second->getDescription());
        }
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
std::unique_ptr<Command::Result> CommandManager::runRemoteCommand(const std::string &remote, const std::string &cmd, const std::vector<std::string> &args) {
    
    size_t startPos = 0;
    std::string command = cmd;
    while ((startPos = command.find(" ", startPos)) != std::string::npos) {
        command.replace(startPos, 1, "%20");
        startPos += 3;
    }
    
    std::string url = "http://" + remote + ":32322/command/" + command;
    
    Json::Value j;
    for (auto &a : args) {
        j.append(a);
    }
    std::vector<std::string> uargs;
    uargs.push_back(url);
    uargs.push_back("POST");
    
    std::string config = SaveJsonToString(j);
    
    uargs.push_back(config);
    return run("URL", uargs);
}

std::unique_ptr<Command::Result> CommandManager::run(const std::string &command, const Json::Value &argsArray) {
    auto f = commands.find(command);
    if (f != commands.end()) {
        LogDebug(VB_COMMAND, "Running command \"%s\"\n", command.c_str());
        std::vector<std::string> args;
        for (int x = 0; x < argsArray.size(); x++) {
            args.push_back(argsArray[x].asString());
        }
        return f->second->run(args);
    }
    LogWarn(VB_COMMAND, "No command found for \"%s\"\n", command.c_str());
    return std::make_unique<Command::ErrorResult>("No Command: " + command);
}
std::unique_ptr<Command::Result> CommandManager::run(const Json::Value &cmd) {
    std::string command = cmd["command"].asString();
    if (cmd.isMember("multisyncCommand")) {
        bool multisync = cmd["multisyncCommand"].asBool();
        if (multisync) {
            std::string hosts = cmd.isMember("multisyncHosts") ? cmd["multisyncHosts"].asString() : "";
            std::vector<std::string> args;
            for (int x = 0; x < cmd["args"].size(); x++) {
                args.push_back(cmd["args"][x].asString());
            }
            MultiSync::INSTANCE.SendFPPCommandPacket(hosts, command, args);
        }
    }
    return run(command, cmd["args"]);
}


const std::shared_ptr<httpserver::http_response> CommandManager::render_GET(const httpserver::http_request &req) {
    int plen = req.get_path_pieces().size();
    std::string p1 = req.get_path_pieces()[0];
    if (p1 == "commands") {
        if (plen > 1) {
            std::string command = req.get_path_pieces()[1];
            auto f = commands.find(command);
            if (f != commands.end()) {
                Json::Value result = f->second->getDescription();
                std::string resultStr = SaveJsonToString(result, "  ");
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
            }
        } else {
            Json::Value result = getDescriptions();
            std::string resultStr = SaveJsonToString(result, "  ");
            return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
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
                    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(r->get(), 500, "text/plain"));
                }
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(r->get(), 200, "text/plain"));
            } else {
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Timeout running command", 500, "text/plain"));
            }
        }
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
}

const std::shared_ptr<httpserver::http_response> CommandManager::render_POST(const httpserver::http_request &req) {
    std::string p1 = req.get_path_pieces()[0];
    if (p1 == "command") {
        if (req.get_path_pieces().size() > 1) {
            std::string command = req.get_path_pieces()[1];
            Json::Value val = LoadJsonFromString(req.get_content());
            std::vector<std::string> args;
            for (int x = 0; x < val.size(); x++) {
                args.push_back(val[x].asString());
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
                        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(r->get(), 500, r->contentType()));
                    }
                    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(r->get(), 200, r->contentType()));
                } else {
                    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Timeout running command", 500, "text/plain"));
                }
            }
        } else {
            Json::Value val = LoadJsonFromString(req.get_content());
            std::unique_ptr<Command::Result> r = run(val);
            int count = 0;
            while (!r->isDone() && count < 1000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                count++;
            }
            if (r->isDone()) {
                if (r->isError()) {
                    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(r->get(), 500, r->contentType()));
                }
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(r->get(), 200, r->contentType()));
            } else {
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Timeout running command", 500, "text/plain"));
            }
        }
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
    }
    return std::shared_ptr<httpserver::http_response>(new httpserver::string_response("Not Found", 404, "text/plain"));
    
}
