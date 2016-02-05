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

#ifndef _NEWPLAYLIST_H
#define _NEWPLAYLIST_H

#include <string>
#include <vector>

#include <jsoncpp/json/json.h>

// FIXME PLAYLIST
#include "Playlist.h"

#include "PlaylistEntryBase.h"

class NewPlaylist {
  public:
  	NewPlaylist();
	~NewPlaylist();

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
	int                Load(Json::Value &config);
	int                Load(const char *filename);
	PlaylistEntryBase *LoadPlaylistEntry(Json::Value entry);

	int                Start(void);
	int                StopNow(void);
	int                StopGracefully(int afterCurrentLoop = 0);

	int                Process(void);
	int                Cleanup(void);

	int                Play(void);

	void               SetPosition(int position) {m_sectionPosition = position;}
	void               SetRepeat(int repeat) { m_repeat = repeat; }

	void               Dump(void);

	std::string        GetConfigStr(void);
	Json::Value        GetConfig(void);

  private:
  	std::string          m_name;
	int                  m_repeat;
	int                  m_loop;
	int                  m_loopCount;
	int                  m_random;
	int                  m_blankBetweenSequences;
	int                  m_blankBetweenIterations;
	int                  m_blankAtEnd;
	long long            m_startTime;

	std::string          m_currentState;
	std::string          m_currentSectionStr;
	int                  m_sectionPosition;

	std::vector<PlaylistEntryBase*>  m_leadIn;
	std::vector<PlaylistEntryBase*>  m_mainPlaylist;
	std::vector<PlaylistEntryBase*>  m_leadOut;
	std::vector<PlaylistEntryBase*> *m_currentSection;
};

extern NewPlaylist *newPlaylist;

#endif
