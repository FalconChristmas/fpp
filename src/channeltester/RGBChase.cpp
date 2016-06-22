/*
 *   Channel Test Pattern RGB Chase class for Falcon Pi Player (FPP)
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

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "RGBChase.h"
#include "common.h"
#include "log.h"

/*
 *
 */
TestPatternRGBChase::TestPatternRGBChase()
  : m_colorPatternStr(""),
	m_colorPatternSize(0),
	m_patternOffset(0),
	m_rgbTriplets(0),
	m_lastTripletOffset(0),
	m_lastPatternOffset(0)
{
	LogExcess(VB_CHANNELOUT, "TestPatternRGBChase::TestPatternRGBChase()\n");

	m_testPatternName = "RGBChase";
}

/*
 *
 */
TestPatternRGBChase::~TestPatternRGBChase()
{
	LogExcess(VB_CHANNELOUT, "TestPatternRGBChase::~TestPatternRGBChase()\n");
}

/*
 *
 */
int TestPatternRGBChase::Init(Json::Value config)
{
	m_configChanged = 0;

	if (m_colorPatternStr != config["colorPattern"].asString())
	{
		m_colorPatternStr = config["colorPattern"].asString();
		m_configChanged = 1;
	}

	return TestPatternBase::Init(config);
}

/*
 *
 */
int TestPatternRGBChase::SetupTest(void)
{
	bzero(m_testData, m_channelCount);

	m_colorPattern.clear();

	char digit = 0;
	for (int i = 0; i < m_colorPatternStr.size(); i += 2)
	{
		digit = (char)strtol(m_colorPatternStr.substr(i,2).c_str(), NULL, 16);
		m_colorPattern.push_back(digit);
	}

	// Make sure we have a valid set of triplets
	while (m_colorPattern.size() % 3)
		m_colorPattern.push_back(0);

	char *c = m_testData;
	int   offset = 0;
	for (int i = 0; i < m_channelCount; i += 3)
	{
		*(c++) = m_colorPattern[offset++];
		*(c++) = m_colorPattern[offset++];
		*(c++) = m_colorPattern[offset++];

		if (offset >= m_colorPattern.size())
			offset = 0;
	}

	if (m_channelCount < 6)
		m_channelCount = 6;

	m_patternOffset = 0;
	m_colorPatternSize = m_colorPattern.size() / 3;
	m_rgbTriplets = m_channelCount / 3;
	m_lastTripletOffset = (m_rgbTriplets - 1) * 3;
	m_lastPatternOffset = m_lastTripletOffset % m_colorPattern.size();

	return TestPatternBase::SetupTest();
}

/*
 *
 */
void TestPatternRGBChase::CycleData(void)
{
	memmove(m_testData + 3, m_testData, m_channelCount - 3);

	m_patternOffset -= 3;

	if (m_patternOffset < 0)
		m_patternOffset = m_colorPattern.size() - 3;

	int offset = m_patternOffset;

	m_testData[0] = m_colorPattern[offset++];
	m_testData[1] = m_colorPattern[offset++];
	m_testData[2] = m_colorPattern[offset];
}

/*
 *
 */
void TestPatternRGBChase::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "TestPatternRGBChase::DumpConfig\n");
	LogDebug(VB_CHANNELOUT, "    colorPattern    : %s\n", m_colorPatternStr.c_str());
	LogDebug(VB_CHANNELOUT, "    colorPatternSize: %d\n", m_colorPatternSize);

	TestPatternBase::DumpConfig();
}

