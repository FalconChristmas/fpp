#ifndef __FPP_PLUGINS_H__
#define __FPP_PLUGINS_H__

#include <vector>
#include <string>
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

	void eventCallback(const char *id, const char *impetus);
	void mediaCallback(const Json::Value &playlist, const MediaDetails &mediaDetails);
    void playlistCallback(const Json::Value &playlist, const std::string &action, const std::string &section, int item);
    void multiSyncData(const std::string &pn, uint8_t *data, int len);
    
    void registerApis(httpserver::webserver *m_ws);
    void unregisterApis(httpserver::webserver *m_ws);
    
    void modifyChannelData(int ms, uint8_t *seqData);
    void addControlCallbacks(std::map<int, std::function<bool(int)>> &callbacks);


    static PluginManager INSTANCE;

private:
    std::vector<FPPPlugin *> mPlugins;
};


#endif //__PLUGINS_H__
