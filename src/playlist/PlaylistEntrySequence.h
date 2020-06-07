#pragma once
/*
 *   Playlist Entry Sequence Class for Falcon Player (FPP)
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


#include <string>

#include "PlaylistEntryBase.h"

class PlaylistEntrySequence : public PlaylistEntryBase {
  public:
	PlaylistEntrySequence(PlaylistEntryBase *parent = NULL);
	virtual ~PlaylistEntrySequence();

	virtual int  Init(Json::Value &config) override;

    int  PreparePlay(int frame = 0);
	virtual int  StartPlaying(void) override;
	virtual int  Process(void) override;
	virtual int  Stop(void) override;

    virtual void Pause() override;
    virtual bool IsPaused() override;
    virtual void Resume() override;

	virtual void Dump(void) override;

	virtual Json::Value GetConfig(void) override;
	virtual Json::Value GetMqttStatus(void) override;

    virtual uint64_t GetLengthInMS() override;
    virtual uint64_t GetElapsedMS() override;
    
    void disableAdjustTiming() { m_adjustTiming = false; }

	std::string GetSequenceName(void) { return m_sequenceName; }

  private:
    
    long long            m_startTme;
    bool                 m_adjustTiming;
    bool                 m_prepared;
	int                  m_duration;
    int                  m_sequenceFrameTime;
	std::string          m_sequenceName;
    
    int                  m_pausedFrame;
};
