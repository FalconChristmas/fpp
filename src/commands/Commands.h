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
#include <httpserver.hpp>
#include <list>
#include <map>
#include <string>
#include <vector>

class Command {
public:
    Command(const std::string& n);
    Command(const std::string& n, const std::string& descript);
    virtual ~Command();

    virtual Json::Value getDescription();
    virtual bool hidden() const { return false; }

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
            adjustable(false) {}

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
        CommandArg& setContentList(const std::vector<std::string>& v) {
            contentList = v;
            return *this;
        }
        CommandArg& setDefaultValue(const std::string& d) {
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
    };

    std::string name;
    std::list<CommandArg> args;
    std::string description;
};

class CommandManager : public httpserver::http_resource {
public:
    void Init();
    void Cleanup();

    void addCommand(Command* cmd);
    void removeCommand(Command* cmd);
    void removeCommand(const std::string& cmdName);

    Json::Value getDescriptions();

    virtual std::unique_ptr<Command::Result> run(const std::string& command, const std::vector<std::string>& args);
    virtual std::unique_ptr<Command::Result> run(const std::string& command, const Json::Value& argsArray);
    virtual std::unique_ptr<Command::Result> run(const Json::Value& command);
    virtual std::unique_ptr<Command::Result> runRemoteCommand(const std::string& remote, const std::string& command, const std::vector<std::string>& args);

    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request& req) override;
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request& req) override;

    int TriggerPreset(int slot, std::map<std::string, std::string>& keywords);
    int TriggerPreset(int slot);
    int TriggerPreset(std::string name, std::map<std::string, std::string>& keywords);
    int TriggerPreset(std::string name);

    static CommandManager INSTANCE;

private:
    CommandManager();
    ~CommandManager();

    void LoadPresets();

    Json::Value ReplaceCommandKeywords(Json::Value cmd, std::map<std::string, std::string>& keywords);

    Json::Value presets;

    std::map<std::string, Command*> commands;
};
