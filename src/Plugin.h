#ifndef __FPP_PLUGIN_H__
#define __FPP_PLUGIN_H__

#include <string>

class OldPlaylistEntry;
class OldPlaylistDetails;

class FPPPlugin {
public:
    FPPPlugin(const std::string &n) : name(n) {}
    virtual ~FPPPlugin() {}
    
    //legacy plugin touch points
    virtual int nextPlaylistEntryCallback(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, OldPlaylistEntry *pe) {return 0;}
    virtual void playlistCallback(OldPlaylistDetails *oldPlaylistDetails, bool starting) {}
    virtual void eventCallback(const char *id, const char *impetus) {}
    virtual void mediaCallback() {}

protected:
    std::string name;
};

#endif
