#ifndef __FPP_PLUGINS_H__
#define __FPP_PLUGINS_H__

#include <vector>
#include <string>

#include "Playlist.h"

// Use these to make code more readable
#define PLAYLIST_STARTING				true
#define PLAYLIST_STOPPING				false

class FPPPlugin;

class PluginManager
{
public:
	PluginManager();
	~PluginManager();
	void init(void);

	int nextPlaylistEntryCallback(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, OldPlaylistEntry *pe);
	void playlistCallback(OldPlaylistDetails *oldPlaylistDetails, bool starting);
	void eventCallback(const char *id, const char *impetus);
	void mediaCallback();

private:
    std::vector<FPPPlugin *> mPlugins;
};

extern PluginManager pluginManager;

#endif //__PLUGINS_H__
