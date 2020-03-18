/*
*   The Falcon Player (FPP) is free software; you can redistribute it
*   and/or modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2 of
*   the License, or (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifndef __FPP_COMMMANDS__
#define __FPP_COMMMANDS__

#include <string>
#include <map>
#include <list>
#include <vector>
#include <atomic>
#include <jsoncpp/json/json.h>
#include <httpserver.hpp>

class Command {
public:
    Command(const std::string &n);
    virtual ~Command();
    
    virtual Json::Value getDescription();
    virtual bool hidden() const { return false; }
    
    class Result {
    public:
        Result() : m_contentType("text/plain") {}
        Result(const std::string &r) : m_result(r) {}
        virtual ~Result() {}
        
        virtual bool isDone() { return true; }
        virtual bool isError() { return false; }
        virtual const std::string &get() { return m_result; }
        virtual const std::string &contentType() { return m_contentType; }
    protected:
        std::string m_result;
        std::string m_contentType;
    };
    class ErrorResult : public Result {
    public:
        ErrorResult() : Result() {}
        ErrorResult(const std::string &r) : Result(r) {}
        virtual ~ErrorResult() {}
        virtual bool isError() override { return true; }
    };
    
    
    virtual std::unique_ptr<Result> run(const std::vector<std::string> &args) = 0;

    
    class CommandArg {
    public:
        CommandArg(const std::string &n, const std::string &t, const std::string &d, bool o = false)
            : name(n), type(t), description(d), optional(o), min(-1), max(-1), allowBlanks(false), adjustable(false) {}

        
        CommandArg &setRange(int mn, int mx) { min = mn; max = mx; return *this;}
        CommandArg &setContentListUrl(const std::string &s, bool ab = false) { contentListUrl = s; allowBlanks = ab; return *this;}
        CommandArg &setContentList(const std::vector<std::string> &v) { contentList = v; return *this;}
        CommandArg &setDefaultValue(const std::string &d) { defaultValue = d; return *this;}
        CommandArg &setGetAdjustableValueURL(const std::string &g) { adjustableGetValueURL = g; adjustable = true; return *this;}
        CommandArg &setAdjustable() { adjustable = true; return *this;}
        
        virtual ~CommandArg() {}


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
};


class CommandManager : public httpserver::http_resource {
public:
    void Init();
    void Cleanup();
    
    void addCommand(Command *cmd);
    void removeCommand(Command *cmd);
    
    Json::Value getDescriptions();
    
    virtual std::unique_ptr<Command::Result> run(const std::string &command, const std::vector<std::string> &args);
    virtual std::unique_ptr<Command::Result> run(const std::string &command, const Json::Value &argsArray);
    virtual std::unique_ptr<Command::Result> run(const Json::Value &command);
    virtual std::unique_ptr<Command::Result> runRemoteCommand(const std::string &remote, const std::string &command, const std::vector<std::string> &args);
    
    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override;
    virtual const std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request &req) override;

    static CommandManager INSTANCE;
private:
    CommandManager();
    ~CommandManager();
    
    std::map<std::string, Command*> commands;
};


#endif
