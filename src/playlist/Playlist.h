/*
 *   Playlist Entry Base Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2016 the Falcon Player Developers
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

#include <string>
#include <vector>

#include <jsoncpp/json/json.h>

#include "PlaylistEntryBase.h"

// Use these to make code more readable
#define PLAYLIST_STARTING				true
#define PLAYLIST_STOPPING				false

// FIXME PLAYLIST, get rid of this and use playlist/PlaylistEntryBase.h
// temporarily copied from old Playlist.h during transition to new playlist
#define PL_TYPE_BOTH            0
#define PL_TYPE_MEDIA           1
#define PL_TYPE_SEQUENCE        2
#define PL_TYPE_PAUSE           3
#define PL_TYPE_VIDEO           4 // deprecated, legacy v0.2.0 implementation
#define PL_TYPE_EVENT           5
#define PL_TYPE_PLUGIN_NEXT     6
#define PL_MAX_ENTRIES        128


// FIXME PLAYLIST, get rid of this and use playlist/PlaylistEntryBase.h
// temporarily copied from old Playlist.h during transition to new playlist
typedef struct {
	unsigned char type;
	char cType;
	char seqName[256];
	char songName[256];
	char eventID[6];
	unsigned int pauselength;
	char data[256];
} PlaylistEntry;

// FIXME PLAYLIST, get rid of this and use playlist/PlaylistEntryBase.h
// temporarily copied from old Playlist.h during transition to new playlist
typedef struct {
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
} PlaylistDetails;


class Playlist {
  public:
	Playlist(void *parent = NULL, int subPlaylist = 0);
	~Playlist();

	////////////////////////////////////////
	// Stub out old Playlist class methods
	void StopPlaylistGracefully(void);
	void StopPlaylistNow(void);
	void PlayListPlayingInit(void);
	void PlayListPlayingProcess(void);
	void PlayListPlayingCleanup(void);
	void PlaylistProcessMediaData(void);

	int m_playlistAction;
	PlaylistDetails m_playlistDetails;
	int m_numberOfSecondsPaused;
	////////////////////////////////////////

	// New methods
	Json::Value        LoadJSON(const char *filename);
	int                Load(Json::Value &config);
	int                Load(const char *filename);
	PlaylistEntryBase *LoadPlaylistEntry(Json::Value entry);
	int                LoadJSONIntoPlaylist(std::vector<PlaylistEntryBase*> &playlistPart, const Json::Value &entries);

	int                Start(void);
	int                StopNow(void);
	int                StopGracefully(int afterCurrentLoop = 0);

	int                IsPlaying(void);

	int                Process(void);
	int                Cleanup(void);

	void               SetIdle(void);

	int                Play(const char *filename, const int position = -1, const int repeat = -1);

	void               SetPosition(int position);
	void               SetRepeat(int repeat);

	void               Dump(void);

	void               NextItem(void);
	void               PrevItem(void);

	Json::Value        GetCurrentEntry(void);
	Json::Value        GetInfo(void);
	std::string        GetPlaylistName(void) { return m_name; }
	int                GetRepeat(void) { return m_repeat; }
	int                GetPosition(void);
	int                GetSize(void);
	std::string        GetConfigStr(void);
	Json::Value        GetConfig(void);

	int                MQTTHandler(std::string topic, std::string msg);

	std::string        ReplaceMatches(std::string in);

  private:
	void                *m_parent;
  	std::string          m_name;
	std::string          m_desc;
	int                  m_repeat;
	int                  m_loop;
	int                  m_loopCount;
	int                  m_random;
	int                  m_blankBetweenSequences;
	int                  m_blankBetweenIterations;
	int                  m_blankAtEnd;
	long long            m_startTime;
	int                  m_subPlaylistDepth;
	int                  m_subPlaylist;

	std::string          m_currentState;
	std::string          m_currentSectionStr;
	int                  m_sectionPosition;
	int                  m_absolutePosition;

	std::vector<PlaylistEntryBase*>  m_leadIn;
	std::vector<PlaylistEntryBase*>  m_mainPlaylist;
	std::vector<PlaylistEntryBase*>  m_leadOut;
	std::vector<PlaylistEntryBase*> *m_currentSection;
};

// Temporary singleton during conversion
extern Playlist *playlist;

#endif
