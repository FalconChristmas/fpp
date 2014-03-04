#ifndef _PLAYLIST_H
#define _PLAYLIST_H

#define PL_TYPE_BOTH          0
#define PL_TYPE_MEDIA         1
#define PL_TYPE_SEQUENCE      2
#define PL_TYPE_PAUSE         3
#define PL_TYPE_VIDEO         4 // deprecated, legacy v0.2.0 implementation
#define PL_TYPE_EVENT         5

#define PAUSE_STATUS_IDLE     0
#define PAUSE_STATUS_STARTED  1
#define PAUSE_STATUS_ENDED    2

#define PLAYLIST_STOP_INDEX		-1


typedef struct{
    unsigned char  type;
    char  cType;
    char seqName[256];
    char songName[256];
    char eventID[6];
    unsigned int pauselength;
}PlaylistEntry;

typedef struct{
	PlaylistEntry playList[32];
	int currentPlaylist[128];
	int currentPlaylistFile[128];
	int playListCount;
	int currentPlaylistEntry;
	int StopPlaylist;
	int ForceStop;
	int playlistStarting;
	int first;
	int last;
	int repeat;
}PlaylistDetails;

void CalculateNextPlayListEntry();
int ReadPlaylist(char const * file);
void PlayListPlayingLoop(void);
void PauseProcess(void);
void Play_PlaylistEntry(void);
void PlaylistPlaySong(void);
void PlaylistPrint();
void StopPlaylistGracefully(void);
void JumpToPlaylistEntry(int entryIndex);
int DirectoryExists(char * Directory);

#endif
