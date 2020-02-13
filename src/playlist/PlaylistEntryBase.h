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

#ifndef _PLAYLISTENTRYBASE_H
#define _PLAYLISTENTRYBASE_H

#include <pthread.h>

#include <string>

#include <jsoncpp/json/json.h>

class PlaylistEntryBase {
  public:
	PlaylistEntryBase(PlaylistEntryBase *parent = NULL);
	virtual ~PlaylistEntryBase();

	virtual int  Init(Json::Value &config);

	virtual int  StartPlaying(void);
	virtual int  IsStarted(void);
	virtual int  IsPlaying(void);
	virtual int  IsFinished(void);

	virtual int  Prep(void);
	virtual int  Process(void);
	virtual int  Stop(void);

	virtual int  HandleSigChild(pid_t pid);

	virtual void Dump(void);

	virtual Json::Value GetConfig(void);

	virtual Json::Value GetMqttStatus(void);

	virtual std::string ReplaceMatches(std::string in);

	std::string  GetType(void) { return m_type; }
	int          IsPrepped(void) { return m_isPrepped; }
    
    
    enum class PlaylistBranchType {
        None,
        Index,
        Offset
    };
    virtual PlaylistBranchType GetNextBranchType() { return PlaylistBranchType::None; }
    virtual std::string  GetNextSection(void) { return ""; }
    virtual int          GetNextItem(void) { return 0; }


  protected:
	int          CanPlay(void);
	void         FinishPlay(void);

	std::string  m_type;
	std::string  m_note;
	int          m_enabled;
	int          m_isStarted;
	int          m_isPlaying;
	int          m_isFinished;
	int          m_playOnce;
	int          m_playCount;
	int          m_isPrepped;
	int          m_deprecated;


	Json::Value  m_config;
	PlaylistEntryBase *m_parent;
};

#endif
