#pragma once
/*
 *   Channel Test Pattern RGB Cycle class for Falcon Player (FPP)
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
#include <vector>

#include "TestPatternBase.h"

class TestPatternRGBCycle : public TestPatternBase {
public:
    TestPatternRGBCycle();
    virtual ~TestPatternRGBCycle();

    virtual int Init(Json::Value config) override;

    virtual int SetupTest(void) override;
    virtual void DumpConfig(void) override;

private:
    void CycleData(void) override;

    std::string m_colorPatternStr;
    std::vector<char> m_colorPattern;
    int m_colorPatternSize;
    int m_patternOffset;
};
