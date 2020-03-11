/*
 *   Channel Test code for Falcon Player (FPP)
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
#include <unistd.h>

#include <jsoncpp/json/json.h>

#include "ChannelTester.h"
#include "common.h"
#include "log.h"

// Test Patterns
#include "RGBChase.h"
#include "RGBCycle.h"
#include "RGBFill.h"
#include "SingleChase.h"


ChannelTester ChannelTester::INSTANCE;


/*
 *
 */
ChannelTester::ChannelTester()
  : m_testPattern(NULL)
{
	LogExcess(VB_CHANNELOUT, "ChannelTester::ChannelTester()\n");

	pthread_mutex_init(&m_testLock, NULL);
}

/*
 *
 */
ChannelTester::~ChannelTester()
{
	LogExcess(VB_CHANNELOUT, "ChannelTester::~ChannelTester()\n");

	pthread_mutex_lock(&m_testLock);

	if (m_testPattern) {
		delete m_testPattern;
		m_testPattern = NULL;
	}

	pthread_mutex_unlock(&m_testLock);

	pthread_mutex_destroy(&m_testLock);
}

/*
 *
 */
int ChannelTester::SetupTest(std::string configStr)
{
	LogDebug(VB_CHANNELOUT, "ChannelTester::SetupTest()\n");
    LogDebug(VB_CHANNELOUT, "     %s\n", configStr.c_str());

	Json::Value config;
	int result = 0;
	std::string patternName;

	if (!LoadJsonFromString(configStr, config))
	{
		LogErr(VB_CHANNELOUT,
			"Error parsing Test Pattern config string: '%s'\n",
			configStr.c_str());

		return 0;
	}

	pthread_mutex_lock(&m_testLock);

	if (config["enabled"].asInt())
	{
		patternName = config["mode"].asString();

		if (m_testPattern)
		{
			if (patternName != m_testPattern->Name())
			{
				delete m_testPattern;
				m_testPattern = NULL;
			}
		}

		if (!m_testPattern)
		{
			if (patternName == "SingleChase")
				m_testPattern = new TestPatternSingleChase();
			else if (patternName == "RGBChase")
				m_testPattern = new TestPatternRGBChase();
			else if (patternName == "RGBFill")
				m_testPattern = new TestPatternRGBFill();
			else if (patternName == "RGBCycle")
				m_testPattern = new TestPatternRGBCycle();
		}

		if (m_testPattern)
		{
			result = m_testPattern->Init(config);
			if (!result)
			{
				delete m_testPattern;
				m_testPattern = NULL;
			}
		}
	}
	else
	{
		if (m_testPattern)
		{
			m_testPattern->DisableTest();
			pthread_mutex_unlock(&m_testLock);

			// Give the channel output loop time to clear test data
			usleep(150000);

			pthread_mutex_lock(&m_testLock);

			delete m_testPattern;
			m_testPattern = NULL;
		}
	}

	pthread_mutex_unlock(&m_testLock);

	m_configStr = configStr;

	return result;
}

/*
 *
 */
void ChannelTester::OverlayTestData(char *channelData)
{
	LogExcess(VB_CHANNELOUT, "ChannelTester::OverlayTestData()\n");

	pthread_mutex_lock(&m_testLock);

	if (!m_testPattern)
	{
		pthread_mutex_unlock(&m_testLock);
		return;
	}

	m_testPattern->OverlayTestData(channelData);

	pthread_mutex_unlock(&m_testLock);
}

/*
 *
 */
int ChannelTester::Testing(void)
{
	return m_testPattern ? 1 : 0;
}

/*
 *
 */
std::string ChannelTester::GetConfig(void)
{
	if (!m_testPattern)
		return std::string("{ \"enabled\": 0 }");
	return m_configStr;
}

