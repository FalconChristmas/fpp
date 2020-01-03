/*
 *   Playlist Entry Base Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
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

#include <mutex>
#include <string>
#include <vector>
#include <atomic>

#include <jsoncpp/json/json.h>

#include "PlaylistEntryBase.h"


enum PlaylistStatus {
    FPP_STATUS_IDLE = 0,
    FPP_STATUS_PLAYLIST_PLAYING,
    FPP_STATUS_STOPPING_GRACEFULLY,
    FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP,
    FPP_STATUS_STOPPING_NOW
};

class Playlist {
  public:
	Playlist(Playlist *parent = NULL);
	~Playlist();

    PlaylistStatus getPlaylistStatus() {
        return m_status;
    }
    
	// New methods
	Json::Value        LoadJSON(const char *filename);
	int                Load(Json::Value &config);
	int                Load(const char *filename);
	PlaylistEntryBase *LoadPlaylistEntry(Json::Value entry);
	int                LoadJSONIntoPlaylist(std::vector<PlaylistEntryBase*> &playlistPart, const Json::Value &entries);

	int                Start(void);
	int                StopNow(int forceStop = 0);
	int                StopGracefully(int forceStop = 0, int afterCurrentLoop = 0);
    void               SetIdle(bool exit = true);

	int                IsPlaying(void);

	int                Process(void);
	void               ProcessMedia(void);
	int                Cleanup(void);

	int                Play(const char *filename, const int position = -1, const int repeat = -1, const int scheduled = 0);
    
    void               InsertPlaylistAsNext(const std::string &filename, const int position = -1);

	void               SetPosition(int position);
	void               SetRepeat(int repeat);

	void               Dump(void);

	void               NextItem(void);
    void               RestartItem(void);
	void               PrevItem(void);

	Json::Value        GetCurrentEntry(void);
	Json::Value        GetInfo(void);
	std::string        GetPlaylistName(void) { return m_name; }
	int                GetRepeat(void) { return m_repeat; }
	int                GetPosition(void);
	int                GetSize(void);
	std::string        GetConfigStr(void);
	Json::Value        GetConfig(void);
	uint64_t           GetFileTime(void) { return (Json::UInt64)m_fileTime; }
	int                GetForceStop(void) { return m_forceStop; }
	int                WasScheduled(void) { return m_scheduled; }

	int                MQTTHandler(std::string topic, std::string msg);

	int                FileHasBeenModified(void);
	std::string        ReplaceMatches(std::string in);
	Json::Value        GetMqttStatusJSON(); // Returns Status as JSON

  private:
    
	int                ReloadPlaylist(void);
	void               ReloadIfNeeded(void);
	void               SwitchToMainPlaylist(void);
	void               SwitchToLeadOut(void);
    
    bool               SwitchToInsertedPlaylist();

    PlaylistStatus       m_status;
    
	Playlist            *m_parent;
	std::string          m_filename;
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
	int                  m_scheduled;
	int                  m_forceStop;

	time_t               m_fileTime;
	Json::Value          m_config;
	time_t               m_configTime;
	Json::Value          m_playlistInfo;

	std::string          m_currentState;
	std::string          m_currentSectionStr;
	int                  m_sectionPosition;
	int                  m_startPosition;

    
    std::string          m_insertedPlaylist;
    int                  m_insertedPlaylistPosition;
    
	std::recursive_mutex m_playlistMutex;

	std::vector<PlaylistEntryBase*>  m_leadIn;
	std::vector<PlaylistEntryBase*>  m_mainPlaylist;
	std::vector<PlaylistEntryBase*>  m_leadOut;
	std::vector<PlaylistEntryBase*> *m_currentSection;
};

// Temporary singleton during conversion
extern Playlist * playlist;

#endif
