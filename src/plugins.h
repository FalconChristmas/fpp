#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#include "playList.h"


// Non-blocking callbacks
#define MEDIA_CALLBACK					1
#define PLAYLIST_CALLBACK				2

// Blocking callbacks
#define NEXT_PLAYLIST_ENTRY_CALLBACK	4


void InitPluginCallbacks(void);
void MediaCallback(char *sequence_name, char *media_file);
void PlaylistCallback(PlaylistDetails *playlistDetails);
void NextPlaylistEntryCallback(void);


#endif //__PLUGINS_H__
