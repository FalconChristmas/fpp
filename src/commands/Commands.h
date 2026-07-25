#pragma once
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

#include <atomic>
#include "fpp-json.h"
#include "fpphttp_types.h"
#include <list>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>

class Command {
public:
    Command(const std::string& n);
    Command(const std::string& n, const std::string& descript);
    virtual ~Command();

    virtual Json::Value getDescription();
    virtual bool hidden() const { return false; }

    // ABI RULE: do NOT add new virtual methods to Command, and do NOT make any
    // Command method call a virtual that older objects might lack. External
    // channel-output/event plugins subclass Command and are compiled separately
    // against these headers; inserting a virtual shifts the vtable so FPP
    // dispatches a base-class call into the wrong slot of a pre-built plugin's
    // object. (A mid-vtable disallowMultisync() once did exactly this:
    // Command::getDescription() ended up calling the plugin's run() -> crash.)
    // To add per-command metadata, emit it from an override of the existing
    // getDescription() virtual, ideally via an FPP-internal intermediate class
    // (see LocalOnlyCommand) that plugins never subclass.

    class Result {
    public:
        Result() :
            m_contentType("text/plain") {}
        Result(const std::string& r) :
            m_result(r),
            m_contentType("text/plain") {}
        virtual ~Result() {}

        virtual bool isDone() { return true; }
        virtual bool isError() { return false; }
        virtual const std::string& get() { return m_result; }
        virtual const std::string& contentType() { return m_contentType; }

    protected:
        std::string m_result;
        std::string m_contentType;
    };
    class ErrorResult : public Result {
    public:
        ErrorResult() :
            Result() {}
        ErrorResult(const std::string& r) :
            Result(r) {}
        virtual ~ErrorResult() {}
        virtual bool isError() override { return true; }
    };

    virtual std::unique_ptr<Result> run(const std::vector<std::string>& args) = 0;

    class CommandArg {
    public:
        CommandArg(const std::string& n, const std::string& t, const std::string& d, bool o = false) :
            name(n),
            type(t),
            description(d),
            optional(o),
            min(-1),
            max(-1),
            allowBlanks(false),
            adjustable(false),
            advanced(false) {}

        CommandArg& setRange(int mn, int mx) {
            min = mn;
            max = mx;
            return *this;
        }
        CommandArg& setContentListUrl(const std::string& s, bool ab = false) {
            contentListUrl = s;
            allowBlanks = ab;
            return *this;
        }
        CommandArg& setContentList(std::vector<std::string> v) {
            contentList = std::move(v);
            return *this;
        }
        CommandArg& setContentListRange(std::span<const std::string_view> s) {
            contentList.clear();
            contentList.reserve(s.size());
            for (auto& e : s)
                contentList.emplace_back(e);
            return *this;
        }
        CommandArg& setDefaultValue(std::string_view d) {
            defaultValue = d;
            return *this;
        }
        CommandArg& setGetAdjustableValueURL(const std::string& g) {
            adjustableGetValueURL = g;
            adjustable = true;
            return *this;
        }
        CommandArg& setAdjustable() {
            adjustable = true;
            return *this;
        }
        // Maps a value of this arg to the names of other args that are only
        // relevant when this arg has that value (e.g. "type" == "Random" ->
        // show "min"/"max"). Frontend hides every listed child arg's row
        // until its parent's value matches, mirroring the same mechanism
        // already used by the playlist-entry-type editor (fpp.js
        // UpdateChildVisibility) for CommandArg-driven UIs generally.
        CommandArg& setChildren(std::map<std::string, std::vector<std::string>> c) {
            children = std::move(c);
            return *this;
        }
        // Hides this arg's row unless FPP's global UI Level setting is
        // "Advanced" (frontend: fpp.js PrintArgInputs already checks
        // val.advanced, this was simply never set from any Command until now).
        CommandArg& setAdvanced(bool a = true) {
            advanced = a;
            return *this;
        }
        // Extra explanatory text shown as a hover tooltip next to the label -
        // for when the short description isn't enough room to explain a
        // field properly. Reuses the same Bootstrap tooltip mechanism
        // (data-bs-toggle="tooltip" + .tooltip()) already used elsewhere in
        // fpp.js (the command-preset preview icon), just newly wired into
        // the generic per-arg renderer.
        CommandArg& setHelp(const std::string& h) {
            help = h;
            return *this;
        }
        // Renders a 2-option contentList as a Bootstrap btn-check pill toggle
        // (matching the If command's own Match ALL/ANY condition toggle)
        // instead of the generic <select> - opt-in only, so every other
        // contentList-backed arg elsewhere in the app keeps its existing
        // dropdown rendering unchanged.
        CommandArg& setToggleStyle(bool t = true) {
            toggleStyle = t;
            return *this;
        }
        // Overrides the label shown next to a toggleStyle arg's own toggle
        // (fpp.js renders it inline, e.g. "Order:", matching the condition
        // editor's own "Match:" label) instead of reusing this arg's
        // description - the description is still what labels the row this
        // toggle is rendered directly above (e.g. "Then Run"), so reusing it
        // here too would just repeat the same text twice.
        CommandArg& setToggleLabel(const std::string& l) {
            toggleLabel = l;
            return *this;
        }

        ~CommandArg() {}

        const std::string name;
        const std::string type;
        const std::string description;

        bool optional;
        std::string contentListUrl;
        std::vector<std::string> contentList;
        bool allowBlanks;
        int min;
        int max;
        std::string defaultValue;
        std::string adjustableGetValueURL;
        bool adjustable;
        bool advanced;
        std::string help;
        std::map<std::string, std::vector<std::string>> children;
        bool toggleStyle = false;
        std::string toggleLabel;
    };

    std::string name;
    std::list<CommandArg> args;
    std::string description;
};

class CommandManager {
public:
    void Init();
    void Cleanup();

    void addCommand(Command* cmd);
    void addCategorizedCommand(Command* cmd, const std::string& category, int level = 0);
    void removeCommand(Command* cmd);
    void removeCommand(const std::string& cmdName);

    Json::Value getDescriptions();
    bool HasPreset(const std::string& name);

    virtual std::unique_ptr<Command::Result> run(const std::string& command, const std::vector<std::string>& args);
    virtual std::unique_ptr<Command::Result> run(const std::string& command, const Json::Value& argsArray);
    virtual std::unique_ptr<Command::Result> run(const Json::Value& command);

    HttpResponsePtr render_GET(const HttpRequestPtr& req);
    HttpResponsePtr render_POST(const HttpRequestPtr& req);

    int TriggerPreset(int slot, std::map<std::string, std::string>& keywords);
    int TriggerPreset(int slot);
    int TriggerPreset(std::string name, std::map<std::string, std::string>& keywords);
    int TriggerPreset(std::string name);

    static CommandManager INSTANCE;

private:
    CommandManager();
    ~CommandManager();

    void LoadPresets();
    void MaybeReloadPresets();

    Json::Value ReplaceCommandKeywords(Json::Value cmd, std::map<std::string, std::string>& keywords);
    // Shared by getDescriptions() and the single-command GET route so both
    // stay in sync on which fields (e.g. category/level) get merged in.
    Json::Value describeCommand(Command* cmd);

    std::mutex presetsMutex;
    Json::Value presets;
    uint64_t lastPresetTimeStamp = 0;

    struct CommandMeta {
        std::string category;
        int level = 0;
    };

    std::map<std::string, Command*> commands;
    std::map<std::string, CommandMeta> commandMeta;
    std::set<std::string> missingPresets;
};
