#pragma once

#include <vector>
#include <string>
#include <atomic>
#include <functional>
#include <jsoncpp/json/json.h>

class FPPPlugin;
class MediaDetails;

namespace httpserver {
class webserver;
}

class PluginManager
{
public:
	PluginManager();
	~PluginManager();
	void init(void);
    
    bool hasPlugins();

	void eventCallback(const char *id, const char *impetus);
	void mediaCallback(const Json::Value &playlist, const MediaDetails &mediaDetails);
    void playlistCallback(const Json::Value &playlist, const std::string &action, const std::string &section, int item);
    void multiSyncData(const std::string &pn, uint8_t *data, int len);
    
    void registerApis(httpserver::webserver *m_ws);
    void unregisterApis(httpserver::webserver *m_ws);
    
    void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks);

    void modifySequenceData(int ms, uint8_t *seqData);
    void modifyChannelData(int ms, uint8_t *seqData);

    static PluginManager INSTANCE;

private:
    std::vector<FPPPlugin *> mPlugins;
    std::vector<void*> mShlibHandles;
    std::atomic_bool mPluginsLoaded;
};
