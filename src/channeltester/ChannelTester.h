/*
 *   Channel Test code for Falcon Pi Player (FPP)
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

#ifndef _CHANNELTESTER_H
#define _CHANNELTESTER_H

#include <string>

#include <pthread.h>

#include "TestPatternBase.h"
#include "Sequence.h"

class ChannelTester {
  public:
    ChannelTester();
	~ChannelTester();

	int  SetupTest(std::string configStr);

	void OverlayTestData(char *channelData);
	int  Testing(void);

	std::string GetConfig(void);

  private:
	unsigned int     m_testing;
	TestPatternBase *m_testPattern;
	pthread_mutex_t  m_testLock;
	std::string      m_configStr;
};

#endif /* _CHANNELTESTER_H */
