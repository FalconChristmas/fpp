/*
 *   Playlist Entry Branch Class for Falcon Player (FPP)
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

#ifndef _PLAYLISTENTRYBRANCH_H
#define _PLAYLISTENTRYBRANCH_H

#include <string>

#include "Playlist.h"
#include "PlaylistEntryBase.h"

#define PE_BRANCH_TYPE_UNDEFINED      ""
#define PE_BRANCH_TYPE_CLOCK_TIME     "Time"

#define PE_BRANCH_COMP_MODE_UNDEFINED 0
#define PE_BRANCH_COMP_MODE_LTEQ      1
#define PE_BRANCH_COMP_MODE_EQ        2
#define PE_BRANCH_COMP_MODE_GTEQ      3

class PlaylistEntryBranch : public PlaylistEntryBase {
  public:
	PlaylistEntryBranch(PlaylistEntryBase *parent = NULL);
	virtual ~PlaylistEntryBranch();

	virtual int  Init(Json::Value &config) override;

	virtual int  StartPlaying(void) override;

	void SetNext(int isTrue);

	virtual void Dump(void) override;

	virtual Json::Value GetConfig(void) override;

    virtual PlaylistBranchType GetNextBranchType() override { return m_nextBranchType; }
    virtual std::string  GetNextSection(void) override { return m_nextSection; }
    virtual int          GetNextItem(void) override { return m_nextItem; }

  private:
	std::string  m_branchType;
	int  m_comparisonMode;

	// Time comparison
	std::string m_startTime;
	std::string m_endTime;
	int  m_sHour;
	int  m_sMinute;
	int  m_sSecond;
	int  m_sDaySecond;
	int  m_sHourSecond;
	int  m_eHour;
	int  m_eMinute;
	int  m_eSecond;
	int  m_eDaySecond;
	int  m_eHourSecond;

    PlaylistBranchType m_trueNextBranchType;
	std::string m_trueNextSection;
	int         m_trueNextItem;
    PlaylistBranchType m_falseNextBranchType;
	std::string m_falseNextSection;
	int         m_falseNextItem;
    
    PlaylistBranchType m_nextBranchType;
    std::string  m_nextSection;
    int          m_nextItem;
};

#endif
