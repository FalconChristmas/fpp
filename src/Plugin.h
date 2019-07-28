#ifndef __FPP_PLUGIN_H__
#define __FPP_PLUGIN_H__

#include <string>

namespace httpserver {
    class webserver;
}

class FPPPlugin {
public:
    FPPPlugin(const std::string &n) : name(n) {}
    virtual ~FPPPlugin() {}
    
    //legacy plugin touch points
    virtual void eventCallback(const char *id, const char *impetus) {}
    virtual void mediaCallback() {}

    
    
    virtual void registerApis(httpserver::webserver *m_ws) {}
    virtual void unregisterApis(httpserver::webserver *m_ws) {}
    virtual void modifyChannelData(int ms, uint8_t *seqData) {}

protected:
    std::string name;
};

#endif
