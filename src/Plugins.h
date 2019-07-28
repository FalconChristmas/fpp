#ifndef __FPP_PLUGINS_H__
#define __FPP_PLUGINS_H__

#include <vector>
#include <string>
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

    
    void registerApis(httpserver::webserver *m_ws);
    void unregisterApis(httpserver::webserver *m_ws);
    
    void modifyChannelData(int ms, uint8_t *seqData);


    static PluginManager INSTANCE;

private:
    std::vector<FPPPlugin *> mPlugins;
};


#endif //__PLUGINS_H__
