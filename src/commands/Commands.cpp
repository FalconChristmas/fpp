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

#include <thread>

#include "../common.h"
#include "../log.h"

#include "../Events.h"

#include "EventCommands.h"
#include "FileMonitor.h"
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
    if (!description.empty()) {
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
        if (!ar.contentListUrl.empty()) {
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
            if (!ar.adjustableGetValueURL.empty()) {
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

    FileMonitor::INSTANCE.AddFile("CommandManager:CommandPresets.json",
                                  FPP_DIR_CONFIG("/commandPresets.json"),
                                  [this]() {
                                      std::unique_lock<std::mutex> lock(presetsMutex);
                                      MaybeReloadPresets();
                                  });

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
    addCommand(new InsertRandomItemFromPlaylistCommand());
#ifdef HAS_GSTREAMER
    addCommand(new PlayMediaCommand());
    addCommand(new StopMediaCommand());
    addCommand(new StopAllMediaCommand());
    addCommand(new StopMediaSlotCommand());
    addCommand(new SetSlotVolumeCommand());
    addCommand(new MediaSlotStatusCommand());
#endif
#ifdef HAS_AES67_GSTREAMER
    addCommand(new AES67ApplyCommand());
    addCommand(new AES67CleanupCommand());
    addCommand(new AES67TestCommand());
    addCommand(new ApplyRoutingPresetCommand());
#endif
#ifdef HAS_OPUS_RTP_GSTREAMER
    addCommand(new OpusRTPApplyCommand());
    addCommand(new OpusRTPCleanupCommand());
#endif
    addCommand(new PlaylistPauseCommand());
    addCommand(new PlaylistResumeCommand());
    addCommand(new TriggerPresetCommand());
    addCommand(new TriggerPresetInFutureCommand());
    addCommand(new TriggerPresetSlotCommand());
    addCommand(new TriggerMultiplePresetsCommand());
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
    addCommand(new SwitchToPlayerModeCommand());
    addCommand(new SwitchToRemoteModeCommand());

    std::function<void(const std::string&, const std::string&)> f =
        [](const std::string& topic, const std::string& payload) {
            if (topic.size() <= 13) {
                Json::Value val;
                bool success = LoadJsonFromString(payload, val);
                if (success && val.isObject()) {
                    CommandManager::INSTANCE.run(val);
                } else {
                    LogWarn(VB_COMMAND, "Invalid JSON Payload: %s\n", payload.c_str());
                }
            } else {
                std::vector<std::string> args;
                std::string command;

                std::string ntopic = topic.substr(13); // remove /set/command/
                args = splitWithQuotes(ntopic, '/');
                command = args[0];
                args.erase(args.begin());
                bool foundp = false;
                for (int x = 0; x < args.size(); x++) {
                    if (args[x] == "{Payload}") {
                        args[x] = payload;
                        foundp = true;
                    }
                }
                if (!payload.empty() && !foundp) {
                    args.push_back(payload);
                }
                if (args.size() == 0 && payload != "") {
                    Json::Value val = LoadJsonFromString(payload);
                    if (val.isObject()) {
                        CommandManager::INSTANCE.run(command, val);
                    } else {
                        LogWarn(VB_COMMAND, "Invalid JSON Payload for topic %s: %s\n", topic.c_str(), payload.c_str());
                    }
                } else {
                    CommandManager::INSTANCE.run(command, args);
                }
            }
        };
    Events::AddCallback("/set/command", f);
    Events::AddCallback("/set/command/#", f);
}
CommandManager::~CommandManager() {
    Cleanup();
}

void CommandManager::Cleanup() {
    // Idempotent: main() calls this during shutdown, and ~CommandManager() calls
    // it again at static-destruction time. The FileMonitor::INSTANCE.RemoveFile()
    // reach-in below locks a mutex on the global FileMonitor singleton, which may
    // already be destroyed by then (cross-TU static destruction order) -- locking
    // a destroyed mutex throws out of the noexcept dtor -> std::terminate. A
    // function-local guard keeps the second call a no-op without changing the
    // (plugin-facing) Commands.h ABI.
    static std::atomic<bool> cleanedUp{false};
    if (cleanedUp.exchange(true)) {
        return;
    }
    FileMonitor::INSTANCE.RemoveFile("CommandManager:CommandPresets.json", FPP_DIR_CONFIG("/commandPresets.json"));
    while (!commands.empty()) {
        Command* cmd = commands.begin()->second;
        commands.erase(commands.begin());

        delete cmd;
    }
    commands.clear();
}

void CommandManager::addCommand(Command* cmd) {
    commands[cmd->name] = cmd;
}
void CommandManager::removeCommand(Command* cmd) {
    auto a = commands.find(cmd->name);
    if (a != commands.end()) {
        commands.erase(a);
    }
}
void CommandManager::removeCommand(const std::string& cmdName) {
    auto a = commands.find(cmdName);
    if (a != commands.end()) {
        commands.erase(a);
    }
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

        // Publish MQTT event for command execution
        Json::Value payload;
        payload["command"] = command;
        payload["args"] = Json::Value(Json::arrayValue);
        for (const auto& arg : args) {
            payload["args"].append(arg);
        }
        payload["trigger"] = "internal";
        std::string topic = "command/run";
        Events::Publish(topic, SaveJsonToString(payload));

        return f->second->run(args);
    }
    LogWarn(VB_COMMAND, "No command found for \"%s\"\n", command.c_str());
    return std::make_unique<Command::ErrorResult>("No Command: " + command);
}

std::unique_ptr<Command::Result> CommandManager::run(const std::string& command, const Json::Value& argsArray) {
    auto f = commands.find(command);
    if (f != commands.end()) {
        std::vector<std::string> args;
        for (int x = 0; x < argsArray.size(); x++) {
            if (argsArray[x].isNull()) {
                args.push_back("");
            } else {
                args.push_back(argsArray[x].asString());
            }
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

        // Publish MQTT event for command execution
        Json::Value payload;
        payload["command"] = command;
        payload["args"] = Json::Value(Json::arrayValue);
        for (const auto& arg : args) {
            payload["args"].append(arg);
        }
        payload["trigger"] = "ui";
        std::string topic = "command/run";
        std::string payloadStr = SaveJsonToString(payload);
        LogWarn(VB_COMMAND, "JSONVAL MQTT Publishing command: %s, payload: %s\n", topic.c_str(), payloadStr.c_str());
        Events::Publish(topic, payloadStr);

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
                if (cmd["args"][x].isNull()) {
                    args.push_back("");
                } else {
                    args.push_back(cmd["args"][x].asString());
                }
            }
            MultiSync::INSTANCE.SendFPPCommandPacket(hosts, command, args);
            return std::make_unique<Command::Result>("Command Sent");
        }
    }
    return run(command, cmd["args"]);
}

// --------------------------------------------------------------------------
// OpenAPI docs for the /command(s) and /commandPresets endpoints handled below.
// --------------------------------------------------------------------------

/**
 * List all available commands and their argument descriptions.
 *
 * @route GET /api/commands
 * @response 200 Array of command descriptions.
 */

/**
 * Get the description of a single command by name.
 *
 * @route GET /api/commands/{command}
 * @response 200 The command description.
 * @response 404 No command with that name exists.
 */

/**
 * Get the saved command presets (config/commandPresets.json).
 *
 * @route GET /api/commandPresets
 * @param boolean names Return just the preset names instead of full definitions.
 * @response 200 Command presets (or preset names when `names=true`).
 */

/**
 * Get a single command preset by name.
 *
 * @route GET /api/commandPresets/{name}
 * @response 200 The preset definition.
 */

/**
 * Run a command by name via GET, passing arguments as extra path segments
 * (e.g. /api/command/Volume%20Set/50).
 *
 * @route GET /api/command/{command}
 * @response 200 Command result (text/plain).
 * @response 404 No command with that name exists.
 * @response 500 The command errored or timed out.
 */
HttpResponsePtr CommandManager::render_GET(const HttpRequestPtr& req) {
    auto parts = getPathPieces(req->path());
    int plen = parts.size();
    std::string p1 = parts[0];

    if (p1 == "commands") {
        if (plen > 1) {
            std::string command = parts[1];
            auto f = commands.find(command);
            if (f != commands.end()) {
                Json::Value result = f->second->getDescription();
                std::string resultStr = SaveJsonToString(result, "  ");
                return makeStringResponse(resultStr, 200, "application/json");
            }
        } else {
            Json::Value result = getDescriptions();
            std::string resultStr = SaveJsonToString(result, "  ");
            return makeStringResponse(resultStr, 200, "application/json");
        }
    } else if (p1 == "commandPresets") {
        std::string commandsFile = FPP_DIR_CONFIG("/commandPresets.json");
        Json::Value allCommands;
        if (FileExists(commandsFile)) {
            // Load new config file
            allCommands = LoadJsonFromFile(commandsFile);
        }
        if (plen > 1) {
            if (allCommands.isMember("commands")) {
                std::string p2 = parts[1];
                for (int x = 0; x < allCommands["commands"].size(); x++) {
                    if (allCommands["commands"][x]["name"].asString() == p2) {
                        std::string resultStr = SaveJsonToString(allCommands["commands"][x], "  ");
                        return makeStringResponse(resultStr, 200, "application/json");
                    }
                }
            }
        } else {
            if (getRequestArg(req, "names") == "true") {
                Json::Value names;
                if (allCommands.isMember("commands")) {
                    for (int x = 0; x < allCommands["commands"].size(); x++) {
                        names.append(allCommands["commands"][x]["name"].asString());
                    }
                }
                std::string resultStr = SaveJsonToString(names, "  ");
                return makeStringResponse(resultStr, 200, "application/json");
            }
            std::string resultStr = SaveJsonToString(allCommands, "  ");
            return makeStringResponse(resultStr, 200, "application/json");
        }
    } else if (p1 == "command" && plen > 1) {
        std::string command = parts[1];
        std::vector<std::string> args;
        for (int x = 2; x < plen; x++) {
            args.push_back(parts[x]);
        }
        auto f = commands.find(command);
        if (f != commands.end()) {
            LogDebug(VB_COMMAND, "Running command \"%s\"\n", command.c_str());

            // Publish MQTT event for command execution
            Json::Value payload;
            payload["command"] = command;
            payload["args"] = Json::Value(Json::arrayValue);
            for (const auto& arg : args) {
                payload["args"].append(arg);
            }
            payload["trigger"] = "api-get";
            std::string topic = "command/run";
            std::string payloadStr = SaveJsonToString(payload);
            LogWarn(VB_COMMAND, "GET MQTT Publishing command: %s, payload: %s\n", topic.c_str(), payloadStr.c_str());
            Events::Publish(topic, payloadStr);

            std::unique_ptr<Command::Result> r = f->second->run(args);
            int count = 0;
            while (!r->isDone() && count < 1000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                count++;
            }
            if (r->isDone()) {
                if (r->isError()) {
                    return makeStringResponse(r->get(), 500, "text/plain");
                }
                return makeStringResponse(r->get(), 200, "text/plain");
            } else {
                return makeStringResponse("Timeout running command", 500, "text/plain");
            }
        }
        return makeStringResponse("Not Found", 404, "text/plain");
    }
    return makeStringResponse("Not Found", 404, "text/plain");
}

/**
 * Run a command described by the posted JSON object (with `command` and `args`).
 *
 * @route POST /api/command
 * @body {"command": "Volume Set", "args": ["50"]}
 * @response 200 Command result.
 * @response 500 The command errored or timed out.
 */

/**
 * Run a named command, passing its arguments as a JSON array in the body.
 *
 * @route POST /api/command/{command}
 * @body ["arg1", "arg2"]
 * @response 200 Command result.
 * @response 500 The command errored or timed out.
 */
HttpResponsePtr CommandManager::render_POST(const HttpRequestPtr& req) {
    auto parts = getPathPieces(req->path());
    std::string p1 = parts[0];
    if (p1 == "command") {
        if (parts.size() > 1) {
            std::string command = parts[1];
            Json::Value val = LoadJsonFromString(std::string(getRequestContent(req)));
            std::vector<std::string> args;
            for (int x = 0; x < val.size(); x++) {
                args.push_back(val[x].asString());
            }
            auto f = commands.find(command);
            if (f != commands.end()) {
                LogDebug(VB_COMMAND, "Running command \"%s\"\n", command.c_str());

                // Publish MQTT event for command execution
                Json::Value payload;
                payload["command"] = command;
                payload["args"] = Json::Value(Json::arrayValue);
                for (const auto& arg : args) {
                    payload["args"].append(arg);
                }
                payload["trigger"] = "api-post";
                std::string topic = "command/run";
                std::string payloadStr = SaveJsonToString(payload);
                LogWarn(VB_COMMAND, "POST MQTT Publishing command: %s, payload: %s\n", topic.c_str(), payloadStr.c_str());
                Events::Publish(topic, payloadStr);

                std::unique_ptr<Command::Result> r = f->second->run(args);
                int count = 0;
                while (!r->isDone() && count < 1000) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    count++;
                }
                if (r->isDone()) {
                    if (r->isError()) {
                        return makeStringResponse(r->get(), 500, r->contentType());
                    }
                    return makeStringResponse(r->get(), 200, r->contentType());
                } else {
                    return makeStringResponse("Timeout running command", 500, "text/plain");
                }
            }
        } else {
            std::string command(getRequestContent(req));
            LogDebug(VB_COMMAND, "Received command: \"%s\"\n", command.c_str());
            Json::Value val = LoadJsonFromString(command);
            std::unique_ptr<Command::Result> r = run(val);
            int count = 0;
            while (!r->isDone() && count < 1000) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                count++;
            }
            if (r->isDone()) {
                if (r->isError()) {
                    return makeStringResponse(r->get(), 500, r->contentType());
                }
                return makeStringResponse(r->get(), 200, r->contentType());
            } else {
                return makeStringResponse("Timeout running command", 500, "text/plain");
            }
        }
        return makeStringResponse("Not Found", 404, "text/plain");
    }
    return makeStringResponse("Not Found", 404, "text/plain");
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
    std::unique_lock<std::mutex> lock(presetsMutex);
    std::list<Json::Value> slotPresets;
    for (auto const& name : presets.getMemberNames()) {
        for (int i = 0; i < presets[name].size(); i++) {
            if (presets[name][i]["presetSlot"].asInt() == slot) {
                Json::Value cmd = ReplaceCommandKeywords(presets[name][i], keywords);
                slotPresets.push_back(cmd);
            }
        }
    }
    lock.unlock();

    // Publish MQTT event for preset slot trigger
    Json::Value payload;
    payload["slot"] = slot;
    if (!keywords.empty()) {
        payload["keywords"] = Json::Value(Json::objectValue);
        for (const auto& kv : keywords) {
            payload["keywords"][kv.first] = kv.second;
        }
    }
    std::string topic = "command/preset/triggered";
    Events::Publish(topic, SaveJsonToString(payload));

    for (auto const& preset : slotPresets) {
        run(preset);
    }
    return 1;
}

int CommandManager::TriggerPreset(int slot) {
    std::map<std::string, std::string> keywords;

    return TriggerPreset(slot, keywords);
}

int CommandManager::TriggerPreset(std::string name, std::map<std::string, std::string>& keywords) {
    std::unique_lock<std::mutex> lock(presetsMutex);
    if (!presets.isMember(name)) {
        if (missingPresets.find(name) == missingPresets.end()) {
            LogWarn(VB_COMMAND, "No preset found for name \"%s\"\n", name.c_str());
            missingPresets.insert(name);
        }
        return 0;
    }

    auto it = presets[name];
    lock.unlock();

    // Publish MQTT event for preset trigger
    Json::Value payload;
    payload["preset"] = name;
    if (!keywords.empty()) {
        payload["keywords"] = Json::Value(Json::objectValue);
        for (const auto& kv : keywords) {
            payload["keywords"][kv.first] = kv.second;
        }
    }
    std::string topic = "command/preset/triggered";
    Events::Publish(topic, SaveJsonToString(payload));

    for (int i = 0; i < it.size(); i++) {
        Json::Value cmd = ReplaceCommandKeywords(it[i], keywords);
        run(cmd);
    }

    return 1;
}

int CommandManager::TriggerPreset(std::string name) {
    std::map<std::string, std::string> keywords;

    return TriggerPreset(name, keywords);
}

void CommandManager::MaybeReloadPresets() {
    std::string commandsFile = FPP_DIR_CONFIG("/commandPresets.json");
    if (lastPresetTimeStamp < FileTimestamp(commandsFile)) {
        presets.clear();
        missingPresets.clear();
        LoadPresets();
    }
}

void CommandManager::LoadPresets() {
    LogDebug(VB_COMMAND, "Loading Command Presets\n");

    char id[6];
    memset(id, 0, sizeof(id));

    std::string commandsFile = FPP_DIR_CONFIG("/commandPresets.json");

    Json::Value allCommands;

    if (FileExists(commandsFile)) {
        // Load new config file
        lastPresetTimeStamp = FileTimestamp(commandsFile);
        allCommands = LoadJsonFromFile(commandsFile);
    } else {
        // Convert any old events to new format
        Json::Value commands(Json::arrayValue);

        for (int major = 1; major <= 25; major++) {
            for (int minor = 1; minor <= 25; minor++) {
                snprintf(id, sizeof(id), "%02d_%02d", major, minor);
                std::string filename = FPP_DIR_MEDIA(std::string("/events/") + id + ".fevt");

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
        lastPresetTimeStamp = FileTimestamp(commandsFile);
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

bool CommandManager::HasPreset(const std::string& name) {
    std::unique_lock<std::mutex> lock(presetsMutex);
    return presets.isMember(name);
}
