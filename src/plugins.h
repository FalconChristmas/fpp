#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#include <stdbool.h>

#include "playList.h"


// Non-blocking callbacks
#define MEDIA_CALLBACK					1
#define PLAYLIST_CALLBACK				2

// Blocking callbacks
#define NEXT_PLAYLIST_ENTRY_CALLBACK	4

// Use these to make code more readable
#define PLAYLIST_STARTING				true
#define PLAYLIST_STOPPING				false


void InitPluginCallbacks(void);
void MediaCallback(void);
void PlaylistCallback(PlaylistDetails *playlistDetails, bool starting);
int NextPlaylistEntryCallback(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, PlaylistEntry *pe);


#endif //__PLUGINS_H__
