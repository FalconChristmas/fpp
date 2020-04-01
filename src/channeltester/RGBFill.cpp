/*
 *   Channel Test Pattern RGB Fill class for Falcon Player (FPP)
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
#include "fpp-pch.h"

#include "RGBFill.h"

/*
 *
 */
TestPatternRGBFill::TestPatternRGBFill()
  : m_color1(0),
	m_color2(0),
	m_color3(0)
{
	LogExcess(VB_CHANNELOUT, "TestPatternRGBFill::TestPatternRGBFill()\n");

	m_testPatternName = "RGBFill";
}

/*
 *
 */
TestPatternRGBFill::~TestPatternRGBFill()
{
	LogExcess(VB_CHANNELOUT, "TestPatternRGBFill::~TestPatternRGBFill()\n");
}

/*
 *
 */
int TestPatternRGBFill::Init(Json::Value config)
{
	m_configChanged = 0;

	if (m_color1 != config["color1"].asInt())
	{
		m_color1 = config["color1"].asInt();
		m_configChanged = 1;
	}

	if (m_color2 != config["color2"].asInt())
	{
		m_color2 = config["color2"].asInt();
		m_configChanged = 1;
	}

	if (m_color3 != config["color3"].asInt())
	{
		m_color3 = config["color3"].asInt();
		m_configChanged = 1;
	}

	return TestPatternBase::Init(config);
}

/*
 *
 */
int TestPatternRGBFill::SetupTest(void)
{
	bzero(m_testData, m_channelCount);

	char *c = m_testData;
	int   offset = 0;
	for (int i = 0; i < m_channelCount; i += 3)
	{
		*(c++) = m_color1;
		*(c++) = m_color2;
		*(c++) = m_color3;
	}

	return TestPatternBase::SetupTest();
}

/*
 *
 */
void TestPatternRGBFill::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "TestPatternRGBFill::DumpConfig\n");
	LogDebug(VB_CHANNELOUT, "    color1 : %02x\n", m_color1);
	LogDebug(VB_CHANNELOUT, "    color2 : %02x\n", m_color2);
	LogDebug(VB_CHANNELOUT, "    color3 : %02x\n", m_color3);

	TestPatternBase::DumpConfig();
}

