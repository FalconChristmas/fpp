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

#include <string.h>
#include <strings.h>

#include "SingleChase.h"
#include "common.h"
#include "log.h"

/*
 *
 */
TestPatternSingleChase::TestPatternSingleChase()
  : m_chaseSize(0),
	m_chaseValue(255)
{
	LogExcess(VB_CHANNELOUT, "TestPatternSingleChase::TestPatternSingleChase()\n");
	m_testPatternName = "SingleChase";
}

/*
 *
 */
TestPatternSingleChase::~TestPatternSingleChase()
{
	LogExcess(VB_CHANNELOUT, "TestPatternSingleChase::~TestPatternSingleChase()\n");
}

/*
 *
 */
int TestPatternSingleChase::Init(Json::Value config)
{
	m_configChanged = 0;

	if (m_chaseSize != config["chaseSize"].asInt())
	{
		m_chaseSize = config["chaseSize"].asInt();
		m_configChanged = 1;
	}

	if (m_chaseValue != config["chaseValue"].asInt())
	{
		m_chaseValue = config["chaseValue"].asInt();
		m_configChanged = 1;
	}

	return TestPatternBase::Init(config);
}

/*
 *
 */
int TestPatternSingleChase::SetupTest(void)
{
	char *c = m_testData;
	bzero(m_testData, m_channelCount);

	for (int i = 0; i < m_channelCount; i += m_chaseSize)
	{
		*c = m_chaseValue;
		c += m_chaseSize;
	}

	return TestPatternBase::SetupTest();
}

/*
 *
 */
void TestPatternSingleChase::CycleData(void)
{
	memmove(m_testData + 1, m_testData, m_channelCount - 1);

	if (m_testData[m_chaseSize] == m_chaseValue)
		m_testData[0] = m_chaseValue;
	else
		m_testData[0] = 0x00;
}

/*
 *
 */
void TestPatternSingleChase::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "TestPatternSingleChase::DumpConfig\n");
	LogDebug(VB_CHANNELOUT, "    chaseSize   : %d\n", m_chaseSize);
	LogDebug(VB_CHANNELOUT, "    chaseValue  : %d (0x%x)\n",
		(unsigned char)m_chaseValue, (unsigned char)m_chaseValue);

	TestPatternBase::DumpConfig();
}

