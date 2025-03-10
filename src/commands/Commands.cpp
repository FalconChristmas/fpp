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
#include <chrono>        // For timer thread sleep
#include <fcntl.h>       // For file operations in USBRelayOnOffCommand
#include <unistd.h>      // For write/close in USBRelayOnOffCommand
#include <dirent.h>      // Unused but kept for original includes
#include <string.h>      // For memset in LoadPresets

#include "../common.h"
#include "../log.h"

#include "../Events.h"

#include "EventCommands.h"
#include "MediaCommands.h"
#include "MultiSync.h"
#include "PlaylistCommands.h"

// Command Manager Singleton
CommandManager CommandManager::INSTANCE;

// Base Command Class Implementation
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

// USB Relay On/Off Command Class
class USBRelayOnOffCommand : public Command {
public:
    USBRelayOnOffCommand() : Command("USB Relay On/Off", "Turns a USB relay channel ON or OFF, optionally for a specified duration") {
        LogDebug(VB_COMMAND, "Initializing USBRelayOnOffCommand\n");

        // Device argument
        args.emplace_back("Device", "string", "USB Relay device from Others tab", false);
        std::string defaultDevice;

        // Populate devices and protocols from co-other.json
        std::string configFile = FPP_DIR_CONFIG("/co-other.json");
        Json::Value config;
        if (FileExists(configFile) && LoadJsonFromFile(configFile, config)) {
            LogDebug(VB_COMMAND, "Parsing co-other.json for USB relay devices\n");
            if (config.isObject() && config.isMember("channelOutputs")) {
                const Json::Value& outputs = config["channelOutputs"];
                if (outputs.isArray()) {
                    LogDebug(VB_COMMAND, "Found %u channel outputs in co-other.json\n", outputs.size());
                    for (const auto& output : outputs) {
                        std::string deviceName = output.get("device", "").asString();
                        std::string type = output.get("type", "").asString();
                        std::string subType = output.get("subType", "").asString();
                        if (!deviceName.empty() && type == "USBRelay") {
                            std::string protocol;
                            if (subType == "CH340") {
                                protocol = "CH340";
                            } else if (subType == "ICStation") {
                                protocol = "ICstation";
                            } else {
                                LogWarn(VB_COMMAND, "Unknown USBRelay subType '%s' for device %s, skipping\n", 
                                        subType.c_str(), deviceName.c_str());
                                continue;
                            }
                            args.back().contentList.push_back(deviceName);
                            deviceProtocols[deviceName] = protocol;
                            LogDebug(VB_COMMAND, "Found device %s with protocol %s\n", 
                                     deviceName.c_str(), protocol.c_str());
                        }
                    }
                    if (!args.back().contentList.empty()) {
                        defaultDevice = args.back().contentList.front();
                        args.back().defaultValue = defaultDevice;
                        LogDebug(VB_COMMAND, "Set default device to %s\n", defaultDevice.c_str());
                    } else {
                        LogWarn(VB_COMMAND, "No USBRelay devices found in co-other.json\n");
                    }
                } else {
                    LogWarn(VB_COMMAND, "channelOutputs in co-other.json is not an array\n");
                }
            } else {
                LogWarn(VB_COMMAND, "co-other.json lacks channelOutputs member or is not an object\n");
            }
        } else {
            LogWarn(VB_COMMAND, "co-other.json not found or invalid, no USB relay devices available\n");
        }

        // Channel argument
        args.emplace_back("Channel", "int", "Relay channel (1-8)", false);
        args.back().min = 1;
        args.back().max = 8;
        args.back().defaultValue = "1";

        // State argument
        args.emplace_back("State", "string", "Relay state", false);
        args.back().defaultValue = "ON";
        args.back().contentList = {"ON", "OFF"};

        // Duration argument
        args.emplace_back("Duration", "int", "Duration in minutes to keep relay ON (0 for no auto-off)", true);
        args.back().min = 0;
        args.back().max = 999;
        args.back().defaultValue = "0";

        LogDebug(VB_COMMAND, "USBRelayOnOffCommand initialization complete\n");
    }

private:
    static std::map<std::string, bool> activeTimers;           // Tracks active timer threads
    static std::map<std::string, unsigned char> relayStates;   // Stores ICstation relay bitmasks
    static std::map<std::string, std::string> deviceProtocols; // Cached device -> protocol mapping

    void initializeICStation(const std::string& device) {
        int fd = open(device.c_str(), O_RDWR | O_NOCTTY);
        if (fd < 0) {
            LogErr(VB_COMMAND, "Failed to open %s for initialization\n", device.c_str());
            return;
        }
        unsigned char c_init = 0x50;
        unsigned char c_reply = 0x00;
        unsigned char c_open = 0x51;

        sleep(1); // Initial delay for device readiness
        write(fd, &c_init, 1);
        usleep(500000); // Wait for response
        read(fd, &c_reply, 1);
        LogDebug(VB_COMMAND, "ICStation handshake response: 0x%02X\n", c_reply);
        write(fd, &c_open, 1);
        unsigned char wakeCmd = 0x00;
        write(fd, &wakeCmd, 1);
        usleep(100000); // Final wake-up delay
        close(fd);

        relayStates[device] = 0x00;
        LogDebug(VB_COMMAND, "Initialized ICStation device %s with handshake and wake-up\n", device.c_str());
    }

    unsigned char getRelayBitmask(const std::string& device, int channel, bool turnOn) {
        if (relayStates.find(device) == relayStates.end()) {
            relayStates[device] = 0x00;
            LogDebug(VB_COMMAND, "Initialized relay state for %s to 0x00\n", device.c_str());
        }
        unsigned char bitmask = relayStates[device];
        if (turnOn) {
            bitmask |= (1 << (channel - 1));
        } else {
            bitmask &= ~(1 << (channel - 1));
        }
        relayStates[device] = bitmask;
        return bitmask;
    }

    // Auto-off thread
    void turnOffAfterDelay(const std::string& device, int channel, const std::string& protocol, int durationMinutes) {
        std::string key = device + ":" + std::to_string(channel);
        // Diagnostic: Log thread start
        LogDebug(VB_COMMAND, "Timer thread started for %s, channel %d, waiting %d minutes\n", device.c_str(), channel, durationMinutes);
        std::this_thread::sleep_for(std::chrono::minutes(durationMinutes));
        
        if (activeTimers[key]) {
            int fd = open(device.c_str(), O_WRONLY | O_NOCTTY);
            if (fd < 0) {
                LogErr(VB_COMMAND, "Failed to open %s for auto-off\n", device.c_str());
                return;
            }
            unsigned char cmd[4] = {0}; // Initialize to zero
            ssize_t len;
            if (protocol == "CH340") {
                cmd[0] = 0xA0;
                cmd[1] = (unsigned char)channel;
                cmd[2] = 0x00; // OFF
                cmd[3] = cmd[0] + cmd[1] + cmd[2];
                len = 4;
            } else if (protocol == "ICstation") {
                cmd[0] = getRelayBitmask(device, channel, false);
                len = 1;
            } else {
                close(fd);
                return;
            }
            ssize_t written = write(fd, cmd, len);
            close(fd);
            if (written == len) {
                LogDebug(VB_COMMAND, "Auto-turned off relay: %s, channel %d\n", device.c_str(), channel);
            } else {
                LogErr(VB_COMMAND, "Failed to auto-turn off relay, wrote %zd bytes\n", written);
            }
        } else {
            // Diagnostic: Log if timer was cancelled or key mismatch occurred
            LogDebug(VB_COMMAND, "Timer skipped for %s, channel %d: no active timer\n", device.c_str(), channel);
        }
        activeTimers.erase(key);
    }

public:
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        if (args.size() < 3) return std::make_unique<Command::ErrorResult>("Device, Channel, and State required");
        
        std::string device = args[0];
        int channel;
        try {
            channel = std::stoi(args[1]);
        } catch (const std::exception& e) {
            LogErr(VB_COMMAND, "Invalid channel value: %s\n", args[1].c_str());
            return std::make_unique<Command::ErrorResult>("Invalid channel value: " + args[1]);
        }
        if (channel < 1 || channel > 8) return std::make_unique<Command::ErrorResult>("Channel must be 1-8");
        
        std::string state = args[2];
        if (state != "ON" && state != "OFF") return std::make_unique<Command::ErrorResult>("State must be ON or OFF");

        int duration = 0;
        if (args.size() > 3 && !args[3].empty()) {
            try {
                duration = std::stoi(args[3]);
            } catch (const std::exception& e) {
                LogErr(VB_COMMAND, "Invalid duration value: %s\n", args[3].c_str());
                return std::make_unique<Command::ErrorResult>("Invalid duration value: " + args[3]);
            }
        }
        if (duration < 0) return std::make_unique<Command::ErrorResult>("Duration cannot be negative");

        // Adjust device path to include /dev/ if missing
        std::string fullDevice = device;
        if (fullDevice.find("/dev/") != 0) {
            fullDevice = "/dev/" + device;
            LogDebug(VB_COMMAND, "Adjusted device path from %s to %s\n", device.c_str(), fullDevice.c_str());
        }
        std::string key = fullDevice + ":" + std::to_string(channel); // Use full path for timer key

        auto it = deviceProtocols.find(device);
        if (it == deviceProtocols.end()) {
            LogErr(VB_COMMAND, "Device %s not configured in Others tab\n", device.c_str());
            return std::make_unique<Command::ErrorResult>("Device " + device + " not configured in Others tab");
        }
        std::string protocol = it->second;

        LogDebug(VB_COMMAND, "Opening device: %s, Channel: %d, State: %s, Protocol: %s, Duration: %d minutes\n", 
                 fullDevice.c_str(), channel, state.c_str(), protocol.c_str(), duration);

        if (protocol == "ICstation" && relayStates.find(fullDevice) == relayStates.end()) {
            initializeICStation(fullDevice);
        }

        int fd = open(fullDevice.c_str(), O_WRONLY | O_NOCTTY);
        if (fd < 0) {
            LogErr(VB_COMMAND, "Failed to open %s\n", fullDevice.c_str());
            return std::make_unique<Command::ErrorResult>("Failed to open device " + device);
        }

        unsigned char cmd[4] = {0}; // Initialize to zero
        ssize_t len;
        if (protocol == "CH340") {
            cmd[0] = 0xA0;
            cmd[1] = (unsigned char)channel;
            cmd[2] = (state == "ON") ? 0x01 : 0x00;
            cmd[3] = cmd[0] + cmd[1] + cmd[2];
            len = 4;
        } else if (protocol == "ICstation") {
            cmd[0] = getRelayBitmask(fullDevice, channel, state == "ON");
            len = 1;
        } else {
            close(fd);
            LogErr(VB_COMMAND, "Unsupported protocol %s for device %s\n", protocol.c_str(), device.c_str());
            return std::make_unique<Command::ErrorResult>("Unsupported protocol for device " + device);
        }

        LogDebug(VB_COMMAND, "Sending command to %s: [%02X %02X %02X %02X] (len=%zd)\n", 
                 fullDevice.c_str(), cmd[0], cmd[1], cmd[2], cmd[3], len);

        ssize_t written = write(fd, cmd, len);
        close(fd);

        if (written != len) {
            LogErr(VB_COMMAND, "Write failed, wrote %zd bytes\n", written);
            return std::make_unique<Command::ErrorResult>("Failed to set relay channel " + args[1] + " to " + state);
        }

        LogDebug(VB_COMMAND, "Wrote to relay: %s, channel %d, state %s\n", 
                 fullDevice.c_str(), channel, state.c_str());

        if (state == "ON" && duration > 0) {
            // Check if a timer is already active for this device/channel
            if (activeTimers.find(key) != activeTimers.end() && activeTimers[key]) {
                LogDebug(VB_COMMAND, "Timer already active for %s, channel %d, ignoring new timer request\n", 
                         fullDevice.c_str(), channel);
                return std::make_unique<Command::Result>("Relay channel " + args[1] + " already ON with active timer");
            }
            activeTimers[key] = true;
            // Diagnostic: Confirm timer thread is spawned
            LogDebug(VB_COMMAND, "Spawning timer thread for %s, channel %d, duration %d minutes\n", 
                     fullDevice.c_str(), channel, duration);
            std::thread([this, fullDevice, channel, protocol, duration]() { turnOffAfterDelay(fullDevice, channel, protocol, duration); }).detach();
            return std::make_unique<Command::Result>("Relay channel " + args[1] + " turned ON for " + std::to_string(duration) + " minutes");
        } else if (state == "OFF") {
            activeTimers[key] = false;
        }

        return std::make_unique<Command::Result>("Relay channel " + args[1] + " turned " + state);
    }
};

// Static Member Definitions
std::map<std::string, bool> USBRelayOnOffCommand::activeTimers;
std::map<std::string, unsigned char> USBRelayOnOffCommand::relayStates;
std::map<std::string, std::string> USBRelayOnOffCommand::deviceProtocols;

// Command Manager Implementation
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
    addCommand(new InsertRandomItemFromPlaylistCommand());
#ifdef HAS_VLC
    addCommand(new PlayMediaCommand());
    addCommand(new StopMediaCommand());
    addCommand(new StopAllMediaCommand());
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

    addCommand(new TriggerRemotePresetCommand());
    addCommand(new TriggerRemotePresetSlotCommand());
    addCommand(new StartRemoteEffectCommand());
    addCommand(new StopRemoteEffectCommand());
    addCommand(new RunRemoteScriptEvent());
    addCommand(new StartRemoteFSEQEffectCommand());
    addCommand(new StartRemotePlaylistCommand());

    // Add our custom USB Relay command
    addCommand(new USBRelayOnOffCommand());

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
    while (!commands.empty()) {
        Command* cmd = commands.begin()->second;
        commands.erase(commands.begin());

        if (cmd->name != "GPIO") { // No idea why deleting the GPIO command causes a crash on exit
            delete cmd;
        }
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

HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> CommandManager::render_GET(const httpserver::http_request& req) {
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
        std::string commandsFile = FPP_DIR_CONFIG("/commandPresets.json");
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
            if (std::string(req.get_arg("names")) == "true") {
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

HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> CommandManager::render_POST(const httpserver::http_request& req) {
    std::string p1 = req.get_path_pieces()[0];
    if (p1 == "command") {
        if (req.get_path_pieces().size() > 1) {
            std::string command = req.get_path_pieces()[1];
            Json::Value val = LoadJsonFromString(std::string(req.get_content()));
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
            std::string command(req.get_content());
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

    std::string commandsFile = FPP_DIR_CONFIG("/commandPresets.json");

    Json::Value allCommands;

    if (FileExists(commandsFile)) {
        // Load new config file
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