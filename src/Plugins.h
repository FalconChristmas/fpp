#ifndef __FPP_PLUGINS_H__
#define __FPP_PLUGINS_H__

#include <vector>
#include <string>
#include <httpserver.hpp>

class FPPPlugin;

class PluginManager
{
public:
	PluginManager();
	~PluginManager();
	void init(void);

	void eventCallback(const char *id, const char *impetus);
	void mediaCallback();

    
    void registerApis(httpserver::webserver *m_ws);
    void unregisterApis(httpserver::webserver *m_ws);
    
    void modifyChannelData(int ms, uint8_t *seqData);
private:
    std::vector<FPPPlugin *> mPlugins;
};

extern PluginManager pluginManager;

#endif //__PLUGINS_H__
