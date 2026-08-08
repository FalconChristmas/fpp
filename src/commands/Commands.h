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
#include <memory>
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

    // LAYOUT RULE: sizeof(CommandArg) is frozen as of plugin API version 5.
    //
    // Plugins build this type and push_back it onto Command::args; std::list
    // instantiates the node (sizeof(CommandArg) + two pointers) inside the
    // *plugin*, while FPP walks and destroys those nodes with its own idea of
    // the size. Every past mismatch corrupted the heap in the field. So: do NOT
    // add, remove, or reorder a data member below. Anything new goes in Ext,
    // which lives in Commands.cpp and can grow freely because plugins only ever
    // see a pointer to it. fpp_command_arg_abi_size() (bottom of this file)
    // enforces this at plugin load, so a slip here refuses to load rather than
    // corrupting memory - but it is a backstop, not a licence.
    class CommandArg {
    public:
        // Out-of-line for the same reason as the special members below: even
        // though it never touches ext, the compiler still needs ~unique_ptr<Ext>
        // here to unwind if a later member's construction throws.
        CommandArg(const std::string& n, const std::string& t, const std::string& d, bool o = false);

        // Out-of-line so that Ext stays incomplete here. This is load-bearing
        // twice over: an implicitly-generated one would fail to compile in any
        // TU that lacks Ext, and routing them through libfpp is what keeps a
        // plugin's std::list<CommandArg> teardown on FPP's implementation
        // rather than its own possibly-stale copy.
        CommandArg(const CommandArg& o);
        CommandArg(CommandArg&& o) noexcept;
        ~CommandArg();
        // Already implicitly deleted by the const members below; stated so a
        // later edit that drops the const does not silently resurrect them
        // with an Ext-aliasing shallow copy.
        CommandArg& operator=(const CommandArg&) = delete;
        CommandArg& operator=(CommandArg&&) = delete;

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
        // ---- Ext-backed fields ------------------------------------------
        // Everything below is stored in Ext (Commands.cpp), not in this object,
        // so it can change freely without moving anyone's members. These are
        // every field added after plugin API version 3; they all landed between
        // 2026-07-23 and 2026-07-27, and API version 5 already forces a plugin
        // rebuild, so this was the moment to get them behind the pointer before
        // anything depended on their offsets. Add new fields here, never above.
        // The setter signatures are unchanged, so call sites are untouched.

        // Maps a value of this arg to the names of other args that are only
        // relevant when this arg has that value (e.g. "type" == "Random" ->
        // show "min"/"max"). Frontend hides every listed child arg's row
        // until its parent's value matches, mirroring the same mechanism
        // already used by the playlist-entry-type editor (fpp.js
        // UpdateChildVisibility) for CommandArg-driven UIs generally.
        CommandArg& setChildren(std::map<std::string, std::vector<std::string>> c);
        const std::map<std::string, std::vector<std::string>>& getChildren() const;
        // Hides this arg's row unless FPP's global UI Level setting is
        // "Advanced" (frontend: fpp.js PrintArgInputs already checks
        // val.advanced, this was simply never set from any Command until now).
        CommandArg& setAdvanced(bool a = true);
        bool isAdvanced() const;
        // Extra explanatory text shown as a hover tooltip next to the label -
        // for when the short description isn't enough room to explain a
        // field properly. Reuses the same Bootstrap tooltip mechanism
        // (data-bs-toggle="tooltip" + .tooltip()) already used elsewhere in
        // fpp.js (the command-preset preview icon), just newly wired into
        // the generic per-arg renderer.
        CommandArg& setHelp(const std::string& h);
        const std::string& getHelp() const;
        // Groups this arg under a named heading alongside any other
        // consecutive args sharing the same section (fpp.js PrintArgInputs -
        // ported from the playlist-entry-editor's Primary Media / Extra
        // Media / Entry Properties sections). Purely presentational.
        CommandArg& setSection(const std::string& s);
        const std::string& getSection() const;
        // Renders a 2-option contentList as a Bootstrap btn-check pill toggle
        // (matching the If command's own Match ALL/ANY condition toggle)
        // instead of the generic <select> - opt-in only, so every other
        // contentList-backed arg elsewhere in the app keeps its existing
        // dropdown rendering unchanged.
        CommandArg& setToggleStyle(bool t = true);
        bool isToggleStyle() const;
        // Overrides the label shown next to a toggleStyle arg's own toggle
        // (fpp.js renders it inline, e.g. "Order:", matching the condition
        // editor's own "Match:" label) instead of reusing this arg's
        // description - the description is still what labels the row this
        // toggle is rendered directly above (e.g. "Then Run"), so reusing it
        // here too would just repeat the same text twice.
        CommandArg& setToggleLabel(const std::string& l);
        const std::string& getToggleLabel() const;

        // ---- FROZEN AT API VERSION 5 - see the LAYOUT RULE above ----------
        // This is exactly the plugin API version 3 member set, the last one that
        // was ever in the field long enough to matter.
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

    private:
        // ---- everything new goes here, in Commands.cpp -------------------
        // Allocated on first use: most args set no extended field, and they
        // should not pay for one.
        struct Ext;
        Ext& mutableExt();
        std::unique_ptr<Ext> ext;
        // ---- nothing may follow ext ---------------------------------------
    };

    // ---- FROZEN AT API VERSION 5 --------------------------------------------
    // Same rule as CommandArg, and for a sharper reason: plugins *subclass*
    // Command, so a member added here shifts every member of their derived
    // class. New base-class state goes in Data (Commands.cpp) instead.
    // Note sizeof(std::list) does not depend on its element type, so args stays
    // put no matter how CommandArg's Ext grows.
    std::string name;
    std::list<CommandArg> args;
    std::string description;

private:
    struct Data;
    std::unique_ptr<Data> data; // allocated on first use, like CommandArg::ext
    // ---- nothing may follow data --------------------------------------------
};

// ABI fingerprints, checked by PluginManager::loadPlugin().
//
// The Ext/Data pimpls above are what make these two sizes stay put; these are
// what prove it. CommandArg grew twice without FPP_PLUGIN_API_VERSION being
// bumped (advanced/help/children/toggleStyle/toggleLabel, then section), and
// both times the result was heap corruption in the field. Relying on a human to
// remember the bump has visibly not worked, so the sizes travel with the header
// and the loader compares them.
//
// Emitted weak + default-visibility so every plugin that includes this header
// exports its own compiled-in values; libfpp defines them too, and the loader
// uses dladdr() provenance to tell "the plugin disagrees with us" (reject) from
// "the plugin never touches commands" (nothing to check). Optional by
// construction - a plugin that does not include Commands.h is unaffected.
#ifdef __cplusplus
extern "C" {
__attribute__((weak, visibility("default"))) unsigned int fpp_command_arg_abi_size() {
    return (unsigned int)sizeof(Command::CommandArg);
}
__attribute__((weak, visibility("default"))) unsigned int fpp_command_abi_size() {
    return (unsigned int)sizeof(Command);
}
}
#endif

class CommandManager {
public:
    void Init();
    void Cleanup();

    void addCommand(Command* cmd);
    void addCategorizedCommand(Command* cmd, const std::string& category, int level = 0);
    void removeCommand(Command* cmd);
    void removeCommand(const std::string& cmdName);

    // Every currently registered command, name and object. PluginManager diffs
    // this around a plugin's load to work out which commands that plugin
    // registered: addCommand() carries no owner and cannot gain one without
    // breaking every plugin that calls it, so ownership is deduced the same way
    // addControlCallbacks() deduces a plugin's epoll descriptors.
    std::vector<std::pair<std::string, Command*>> getRegisteredCommands() const;

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
