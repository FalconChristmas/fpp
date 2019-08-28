#ifndef __FPP_PLUGIN_H__
#define __FPP_PLUGIN_H__

#include <string>
#include <map>
#include <functional>

#include <jsoncpp/json/json.h>

namespace httpserver {
    class webserver;
}
class MediaDetails;

class FPPPlugin {
public:
    FPPPlugin(const std::string &n) : name(n) {}
    virtual ~FPPPlugin() {}
    
    //legacy plugin touch points
    virtual void eventCallback(const char *id, const char *impetus) {}
    virtual void mediaCallback(const Json::Value &playlist, const MediaDetails &mediaDetails) {}
    
    virtual void registerApis(httpserver::webserver *m_ws) {}
    virtual void unregisterApis(httpserver::webserver *m_ws) {}
    virtual void modifyChannelData(int ms, uint8_t *seqData) {}
    
    virtual void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) {}
    
    virtual void multiSyncData(const uint8_t *data, int len) {}
    const std::string & getName() const { return name; }
protected:
    std::string name;
};

#endif
