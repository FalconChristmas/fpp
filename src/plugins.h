#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#include <stdbool.h>

#include "playList.h"


#define MEDIA_CALLBACK					1
#define PLAYLIST_CALLBACK				2
#define NEXT_PLAYLIST_ENTRY_CALLBACK	4
#define EVENT_ENTRY_CALLBACK			8

// Use these to make code more readable
#define PLAYLIST_STARTING				true
#define PLAYLIST_STOPPING				false


void InitPluginCallbacks(void);
void MediaCallback(void);
void PlaylistCallback(PlaylistDetails *playlistDetails, bool starting);
int NextPlaylistEntryCallback(const char *plugin_data, int currentPlaylistEntry, int mode, bool repeat, PlaylistEntry *pe);
void EventCallback(char *id, char *impetus);


#endif //__PLUGINS_H__
