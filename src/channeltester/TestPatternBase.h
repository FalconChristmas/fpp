/*
 *   Channel Test Pattern base class for Falcon Player (FPP)
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

#ifndef _TESTPATTERNBASE_H
#define _TESTPATTERNBASE_H

#include <string>
#include <utility>
#include <vector>

#include <jsoncpp/json/json.h>

#include "Sequence.h"

class TestPatternBase {
  public:
    TestPatternBase();
	virtual ~TestPatternBase();

	std::string  Name(void) { return m_testPatternName; }
	int          Init(std::string configStr);
	virtual int  Init(Json::Value config);
	virtual int  SetupTest(void);

	int          SetChannelSet(std::string channelSetStr);
	int          OverlayTestData(char *channelData);
	void         DisableTest(void) { m_testEnabled = 0; }

	virtual void DumpConfig(void);

  protected:
	virtual void CycleData(void) {}

	volatile int      m_testEnabled;
	std::string       m_testPatternName;
	int               m_minChannels;
	char             *m_testData;
	long long         m_nextCycleTime;
	int               m_cycleMS;

	std::string       m_channelSetStr;
	int               m_channelCount;
	int               m_configChanged;

	std::vector<std::pair<int,int> >  m_channelSet;
};

#endif /* _TESTPATTERNBASE_H */
