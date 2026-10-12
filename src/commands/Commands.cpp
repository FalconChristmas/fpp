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

#include "fpp-json.h"
#include "fpphttp.h" // drogon/HTTP helpers used here; no longer pulled transitively (see fpphttp_types.h)
#include "fpphttp_clientip.h" // getEffectiveClientIP() - FPP-internal, not in the plugin API

#include <algorithm>
#include <thread>

#include "../Variables.h"
#include "../common.h"
#include "../log.h"
#include "../Warnings.h"

#include "../Events.h"

#include "EventCommands.h"
#include "FileMonitor.h"
#include "IfCommand.h"
#include "MediaCommands.h"
#include "MultiSync.h"
#include "PlaylistCommands.h"
#include "VariableCommands.h"

// %VAR:name% - substitute a named User Variable's current value. Applied
// once, centrally, inside CommandManager::run() (the one chokepoint every
// caller funnels through - api/command, presets, GPIO, MultiSync, playlist
// "FPP Command" entries, If, recurring tasks, ...) rather than at each
// individual call site, so it works the same way no matter how a command
// gets run. Defined further down (needs Variables.h); forward-declared here
// since run() is used well above that point in the file.
static std::string ReplaceVariableKeywords(std::string str);

CommandManager CommandManager::INSTANCE;

// Out-of-line storage for anything added to Command/CommandArg after plugin API
// version 5. Both hold it by unique_ptr, so growing either struct here is
// invisible to a plugin's sizeof and cannot desync a std::list node or shift a
// subclass's members. See the LAYOUT RULE comments in Commands.h.
struct Command::Data {
    // (empty - first field added after API version 5 goes here)
};
struct Command::CommandArg::Ext {
    bool advanced = false;
    std::string help;
    std::string section;
    std::map<std::string, std::vector<std::string>> children;
    bool toggleStyle = false;
    std::string toggleLabel;
};

Command::Command(const std::string& n) :
    name(n),
    description("") {
}
Command::Command(const std::string& n, const std::string& descript) :
    name(n),
    description(descript) {
}
Command::~Command() {}

Command::CommandArg::CommandArg(const std::string& n, const std::string& t, const std::string& d, bool o) :
    name(n),
    type(t),
    description(d),
    optional(o),
    min(-1),
    max(-1),
    allowBlanks(false),
    adjustable(false) {
}
Command::CommandArg::CommandArg(const CommandArg& o) :
    name(o.name),
    type(o.type),
    description(o.description),
    optional(o.optional),
    contentListUrl(o.contentListUrl),
    contentList(o.contentList),
    allowBlanks(o.allowBlanks),
    min(o.min),
    max(o.max),
    defaultValue(o.defaultValue),
    adjustableGetValueURL(o.adjustableGetValueURL),
    adjustable(o.adjustable),
    // Deep copy, not a shared handle: the builder pattern returns CommandArg&,
    // so nearly every args.push_back() in the tree copies from an lvalue and
    // the temporary is then destroyed. Aliasing ext would leave a dangling one.
    ext(o.ext ? std::make_unique<Ext>(*o.ext) : nullptr) {
}
Command::CommandArg::CommandArg(CommandArg&& o) noexcept = default;
Command::CommandArg::~CommandArg() = default;

// Ext is allocated on first write; readers fall back to a default so an arg that
// set none of these still costs nothing. Every accessor pair below follows this
// shape - copy it when adding a field.
Command::CommandArg::Ext& Command::CommandArg::mutableExt() {
    if (!ext) {
        ext = std::make_unique<Ext>();
    }
    return *ext;
}

Command::CommandArg& Command::CommandArg::setSection(const std::string& s) {
    mutableExt().section = s;
    return *this;
}
const std::string& Command::CommandArg::getSection() const {
    static const std::string EMPTY;
    return ext ? ext->section : EMPTY;
}

Command::CommandArg& Command::CommandArg::setHelp(const std::string& h) {
    mutableExt().help = h;
    return *this;
}
const std::string& Command::CommandArg::getHelp() const {
    static const std::string EMPTY;
    return ext ? ext->help : EMPTY;
}

Command::CommandArg& Command::CommandArg::setAdvanced(bool a) {
    mutableExt().advanced = a;
    return *this;
}
bool Command::CommandArg::isAdvanced() const {
    return ext && ext->advanced;
}

Command::CommandArg& Command::CommandArg::setChildren(std::map<std::string, std::vector<std::string>> c) {
    mutableExt().children = std::move(c);
    return *this;
}
const std::map<std::string, std::vector<std::string>>& Command::CommandArg::getChildren() const {
    static const std::map<std::string, std::vector<std::string>> EMPTY;
    return ext ? ext->children : EMPTY;
}

Command::CommandArg& Command::CommandArg::setToggleStyle(bool t) {
    mutableExt().toggleStyle = t;
    return *this;
}
bool Command::CommandArg::isToggleStyle() const {
    return ext && ext->toggleStyle;
}

Command::CommandArg& Command::CommandArg::setToggleLabel(const std::string& l) {
    mutableExt().toggleLabel = l;
    return *this;
}
const std::string& Command::CommandArg::getToggleLabel() const {
    static const std::string EMPTY;
    return ext ? ext->toggleLabel : EMPTY;
}

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
        if (ar.isAdvanced()) {
            a["advanced"] = true;
        }
        if (!ar.getHelp().empty()) {
            a["help"] = ar.getHelp();
        }
        if (!ar.getSection().empty()) {
            a["section"] = ar.getSection();
        }
        if (ar.isToggleStyle()) {
            a["toggleStyle"] = true;
        }
        if (!ar.getToggleLabel().empty()) {
            a["toggleLabel"] = ar.getToggleLabel();
        }
        if (!ar.getChildren().empty()) {
            for (auto& kv : ar.getChildren()) {
                for (auto& childName : kv.second) {
                    a["children"][kv.first].append(childName);
                }
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

    addCategorizedCommand(new StopPlaylistCommand(), "Playlist", 0);
    addCategorizedCommand(new StopGracefullyPlaylistCommand(), "Playlist", 0);
    addCategorizedCommand(new RestartPlaylistCommand(), "Playlist", 1);
    addCategorizedCommand(new NextPlaylistCommand(), "Playlist", 0);
    addCategorizedCommand(new PrevPlaylistCommand(), "Playlist", 0);
    addCategorizedCommand(new StartPlaylistCommand(), "Playlist", 0);
    addCategorizedCommand(new TogglePlaylistCommand(), "Playlist", 0);
    addCategorizedCommand(new StartPlaylistAtCommand(), "Playlist", 1);
    addCategorizedCommand(new StartPlaylistAtRandomCommand(), "Playlist", 1);
    addCategorizedCommand(new InsertPlaylistCommand(), "Playlist", 0);
    addCategorizedCommand(new InsertPlaylistImmediate(), "Playlist", 0);
    addCategorizedCommand(new InsertRandomItemFromPlaylistCommand(), "Playlist", 1);
#ifdef HAS_GSTREAMER
    addCategorizedCommand(new PlayMediaCommand(), "Media", 0);
    addCategorizedCommand(new StopMediaCommand(), "Media", 0);
    addCategorizedCommand(new StopAllMediaCommand(), "Media", 0);
    addCategorizedCommand(new StopMediaSlotCommand(), "Media", 1);
    addCategorizedCommand(new SetSlotVolumeCommand(), "Audio", 1);
    addCategorizedCommand(new MediaSlotStatusCommand(), "Media", 1);
#endif
#ifdef HAS_AES67_GSTREAMER
    addCategorizedCommand(new AES67ApplyCommand(), "Audio", 3);
    addCategorizedCommand(new AES67CleanupCommand(), "Audio", 3);
    addCategorizedCommand(new AES67TestCommand(), "Audio", 3);
    addCategorizedCommand(new ApplyRoutingPresetCommand(), "Audio", 3);
#endif
#ifdef HAS_OPUS_RTP_GSTREAMER
    addCategorizedCommand(new OpusRTPApplyCommand(), "Audio", 3);
    addCategorizedCommand(new OpusRTPCleanupCommand(), "Audio", 3);
#endif
    addCategorizedCommand(new PlaylistPauseCommand(), "Playlist", 0);
    addCategorizedCommand(new PlaylistResumeCommand(), "Playlist", 0);
    addCategorizedCommand(new SetPlaylistRepeatCommand(), "Playlist", 0);
    addCategorizedCommand(new TriggerPresetCommand(), "Events", 0);
    addCategorizedCommand(new TriggerPresetInFutureCommand(), "Events", 1);
    addCategorizedCommand(new TriggerPresetSlotCommand(), "Events", 1);
    addCategorizedCommand(new TriggerMultiplePresetsCommand(), "Events", 1);
    addCategorizedCommand(new TriggerMultiplePresetSlotsCommand(), "Events", 1);
    addCategorizedCommand(new RunScriptEvent(), "Events", 1);
    addCategorizedCommand(new StartEffectCommand(), "Effects", 0);
    addCategorizedCommand(new StartFSEQAsEffectCommand(), "Effects", 0);
    addCategorizedCommand(new StopFSEQAsEffectCommand(), "Effects", 0);
    addCategorizedCommand(new StopEffectCommand(), "Effects", 0);
    addCategorizedCommand(new StopAllEffectsCommand(), "Effects", 0);
    addCategorizedCommand(new SetVolumeCommand(), "Audio", 0);
    addCategorizedCommand(new AdjustVolumeCommand(), "Audio", 0);
    addCategorizedCommand(new IncreaseVolumeCommand(), "Audio", 0);
    addCategorizedCommand(new DecreaseVolumeCommand(), "Audio", 0);
    addCategorizedCommand(new URLCommand(), "Events", 1);
    addCategorizedCommand(new AllLightsOffCommand(), "Effects", 0);
    addCategorizedCommand(new SwitchToPlayerModeCommand(), "System", 1);
    addCategorizedCommand(new SwitchToRemoteModeCommand(), "System", 1);
    addCategorizedCommand(new RestartFPPDCommand(), "System", 1);
    addCategorizedCommand(new RebootCommand(), "System", 1);
    addCategorizedCommand(new ShutdownCommand(), "System", 1);
    addCategorizedCommand(new SetVariableCommand(), "Events", 1);
    addCategorizedCommand(new IfCommand(), "Events", 1);

    std::function<void(const std::string&, const std::string&)> f =
        [](const std::string& topic, const std::string& payload) {
            if (topic.size() <= 13) {
                Json::Value val;
                bool success = LoadJsonFromString(payload, val);
                if (success && val.isObject()) {
                    LogDebug(VB_COMMAND, "MQTT topic \"%s\" running command (JSON payload)\n", topic.c_str());
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
                        LogDebug(VB_COMMAND, "MQTT topic \"%s\" running command \"%s\" (JSON payload)\n", topic.c_str(), command.c_str());
                        CommandManager::INSTANCE.run(command, val);
                    } else {
                        LogWarn(VB_COMMAND, "Invalid JSON Payload for topic %s: %s\n", topic.c_str(), payload.c_str());
                    }
                } else {
                    LogDebug(VB_COMMAND, "MQTT topic \"%s\" running command \"%s\"\n", topic.c_str(), command.c_str());
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
    std::map<std::string, Command*> all;
    {
        std::unique_lock<std::shared_mutex> l(commandsLock);
        all.swap(commands);
        commandMeta.clear();
        ++commandsGeneration;
    }
    for (auto& c : all) {
        // An MQTT or HTTP thread can still be inside one at shutdown.
        waitForIdle(c.second);
        delete c.second;
    }
}

void CommandManager::addCommand(Command* cmd) {
    // Plugin-facing entry point. Kept as a distinct, unchanged symbol (name and
    // signature) so no plugin needs to be rebuilt. Anything registered through
    // here is by definition not one of FPP's own categorized core commands, so
    // it's tagged "Plugins" / level 0 (visible in Basic UI, per user decision:
    // don't hide something a user deliberately installed).
    addCategorizedCommand(cmd, "Plugins", 0);
}
void CommandManager::addCategorizedCommand(Command* cmd, const std::string& category, int level) {
    std::unique_lock<std::shared_mutex> l(commandsLock);
    auto a = commands.find(cmd->name);
    if (a != commands.end() && a->second != cmd) {
        LogWarn(VB_COMMAND, "Command \"%s\" registered twice; the later registration replaces the earlier\n", cmd->name.c_str());
    }
    commands[cmd->name] = cmd;
    commandMeta[cmd->name] = { category, level };
    ++commandsGeneration;
}

// Commands this thread is running, innermost last. waitForIdle() discounts
// them, so a command that unregisters itself from its own run() does not wait
// on itself.
static thread_local std::vector<Command*> tl_runningCommands;

std::shared_ptr<Command> CommandManager::acquireCommand(const std::string& name) {
    std::shared_lock<std::shared_mutex> l(commandsLock);
    auto f = commands.find(name);
    if (f == commands.end()) {
        return nullptr;
    }
    Command* cmd = f->second;
    {
        std::lock_guard<std::mutex> il(inFlightLock);
        inFlight[cmd]++;
    }
    // Releases the pin, never deletes: the command's owner does that.
    return std::shared_ptr<Command>(cmd, [this](Command* c) {
        std::lock_guard<std::mutex> il(inFlightLock);
        auto it = inFlight.find(c);
        if (it != inFlight.end() && --it->second <= 0) {
            inFlight.erase(it);
        }
        inFlightCV.notify_all();
    });
}

// Every caller of removeCommand() deletes the command straight after, so it
// must not return while another thread is still inside that command's run().
// Bounded, because a run that never ends (one blocked on the thread doing the
// removal) would otherwise hang the removal for good; that case is logged.
void CommandManager::waitForIdle(Command* cmd) {
    const int own = (int)std::count(tl_runningCommands.begin(), tl_runningCommands.end(), cmd);
    std::unique_lock<std::mutex> il(inFlightLock);
    bool idle = inFlightCV.wait_for(il, std::chrono::seconds(10), [&] {
        auto it = inFlight.find(cmd);
        return it == inFlight.end() || it->second <= own;
    });
    if (!idle) {
        LogErr(VB_COMMAND, "Command \"%s\" is still running 10s after it was unregistered; continuing anyway\n", cmd->name.c_str());
    }
}

void CommandManager::removeCommand(Command* cmd) {
    {
        std::unique_lock<std::shared_mutex> l(commandsLock);
        auto a = commands.find(cmd->name);
        if (a == commands.end() || a->second != cmd) {
            // Another object holds this name now (two outputs registering the
            // same command); removing by name would unregister theirs.
            return;
        }
        commands.erase(a);
        commandMeta.erase(cmd->name);
        ++commandsGeneration;
    }
    waitForIdle(cmd);
}
void CommandManager::removeCommand(const std::string& cmdName) {
    Command* cmd = nullptr;
    {
        std::unique_lock<std::shared_mutex> l(commandsLock);
        auto a = commands.find(cmdName);
        if (a != commands.end()) {
            cmd = a->second;
            commands.erase(a);
        }
        commandMeta.erase(cmdName);
        ++commandsGeneration;
    }
    if (cmd) {
        waitForIdle(cmd);
    }
}
std::vector<std::pair<std::string, Command*>> CommandManager::getRegisteredCommands() const {
    std::shared_lock<std::shared_mutex> l(commandsLock);
    std::vector<std::pair<std::string, Command*>> ret;
    ret.reserve(commands.size());
    for (const auto& c : commands) {
        ret.emplace_back(c.first, c.second);
    }
    return ret;
}
// Caller holds commandsLock (shared).
Json::Value CommandManager::describeCommand(Command* cmd) {
    Json::Value result = cmd->getDescription();
    auto m = commandMeta.find(cmd->name);
    if (m != commandMeta.end()) {
        result["category"] = m->second.category;
        result["level"] = m->second.level;
    }
    return result;
}
Json::Value CommandManager::getDescriptions() {
    Json::Value ret;
    std::shared_lock<std::shared_mutex> l(commandsLock);
    for (auto& a : commands) {
        // Isolate each command's description: a single command that throws while
        // being described (e.g. a stale external plugin whose Command/CommandArg
        // layout no longer matches these headers, so getDescription() reads a
        // garbage std::string and jsoncpp rejects its length) must not take out
        // the whole /commands endpoint - and the UI that depends on it - for
        // every other command. Log and skip the offender instead. The map key
        // (a.first) is FPP's own string, safe to read even when the command
        // object behind it is not. (This only catches C++ exceptions; a hard
        // memory fault in a plugin is a separate failure the vtable/ABI rules
        // in Commands.h exist to prevent.)
        try {
            if (!a.second->hidden()) {
                ret.append(describeCommand(a.second));
            }
        } catch (const std::exception& e) {
            LogWarn(VB_COMMAND, "Skipping command \"%s\" in /commands: %s\n",
                    a.first.c_str(), e.what());
        } catch (...) {
            LogWarn(VB_COMMAND, "Skipping command \"%s\" in /commands: unknown exception\n",
                    a.first.c_str());
        }
    }
    return ret;
}

// Flattens a command's args into the "a,b,c" form used by the log lines below,
// so every path a command can arrive on reports it the same way: as
// "command(args)".  Args are unbounded (one could carry a Variable's full
// value), so each is truncated before logging - see TruncateForLog().
//
// Deliberately NOT gated behind WillLog() at the call sites.  Every caller logs
// at Info, and _LogWrite() formats any line at Debug or below regardless of the
// configured level so the crash ring can retain it (see CrashLogRingWillCapture)
// - so a guard would not skip the formatting, it would only drop the line out of
// crash dumps.  Guarding is a pure win only at LOG_EXCESSIVE, which the ring
// never captures; that is the one level FPP's other WillLog() call sites use.
static std::string FormatArgsForLog(const std::vector<std::string>& args) {
    std::string argString;
    for (auto& a : args) {
        if (!argString.empty()) {
            argString += ",";
        }
        argString += TruncateForLog(a);
    }
    return argString;
}

// The one place a Command is actually invoked, so the one place its argument
// contract can be enforced.
//
// A command that declares arguments reads args[0] to get its subject - the
// playlist name, the media file, the volume. Most check that it is there
// first; several index it bare, and operator[](0) on an empty vector hands
// run() a reference to nothing. That is not a theoretical hazard: a bare
// "/api/command/Play Media" (no arguments at all) walked a garbage
// std::string down to GStreamerOutput's constructor and segfaulted there,
// and "Insert Playlist Immediate" did the same through Playlist::Play - the
// POST form reaching zero args because a form-urlencoded body fails to parse
// as JSON and leaves an empty array behind.
//
// Enforcing it here rather than in each run() covers every entry path at once
// - GET, POST, MQTT, presets, GPIO, playlist entries, MultiSync - and covers
// plugin commands, which FPP cannot audit. A command that genuinely works
// with no arguments says so by declaring its first argument optional (or by
// declaring none).
static std::unique_ptr<Command::Result> invokeCommand(Command* cmd, const std::vector<std::string>& args) {
    if (args.empty() && !cmd->args.empty() && !cmd->args.front().optional) {
        const std::string& argName = cmd->args.front().name;
        LogWarn(VB_COMMAND, "Command \"%s\" requires the \"%s\" argument, none given - not running it\n",
                cmd->name.c_str(), argName.c_str());
        return std::make_unique<Command::ErrorResult>(
            "Command \"" + cmd->name + "\" requires the \"" + argName + "\" argument");
    }
    // ...and the one place every nested command passes through, so the one
    // place runaway nesting can be bounded. Commands run commands: a preset, an
    // If's Then/Else list, a playlist entry that restarts its own playlist, a
    // %VAR: whose value is an If that expands the same variable again. Each
    // path that recurses into itself on the same stack has its own fix, but
    // anything missed - including a plugin command - would otherwise grow the
    // stack until it faults. Real chains are a handful deep.
    static constexpr int MAX_COMMAND_DEPTH = 32;
    static thread_local int depth = 0;
    if (depth >= MAX_COMMAND_DEPTH) {
        LogErr(VB_COMMAND, "Command \"%s\" not run: commands nested more than %d deep - a command is running itself\n",
               cmd->name.c_str(), MAX_COMMAND_DEPTH);
        return std::make_unique<Command::ErrorResult>("Command \"" + cmd->name + "\" nested too deep");
    }
    struct DepthGuard {
        Command* c;
        explicit DepthGuard(Command* cmd) :
            c(cmd) {
            depth++;
            tl_runningCommands.push_back(c);
        }
        ~DepthGuard() {
            depth--;
            tl_runningCommands.pop_back();
        }
    } depthGuard(cmd);

    // ...and the one place a throwing command can be stopped from taking fppd
    // down. Only the HTTP routes have a catch above them (drogon's); MQTT runs
    // commands on libmosquitto's thread, MultiSync, GPIO, presets and playlist
    // entries on the main loop, and none of those frames catch, so an exception
    // out of run() - jsoncpp's LogicError on an argument of the wrong JSON type,
    // std::stoi on a non-number, a Magick error, any plugin's own throw - was
    // std::terminate. A malformed command is the sender's error; report it as
    // one.
    try {
        return cmd->run(args);
    } catch (const std::exception& e) {
        LogErr(VB_COMMAND, "Command \"%s(%s)\" failed: %s\n", cmd->name.c_str(), FormatArgsForLog(args).c_str(), e.what());
        return std::make_unique<Command::ErrorResult>("Command \"" + cmd->name + "\" failed: " + e.what());
    } catch (...) {
        LogErr(VB_COMMAND, "Command \"%s(%s)\" failed with an unknown exception\n", cmd->name.c_str(), FormatArgsForLog(args).c_str());
        return std::make_unique<Command::ErrorResult>("Command \"" + cmd->name + "\" failed");
    }
}

// A command's args as strings, whatever JSON type each one arrived as. Every
// documented sender writes strings, but these come off MQTT payloads, MultiSync
// packets and hand-edited presets, and asString() throws on an object or array
// - and indexing a non-array by position throws too. Scalars convert the way
// asString() always has; an object or array is passed on as its JSON text,
// which is what the commands that take structured args (If) parse anyway.
static bool JsonArgsToStrings(const Json::Value& argsArray, std::vector<std::string>& args, std::string& err) {
    if (argsArray.isNull()) {
        return true;
    }
    if (!argsArray.isArray()) {
        err = "\"args\" must be an array";
        return false;
    }
    for (Json::ArrayIndex x = 0; x < argsArray.size(); x++) {
        const Json::Value& a = argsArray[x];
        if (a.isNull()) {
            args.push_back("");
        } else if (a.isObject() || a.isArray()) {
            args.push_back(SaveJsonToString(a));
        } else {
            args.push_back(a.asString());
        }
    }
    return true;
}

// A boolean flag in a command's JSON, tolerating the "true"/"1" string forms a
// hand-written payload uses; asBool() throws on a string.
static bool JsonFlag(const Json::Value& v) {
    if (v.isBool()) {
        return v.asBool();
    }
    if (v.isIntegral()) {
        return v.asLargestInt() != 0;
    }
    if (v.isString()) {
        std::string s = v.asString();
        return s == "true" || s == "1";
    }
    return false;
}

std::unique_ptr<Command::Result> CommandManager::run(const std::string& command, const std::vector<std::string>& args) {
    // Pinned: removeCommand() waits for it, so it cannot be freed mid-run.
    std::shared_ptr<Command> pinned = acquireCommand(command);
    if (pinned) {
        std::vector<std::string> resolvedArgs;
        resolvedArgs.reserve(args.size());
        for (auto const& arg : args) {
            resolvedArgs.push_back(ReplaceVariableKeywords(arg));
        }

        // Logged after substitution (like the Json overload below) so the line
        // shows what actually ran, not the unresolved %VAR:name% that was
        // passed in.
        LogInfo(VB_COMMAND, "Running command \"%s(%s)\"\n", command.c_str(), FormatArgsForLog(resolvedArgs).c_str());

        // Publish MQTT event for command execution
        Json::Value payload;
        payload["command"] = command;
        payload["args"] = Json::Value(Json::arrayValue);
        for (const auto& arg : resolvedArgs) {
            payload["args"].append(arg);
        }
        payload["trigger"] = "internal";
        std::string topic = "command/run";
        Events::Publish(topic, SaveJsonToString(payload));

        return invokeCommand(pinned.get(), resolvedArgs);
    }
    LogWarn(VB_COMMAND, "No command found for \"%s\"\n", command.c_str());
    // A playlist "FPP Command" entry (or preset, GPIO binding, etc.) can
    // reference a command that a plugin used to provide but no longer does
    // (uninstalled/updated) - the failure above is otherwise silent: the
    // caller just gets an ErrorResult and, for a playlist entry, that item is
    // marked finished exactly like a successful one (PlaylistEntryCommand::
    // StartPlaying() -> FinishPlay(), no distinct failure state), so a show
    // can silently skip this step forever with nothing but a log line to
    // show for it. Surface it as a real warning instead.
    WarningHolder::AddWarningTimeout(900, 61, "No command found for \"" + command + "\" - it may have come from a plugin that is no longer installed or was updated");
    return std::make_unique<Command::ErrorResult>("No Command: " + command);
}

std::unique_ptr<Command::Result> CommandManager::run(const std::string& command, const Json::Value& argsArray) {
    // Pinned: removeCommand() waits for it, so it cannot be freed mid-run.
    std::shared_ptr<Command> pinned = acquireCommand(command);
    if (pinned) {
        std::vector<std::string> args;
        std::string err;
        if (!JsonArgsToStrings(argsArray, args, err)) {
            LogWarn(VB_COMMAND, "Command \"%s\" not run: %s\n", command.c_str(), err.c_str());
            return std::make_unique<Command::ErrorResult>(err);
        }
        for (auto& a : args) {
            a = ReplaceVariableKeywords(a);
        }
        LogInfo(VB_COMMAND, "Running command \"%s(%s)\"\n", command.c_str(), FormatArgsForLog(args).c_str());

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
        LogExcess(VB_COMMAND, "JSONVAL MQTT Publishing command: %s, payload: %s\n", topic.c_str(), TruncateForLog(payloadStr, 2000).c_str());
        Events::Publish(topic, payloadStr);

        return invokeCommand(pinned.get(), args);
    }
    LogWarn(VB_COMMAND, "No command found for \"%s\"\n", command.c_str());
    // A playlist "FPP Command" entry (or preset, GPIO binding, etc.) can
    // reference a command that a plugin used to provide but no longer does
    // (uninstalled/updated) - the failure above is otherwise silent: the
    // caller just gets an ErrorResult and, for a playlist entry, that item is
    // marked finished exactly like a successful one (PlaylistEntryCommand::
    // StartPlaying() -> FinishPlay(), no distinct failure state), so a show
    // can silently skip this step forever with nothing but a log line to
    // show for it. Surface it as a real warning instead.
    WarningHolder::AddWarningTimeout(900, 61, "No command found for \"" + command + "\" - it may have come from a plugin that is no longer installed or was updated");
    return std::make_unique<Command::ErrorResult>("No Command: " + command);
}
std::unique_ptr<Command::Result> CommandManager::run(const Json::Value& cmd) {
    if (!cmd.isObject() || !cmd["command"].isString()) {
        LogWarn(VB_COMMAND, "Command not run: \"command\" must be a string\n");
        return std::make_unique<Command::ErrorResult>("\"command\" must be a string");
    }
    std::string command = cmd["command"].asString();
    if (cmd.isMember("multisyncCommand")) {
        bool multisync = JsonFlag(cmd["multisyncCommand"]);
        if (multisync) {
            std::string hosts = cmd["multisyncHosts"].isString() ? cmd["multisyncHosts"].asString() : "";
            std::vector<std::string> args;
            std::string err;
            if (!JsonArgsToStrings(cmd["args"], args, err)) {
                LogWarn(VB_COMMAND, "Command \"%s\" not sent: %s\n", command.c_str(), err.c_str());
                return std::make_unique<Command::ErrorResult>(err);
            }
            // Resolved against THIS host's Variables before sending - a remote
            // host has no way to resolve %VAR:name% itself (and may not even
            // have a same-named Variable), so it must go out already
            // substituted, unlike the non-multisync path below where run()
            // does this.
            for (auto& a : args) {
                a = ReplaceVariableKeywords(a);
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
 * List all available commands and their argument descriptions. Each entry
 * also carries "category" (e.g. "Playlist", "Media", "Plugins") and "level"
 * (0 Basic / 1 Advanced / 3 Developer) for UI grouping and filtering, plus
 * "disallowMultisync" (true) on a command whose Multisync option should stay
 * hidden - e.g. "If", since multisyncing it would broadcast the raw check to
 * other instances rather than propagate the result of evaluating it.
 *
 * @route GET /api/commands
 * @response 200 Array of command descriptions.
 */

/**
 * Get the description of a single command by name. Includes the same
 * "category"/"level" fields as the list route.
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
 * An argument may contain `%VAR:name%` to substitute a User Variable's current
 * value, as anywhere else a command's arguments are given. Percent-encode it as
 * `%25VAR:name%25`: sent raw, `%VA` is an invalid escape and Apache rejects the
 * request with a 400 before it reaches FPP. An unset variable substitutes as an
 * empty string.
 *
 * Note that empty path segments collapse, so an argument that should be empty
 * cannot be passed this way - use the POST form below for that.
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
            std::shared_lock<std::shared_mutex> cl(commandsLock);
            auto f = commands.find(command);
            if (f != commands.end()) {
                Json::Value result = describeCommand(f->second);
                std::string resultStr = SaveJsonToString(result, "  ");
                return makeStringResponse(resultStr, 200, "application/json");
            }
        } else {
            // The full list is ~33KB and every page that can edit a command
            // fetches it twice on load (PopulateCommandListCache and
            // captureCommandListJSON), plus once more after an fppd restart.
            // Its content is fixed until something registers or unregisters a
            // command, so the generation counter answers a conditional request
            // without describing all of them again.
            return makeVersionedETagResponse(req, std::to_string(commandsGeneration.load()), [this]() {
                Json::Value result = getDescriptions();
                return SaveJsonToString(result, "  ");
            });
        }
    } else if (p1 == "commandPresets") {
        std::string commandsFile = FPP_DIR_CONFIG("/commandPresets.json");
        Json::Value allCommands;
        if (FileExists(commandsFile)) {
            // Load new config file
            allCommands = LoadJsonFromFile(commandsFile, JsonRoot::Object);
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
            // Substituted here rather than inherited from CommandManager::run():
            // this route calls the Command directly (see the MQTT publish below
            // for why), so without this %VAR:name% would pass through literally
            // on /api/command/... alone while resolving on every other path.
            args.push_back(ReplaceVariableKeywords(parts[x]));
        }
        // Pinned: removeCommand() waits for it, so it cannot be freed mid-run.
        std::shared_ptr<Command> pinned = acquireCommand(command);
        if (pinned) {
            // This route calls the Command directly rather than through
            // CommandManager::run(), so it must log the args itself - see the
            // MQTT publish just below for why it can't simply delegate.
            LogInfo(VB_COMMAND, "GET /api/command/%s from %s: running command \"%s(%s)\"\n",
                    command.c_str(), getEffectiveClientIP(req).c_str(),
                    command.c_str(), FormatArgsForLog(args).c_str());

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
            LogExcess(VB_COMMAND, "GET MQTT Publishing command: %s, payload: %s\n", topic.c_str(), TruncateForLog(payloadStr, 2000).c_str());
            Events::Publish(topic, payloadStr);

            std::unique_ptr<Command::Result> r = invokeCommand(pinned.get(), args);
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
        WarningHolder::AddWarningTimeout(900, 61, "No command found for \"" + command + "\" - it may have come from a plugin that is no longer installed or was updated");
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
 * An argument may contain `%VAR:name%` to substitute a User Variable's current
 * value, as anywhere else a command's arguments are given. No encoding is
 * needed here, unlike the GET form above. An unset variable substitutes as an
 * empty string.
 *
 * @route POST /api/command/{command}
 * @body ["arg1", "%VAR:brightness%"]
 * @response 200 Command result.
 * @response 500 The command errored or timed out.
 */
HttpResponsePtr CommandManager::render_POST(const HttpRequestPtr& req) {
    auto parts = getPathPieces(req->path());
    std::string p1 = parts[0];
    if (p1 == "command") {
        if (parts.size() > 1) {
            std::string command = parts[1];
            std::string content(getRequestContent(req));
            Json::Value val;
            // A body that does not parse used to yield an empty Json::Value and
            // the command then ran with zero arguments, which is how a
            // form-urlencoded POST ("name=...&startItem=0&...") reached
            // "Insert Playlist Immediate" with nothing to insert. Tell the
            // caller their body was wrong rather than running something else.
            if (!content.empty() && !LoadJsonFromString(content, val)) {
                return makeStringResponse("Body is not valid JSON - expected an array of arguments", 400, "text/plain");
            }
            std::vector<std::string> args;
            std::string err;
            if (!JsonArgsToStrings(val, args, err)) {
                return makeStringResponse("Body must be a JSON array of arguments", 400, "text/plain");
            }
            for (auto& a : args) {
                // Direct Command call, so substitute here - same reason as the
                // GET route above.
                a = ReplaceVariableKeywords(a);
            }
            // Pinned: removeCommand() waits for it, so it cannot be freed mid-run.
            std::shared_ptr<Command> pinned = acquireCommand(command);
            if (pinned) {
                // Direct Command call, not CommandManager::run() - log the
                // args here too, same as the GET route above.
                LogInfo(VB_COMMAND, "POST /api/command/%s from %s: running command \"%s(%s)\"\n",
                        command.c_str(), getEffectiveClientIP(req).c_str(),
                        command.c_str(), FormatArgsForLog(args).c_str());

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
                LogExcess(VB_COMMAND, "POST MQTT Publishing command: %s, payload: %s\n", topic.c_str(), TruncateForLog(payloadStr, 2000).c_str());
                Events::Publish(topic, payloadStr);

                std::unique_ptr<Command::Result> r = invokeCommand(pinned.get(), args);
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
            WarningHolder::AddWarningTimeout(900, 61, "No command found for \"" + command + "\" - it may have come from a plugin that is no longer installed or was updated");
        } else {
            std::string command(getRequestContent(req));
            LogExcess(VB_COMMAND, "Received command: \"%s\"\n", TruncateForLog(command, 2000).c_str());
            Json::Value val = LoadJsonFromString(command);
            if (!val.isObject() || !val["command"].isString()) {
                return makeStringResponse("Body must be a JSON object with a \"command\" string", 400, "text/plain");
            }
            LogDebug(VB_COMMAND, "POST /api/command (bare JSON body) from %s: running command \"%s\"\n",
                     getEffectiveClientIP(req).c_str(), val["command"].asString().c_str());
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

// %VAR:name% - substitute a named User Variable's current value. Explicit
// "VAR:" prefix (rather than bare %name%) avoids colliding with unrelated
// existing usages of %word% patterns that happen to match a variable name.
// Applied centrally in CommandManager::run() (see the forward declaration
// near the top of this file) rather than at each call site, so every way a
// command can run - api/command, a preset trigger, GPIO, MultiSync, a
// playlist "FPP Command" entry, If, a recurring task, ... - resolves it the
// same way. Kept out of the shared ReplaceKeywords() in common.cpp so the
// Variables dependency doesn't spread into the small standalone CLI helpers
// that link common.o directly (fpp, fppoled, fsequtils).
static std::string ReplaceVariableKeywords(std::string str) {
    std::size_t varPos = 0;
    while ((varPos = str.find("%VAR:", varPos)) != std::string::npos) {
        std::size_t varEnd = str.find('%', varPos + 5);
        if (varEnd == std::string::npos) {
            break;
        }
        std::string varName = str.substr(varPos + 5, varEnd - varPos - 5);
        std::string varValue = Variables::INSTANCE.getVariable(varName, "");
        str.replace(varPos, varEnd - varPos + 1, varValue);
        varPos += varValue.length();
    }
    return str;
}

// %VAR: substitution isn't done here - it's applied once, centrally, by
// CommandManager::run() itself (see ReplaceVariableKeywords above), which
// every caller of this function's result eventually goes through.
Json::Value CommandManager::ReplaceCommandKeywords(Json::Value cmd, std::map<std::string, std::string>& keywords) {
    if (!cmd.isMember("args"))
        return cmd;

    if (!cmd["args"].isArray())
        return cmd;
    for (Json::ArrayIndex i = 0; i < cmd["args"].size(); i++) {
        // Only text has keywords in it; an object or array arg is passed on
        // as JSON by run(), and asString() would throw on it here.
        if (cmd["args"][i].isString()) {
            cmd["args"][i] = ReplaceKeywords(cmd["args"][i].asString(), keywords);
        }
    }

    return cmd;
}

// A preset whose own commands fire the event that triggers it - a MEDIA_STARTED
// preset that runs Play Media, which fires MEDIA_STARTED again - recursed on
// the calling thread without bound. Each level started and stopped another
// pipeline; thousands of nested calls deep, a second request racing the chain
// for the same stream slot hit a freed object. Run a preset at most once per
// call chain: a nested trigger of a preset already running on this thread is
// skipped. Triggers from other threads, or later from the same thread (a
// MEDIA_STOPPED preset restarting media when it ends), are unaffected.
static constexpr int WARNING_ID_PRESET_RECURSION = 66;

class PresetRecursionGuard {
public:
    explicit PresetRecursionGuard(const std::string& name) :
        m_name(name), m_entered(running().insert(name).second) {
        if (!m_entered) {
            LogWarn(VB_COMMAND, "Command Preset \"%s\" triggered itself while it was still running; skipping the nested run\n", name.c_str());
            WarningHolder::AddWarningTimeout(900, WARNING_ID_PRESET_RECURSION,
                                             "Command Preset \"" + name + "\" triggers itself, so its nested run was skipped");
        }
    }
    ~PresetRecursionGuard() {
        if (m_entered) {
            running().erase(m_name);
        }
    }
    bool entered() const { return m_entered; }

private:
    static std::set<std::string>& running() {
        static thread_local std::set<std::string> names;
        return names;
    }
    std::string m_name;
    bool m_entered;
};

int CommandManager::TriggerPreset(int slot, std::map<std::string, std::string>& keywords) {
    std::unique_lock<std::mutex> lock(presetsMutex);
    std::list<Json::Value> slotPresets;
    std::list<std::string> slotPresetNames;
    for (auto const& name : presets.getMemberNames()) {
        for (int i = 0; i < presets[name].size(); i++) {
            if (presets[name][i]["presetSlot"].asInt() == slot) {
                Json::Value cmd = ReplaceCommandKeywords(presets[name][i], keywords);
                slotPresets.push_back(cmd);
                slotPresetNames.push_back(name);
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

    auto nameIt = slotPresetNames.begin();
    for (auto const& preset : slotPresets) {
        PresetRecursionGuard guard(*nameIt);
        if (guard.entered()) {
            LogDebug(VB_COMMAND, "Command Preset \"%s\" (slot %d) running command \"%s\"\n", nameIt->c_str(), slot, preset["command"].asString().c_str());
            run(preset);
        }
        ++nameIt;
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

    PresetRecursionGuard guard(name);
    if (!guard.entered()) {
        return 0;
    }

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
        LogDebug(VB_COMMAND, "Command Preset \"%s\" running command \"%s\"\n", name.c_str(), cmd["command"].asString().c_str());
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
        allCommands = LoadJsonFromFile(commandsFile, JsonRoot::Object);
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
                    if (LoadJsonFromFile(filename, event, JsonRoot::Object)) {
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

    // Built aside and swapped in under presetsMutex: TriggerPreset() reads
    // `presets` under that lock from HTTP, MQTT and main-loop threads, and a
    // reload used to clear and refill it in place with no lock at all.
    //
    // The file can be hand-edited or uploaded through the config file API, so
    // every entry is checked here, once, rather than by asString()/asInt() at
    // trigger time - where the wrong type threw on whichever thread fired the
    // preset, and here at startup threw out of Init() on every boot.
    Json::Value loaded(Json::objectValue);
    const Json::Value& list = allCommands.isObject() ? allCommands["commands"] : Json::Value::nullSingleton();
    if (!list.isNull() && !list.isArray()) {
        LogErr(VB_COMMAND, "commandPresets.json: \"commands\" is not an array - no presets loaded\n");
    }
    for (Json::ArrayIndex i = 0; list.isArray() && i < list.size(); i++) {
        Json::Value entry = list[i];
        if (!entry.isObject() || !entry["name"].isString() || entry["name"].asString().empty() ||
            !entry["command"].isString() || (!entry["args"].isNull() && !entry["args"].isArray())) {
            LogWarn(VB_COMMAND, "commandPresets.json: skipping entry %d - it needs a text \"name\" and \"command\" and an \"args\" array\n", (int)i);
            continue;
        }
        if (!entry["presetSlot"].isConvertibleTo(Json::intValue)) {
            entry["presetSlot"] = 0;
        }
        std::string name = entry["name"].asString();
        if (!loaded.isMember(name)) {
            loaded[name] = Json::Value(Json::arrayValue);
        }
        loaded[name].append(entry);
    }
    std::unique_lock<std::mutex> lock(presetsMutex);
    presets.swap(loaded);
    missingPresets.clear();
}

bool CommandManager::HasPreset(const std::string& name) {
    std::unique_lock<std::mutex> lock(presetsMutex);
    return presets.isMember(name);
}
