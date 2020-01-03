/*
 *   Channel Test Pattern Single Chase class for Falcon Player (FPP)
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

#ifndef _SINGLECHASE_H
#define _SINGLECHASE_H

#include <string>
#include <vector>

#include <jsoncpp/json/json.h>

#include "TestPatternBase.h"

class TestPatternSingleChase : public TestPatternBase {
  public:
    TestPatternSingleChase();
	virtual ~TestPatternSingleChase();

	virtual int Init(Json::Value config) override;

	virtual int SetupTest(void) override;
	virtual void DumpConfig(void) override;

  private:
	void CycleData(void) override;

	int  m_chaseSize;
	char m_chaseValue;
};

#endif /* _SINGLECHASE_H */
