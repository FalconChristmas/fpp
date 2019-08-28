#ifndef __FPP_PLUGINS_H__
#define __FPP_PLUGINS_H__

#include <vector>
#include <string>
#include <functional>
#include <httpserver.hpp>
#include <jsoncpp/json/json.h>

class FPPPlugin;
class MediaDetails;

class PluginManager
{
public:
	PluginManager();
	~PluginManager();
	void init(void);

	void eventCallback(const char *id, const char *impetus);
	void mediaCallback(const Json::Value &playlist, const MediaDetails &mediaDetails);
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
