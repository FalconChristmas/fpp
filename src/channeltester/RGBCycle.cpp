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

#include "fpp-pch.h"

#include "RGBCycle.h"

/*
 *
 */
TestPatternRGBCycle::TestPatternRGBCycle()
  : m_colorPatternStr(""),
	m_colorPatternSize(0),
	m_patternOffset(0)
{
	LogExcess(VB_CHANNELOUT, "TestPatternRGBCycle::TestPatternRGBCycle()\n");

	m_testPatternName = "RGBCycle";
}

/*
 *
 */
TestPatternRGBCycle::~TestPatternRGBCycle()
{
	LogExcess(VB_CHANNELOUT, "TestPatternRGBCycle::~TestPatternRGBCycle()\n");
}

/*
 *
 */
int TestPatternRGBCycle::Init(Json::Value config)
{
	m_configChanged = 0;

	if (m_colorPatternStr != config["colorPattern"].asString()) {
		m_colorPatternStr = config["colorPattern"].asString();
		m_configChanged = 1;
	}

	return TestPatternBase::Init(config);
}

/*
 *
 */
int TestPatternRGBCycle::SetupTest(void)
{
	bzero(m_testData, m_channelCount);

	m_colorPattern.clear();

	char digit = 0;
	for (int i = 0; i < m_colorPatternStr.size(); i += 2) {
		digit = (char)strtol(m_colorPatternStr.substr(i,2).c_str(), NULL, 16);
		m_colorPattern.push_back(digit);
	}

	// Make sure we have a valid set of triplets
    while (m_colorPattern.size() % 3) {
		m_colorPattern.push_back(0);
    }

	char *c = m_testData;
	for (int i = 0; i < m_channelCount; i += 3) {
		*(c++) = m_colorPattern[0];
		*(c++) = m_colorPattern[1];
		*(c++) = m_colorPattern[2];
	}

    m_patternOffset = 0;
	m_colorPatternSize = m_colorPattern.size() / 3;

	return TestPatternBase::SetupTest();
}

/*
 *
 */
void TestPatternRGBCycle::CycleData(void)
{
	m_patternOffset += 3;
    if (m_patternOffset >= m_colorPattern.size()) {
        m_patternOffset = 0;
    }
    char *c = m_testData;
    for (int i = 0; i < m_channelCount; i += 3) {
        *(c++) = m_colorPattern[m_patternOffset];
        *(c++) = m_colorPattern[m_patternOffset + 1];
        *(c++) = m_colorPattern[m_patternOffset + 2];
    }
}

/*
 *
 */
void TestPatternRGBCycle::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "TestPatternRGBCycle::DumpConfig\n");
	LogDebug(VB_CHANNELOUT, "    colorPattern    : %s\n", m_colorPatternStr.c_str());
	LogDebug(VB_CHANNELOUT, "    colorPatternSize: %d\n", m_colorPatternSize);

	TestPatternBase::DumpConfig();
}

