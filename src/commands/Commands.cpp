#include "fpp-pch.h"

#include "EventCommands.h"
#include "MediaCommands.h"
#include "MultiSync.h"
#include "PlaylistCommands.h"

CommandManager CommandManager::INSTANCE;
Command::Command(const std::string& n) :
    name(n),
    description("") {
}
Command::Command(const std::string& n, const std::string& descript) :
    name(n),
    description(descript) {
}
Command::~Command() {}

Json::Value Command::getDescription() {
    Json::Value cmd;
    cmd["name"] = name;
    if (description != "") {
        cmd["description"] = description;
    }
    for (auto& ar : args) {
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
            for (auto& x : ar.contentList) {
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
    LoadPresets();

    addCommand(new StopPlaylistCommand());
    addCommand(new StopGracefullyPlaylistCommand());
    addCommand(new RestartPlaylistCommand());
    addCommand(new NextPlaylistCommand());
    addCommand(new PrevPlaylistCommand());
    addCommand(new StartPlaylistCommand());
    addCommand(new TogglePlaylistCommand());
    addCommand(new StartPlaylistAtCommand());
    addCommand(new StartPlaylistAtRandomCommand());
    addCommand(new InsertPlaylistCommand());
    addCommand(new InsertPlaylistImmediate());
    addCommand(new PlayMediaCommand());
    /* These too are currently disabled because stopping a Video
     * can cause Siginal 6 in VLC Player
     *
     * ./include/vlc_atomic.h:64: vlc_atomic_rc_dec: Assertion `prev' failed.
     */
    //addCommand(new StopMediaCommand());
    //addCommand(new StopAllMediaCommand());
    addCommand(new PlaylistPauseCommand());
    addCommand(new PlaylistResumeCommand());
    addCommand(new TriggerPresetCommand());
    addCommand(new TriggerPresetSlotCommand());
    addCommand(new TriggerMultiplePresetSlotsCommand());
    addCommand(new RunScriptEvent());
    addCommand(new StartEffectCommand());
    addCommand(new StartFSEQAsEffectCommand());
    addCommand(new StopFSEQAsEffectCommand());
    addCommand(new StopEffectCommand());
    addCommand(new StopAllEffectsCommand());
    addCommand(new SetVolumeCommand());
    addCommand(new AdjustVolumeCommand());
    addCommand(new IncreaseVolumeCommand());
    addCommand(new DecreaseVolumeCommand());
    addCommand(new URLCommand());
    addCommand(new AllLightsOffCommand());

    addCommand(new TriggerRemotePresetCommand());
    addCommand(new TriggerRemotePresetSlotCommand());
    addCommand(new StartRemoteEffectCommand());
    addCommand(new StopRemoteEffectCommand());
    addCommand(new RunRemoteScriptEvent());
    addCommand(new StartRemoteFSEQEffectCommand());
    addCommand(new StartRemotePlaylistCommand());
}
CommandManager::~CommandManager() {
    Cleanup();
}

void CommandManager::Cleanup() {
    while (!commands.empty()) {
        Command* cmd = commands.begin()->second;
        commands.erase(commands.begin());

        if (cmd->name != "GPIO") { // No idea why deleteing the GPIO command causes a crash on exit
            delete cmd;
        }
    }
    commands.clear();
}

void CommandManager::addCommand(Command* cmd) {
    commands[cmd->name] = cmd;
}
void CommandManager::removeCommand(Command* cmd) {
    commands.erase(commands.find(cmd->name));
}

Json::Value CommandManager::getDescriptions() {
    Json::Value ret;
    for (auto& a : commands) {
        if (!a.second->hidden()) {
            ret.append(a.second->getDescription());
        }
    }
    return ret;
}
std::unique_ptr<Command::Result> CommandManager::run(const std::string& command, const std::vector<std::string>& args) {
    auto f = commands.find(command);
    if (f != commands.end()) {
        LogDebug(VB_COMMAND, "Running command \"%s\"\n", command.c_str());
        return f->second->run(args);
    }
    LogWarn(VB_COMMAND, "No command found for \"%s\"\n", command.c_str());
    return std::make_unique<Command::ErrorResult>("No Command: " + command);
}
std::unique_ptr<Command::Result> CommandManager::runRemoteCommand(const std::string& remote, const std::string& cmd, const std::vector<std::string>& args) {
    size_t startPos = 0;
    std::string command = cmd;
    while ((startPos = command.find(" ", startPos)) != std::string::npos) {
        command.replace(startPos, 1, "%20");
        startPos += 3;
    }

    std::string url = "http://" + remote + ":32322/command/" + command;

    Json::Value j;
    for (auto& a : args) {
        j.append(a);
    }
    std::vector<std::string> uargs;
    uargs.push_back(url);
    uargs.push_back("POST");

    std::string config = SaveJsonToString(j);

    uargs.push_back(config);
    return run("URL", uargs);
}

std::unique_ptr<Command::Result> CommandManager::run(const std::string& command, const Json::Value& argsArray) {
    auto f = commands.find(command);
    if (f != commands.end()) {
        std::vector<std::string> args;
        for (int x = 0; x < argsArray.size(); x++) {
            args.push_back(argsArray[x].asString());
        }
        if (WillLog(LOG_DEBUG, VB_COMMAND)) {
            std::string argString;
            for (auto& a : args) {
                if (!argString.empty()) {
                    argString += ",";
                }
                argString += a;
            }
            LogDebug(VB_COMMAND, "Running command \"%s(%s)\"\n", command.c_str(), argString.c_str());
        }
        return f->second->run(args);
    }
    LogWarn(VB_COMMAND, "No command found for \"%s\"\n", command.c_str());
    return std::make_unique<Command::ErrorResult>("No Command: " + command);
}
std::unique_ptr<Command::Result> CommandManager::run(const Json::Value& cmd) {
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
            return std::make_unique<Command::Result>("Command Sent");
        }
    }
    return run(command, cmd["args"]);
}

const std::shared_ptr<httpserver::http_response> CommandManager::render_GET(const httpserver::http_request& req) {
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
    } else if (p1 == "commandPresets") {
        std::string commandsFile(FPP_DIR_CONFIG "/commandPresets.json");
        Json::Value allCommands;
        if (FileExists(commandsFile)) {
            // Load new config file
            allCommands = LoadJsonFromFile(commandsFile);
        }
        if (plen > 1) {
            if (allCommands.isMember("commands")) {
                std::string p2 = req.get_path_pieces()[1];
                for (int x = 0; x < allCommands["commands"].size(); x++) {
                    if (allCommands["commands"][x]["name"].asString() == p2) {
                        std::string resultStr = SaveJsonToString(allCommands["commands"][x], "  ");
                        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
                    }
                }
            }
        } else {
            if (req.get_arg("names") == "true") {
                Json::Value names;
                if (allCommands.isMember("commands")) {
                    for (int x = 0; x < allCommands["commands"].size(); x++) {
                        names.append(allCommands["commands"][x]["name"].asString());
                    }
                }
                std::string resultStr = SaveJsonToString(names, "  ");
                return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(resultStr, 200, "application/json"));
            }
            std::string resultStr = SaveJsonToString(allCommands, "  ");
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

const std::shared_ptr<httpserver::http_response> CommandManager::render_POST(const httpserver::http_request& req) {
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

Json::Value CommandManager::ReplaceCommandKeywords(Json::Value cmd, std::map<std::string, std::string>& keywords) {
    if (!cmd.isMember("args"))
        return cmd;

    for (int i = 0; i < cmd["args"].size(); i++) {
        std::string arg = cmd["args"][i].asString();

        cmd["args"][i] = ReplaceKeywords(cmd["args"][i].asString(), keywords);
    }

    return cmd;
}

int CommandManager::TriggerPreset(int slot, std::map<std::string, std::string>& keywords) {
    for (auto const& name : presets.getMemberNames()) {
        for (int i = 0; i < presets[name].size(); i++) {
            if (presets[name][i]["presetSlot"].asInt() == slot) {
                Json::Value cmd = ReplaceCommandKeywords(presets[name][i], keywords);
                run(cmd);
            }
        }
    }

    return 1;
}

int CommandManager::TriggerPreset(int slot) {
    std::map<std::string, std::string> keywords;

    return TriggerPreset(slot, keywords);
}

int CommandManager::TriggerPreset(std::string name, std::map<std::string, std::string>& keywords) {
    if (!presets.isMember(name))
        return 0;

    for (int i = 0; i < presets[name].size(); i++) {
        Json::Value cmd = ReplaceCommandKeywords(presets[name][i], keywords);
        run(cmd);
    }

    return 1;
}

int CommandManager::TriggerPreset(std::string name) {
    std::map<std::string, std::string> keywords;

    return TriggerPreset(name, keywords);
}

void CommandManager::LoadPresets() {
    LogDebug(VB_COMMAND, "Loading Command Presets\n");

    char id[6];
    memset(id, 0, sizeof(id));

    std::string commandsFile(FPP_DIR_CONFIG "/commandPresets.json");

    Json::Value allCommands;

    if (FileExists(commandsFile)) {
        // Load new config file
        allCommands = LoadJsonFromFile(commandsFile);
    } else {
        // Convert any old events to new format
        Json::Value commands(Json::arrayValue);

        for (int major = 1; major <= 25; major++) {
            for (int minor = 1; minor <= 25; minor++) {
                sprintf(id, "%02d_%02d", major, minor);
                std::string filename = "/home/fpp/media/events/";
                filename += id;
                filename += ".fevt";

                if (FileExists(filename)) {
                    LogDebug(VB_COMMAND, "Converting old %s event to a Command Preset\n", id);

                    Json::Value event;
                    if (LoadJsonFromFile(filename, event)) {
                        event.removeMember("majorId");
                        event.removeMember("minorId");
                        event["presetSlot"] = 0;

                        commands.append(event);
                    }
                }
            }
        }

        allCommands["commands"] = commands;
        SaveJsonToFile(allCommands, commandsFile, "\t");
    }

    if (allCommands.isMember("commands")) {
        for (int i = 0; i < allCommands["commands"].size(); i++) {
            if (presets.isMember(allCommands["commands"][i]["name"].asString())) {
                presets[allCommands["commands"][i]["name"].asString()].append(allCommands["commands"][i]);
            } else {
                Json::Value pArr(Json::arrayValue);
                pArr.append(allCommands["commands"][i]);
                presets[allCommands["commands"][i]["name"].asString()] = pArr;
            }
        }
    }
}
