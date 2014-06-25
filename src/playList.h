/*
 *   Play list handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

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

#define PL_MAX_ENTRIES       64

typedef struct{
    unsigned char  type;
    char  cType;
    char seqName[256];
    char songName[256];
    char eventID[6];
    unsigned int pauselength;
}PlaylistEntry;

typedef struct{
	PlaylistEntry playList[PL_MAX_ENTRIES];
	char currentPlaylist[128];
	char currentPlaylistFile[128];
	int  playListCount;
	int  currentPlaylistEntry;
	int  StopPlaylist;
	int  ForceStop;
	int  playlistStarting;
	int  first;
	int  last;
	int  repeat;
}PlaylistDetails;

void CalculateNextPlayListEntry();
int ReadPlaylist(char const * file);
void PlayListPlayingInit(void);
void PlayListPlayingProcess(void);
void PlayListPlayingCleanup(void);
void PauseProcess(void);
void Play_PlaylistEntry(void);
void PlaylistPlaySong(void);
void PlaylistPrint();
void StopPlaylistGracefully(void);
void StopPlaylistNow(void);
void JumpToPlaylistEntry(int entryIndex);

#endif
