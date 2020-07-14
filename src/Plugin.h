#pragma once

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
    FPPPlugin(const std::string &n);
    virtual ~FPPPlugin() {}
    
    //Touch points for various triggers within FPP
    virtual void eventCallback(const char *id, const char *impetus) {}
    virtual void mediaCallback(const Json::Value &playlist, const MediaDetails &mediaDetails) {}
    virtual void playlistCallback(const Json::Value &playlist, const std::string &action, const std::string &section, int item) {}

    // intitialization routines
    virtual void registerApis(httpserver::webserver *m_ws) {}
    virtual void unregisterApis(httpserver::webserver *m_ws) {}
    virtual void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks) {}

    // direct access to channel data.
    // modifySequenceData is immedately after data is loaded/recieved (before overlays)
    virtual void modifySequenceData(int ms, uint8_t *seqData) {}
    // modifyChannelData is immediately before sending to outputs (after overlays)
    virtual void modifyChannelData(int ms, uint8_t *seqData) {}
        
    // A plugin can call PluginManager::INSTANCE.multiSyncData(pluginName, data, len);
    // with data and that data is multisynced out to all the remotes.  If the plugin
    // is installed and running on the remote, it will get that data in via this method
    virtual void multiSyncData(const uint8_t *data, int len) {}

    
    const std::string & getName() const { return name; }
protected:
    void reloadSettings();
    
    std::string name;
    std::map<std::string, std::string> settings;
};
