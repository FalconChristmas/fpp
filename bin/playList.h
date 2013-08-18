#ifndef _PLAYLIST_H
#define _PLAYLIST_H

#define PL_TYPE_BOTH          0
#define PL_TYPE_MUSIC         1
#define PL_TYPE_SEQUENCE      2
#define PL_TYPE_PAUSE         3

#define PAUSE_STATUS_IDLE     0
#define PAUSE_STATUS_STARTED  1
#define PAUSE_STATUS_ENDED    2

#define PLAYLIST_STOP_INDEX		-1


typedef struct{
    unsigned char  type;
    char  cType;
    char seqName[256];
    char songName[256];
    unsigned char pauselength;
}PlaylistEntry;

typedef struct{
	PlaylistEntry playList[32];
	int currentPlaylist[128];
	int currentPlaylistFile[128];
	int playListCount;
	int currentPlaylistEntry;
	int StopPlaylist;
	int playlistStarting;
	int first;
	int last;
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
int FileExists(char * File);


#endif