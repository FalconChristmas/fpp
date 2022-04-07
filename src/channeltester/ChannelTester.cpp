/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "ChannelTester.h"

// Test Patterns
#include "RGBChase.h"
#include "RGBCycle.h"
#include "RGBFill.h"
#include "SingleChase.h"

ChannelTester ChannelTester::INSTANCE;

/*
 *
 */
ChannelTester::ChannelTester() :
    m_testPattern(NULL) {
    LogExcess(VB_CHANNELOUT, "ChannelTester::ChannelTester()\n");

    pthread_mutex_init(&m_testLock, NULL);
}

/*
 *
 */
ChannelTester::~ChannelTester() {
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
int ChannelTester::SetupTest(std::string configStr) {
    LogDebug(VB_CHANNELOUT, "ChannelTester::SetupTest()\n");
    LogDebug(VB_CHANNELOUT, "     %s\n", configStr.c_str());

    Json::Value config;
    int result = 0;
    std::string patternName;

    if (!LoadJsonFromString(configStr, config)) {
        LogErr(VB_CHANNELOUT,
               "Error parsing Test Pattern config string: '%s'\n",
               configStr.c_str());

        return 0;
    }

    pthread_mutex_lock(&m_testLock);

    if (config["enabled"].asInt()) {
        patternName = config["mode"].asString();

        if (m_testPattern) {
            if (patternName != m_testPattern->Name()) {
                delete m_testPattern;
                m_testPattern = NULL;
            }
        }

        if (!m_testPattern) {
            if (patternName == "SingleChase")
                m_testPattern = new TestPatternSingleChase();
            else if (patternName == "RGBChase")
                m_testPattern = new TestPatternRGBChase();
            else if (patternName == "RGBFill")
                m_testPattern = new TestPatternRGBFill();
            else if (patternName == "RGBCycle")
                m_testPattern = new TestPatternRGBCycle();
        }

        if (m_testPattern) {
            result = m_testPattern->Init(config);
            if (!result) {
                delete m_testPattern;
                m_testPattern = NULL;
            }
        }
    } else {
        if (m_testPattern) {
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
void ChannelTester::OverlayTestData(char* channelData) {
    LogExcess(VB_CHANNELOUT, "ChannelTester::OverlayTestData()\n");

    pthread_mutex_lock(&m_testLock);

    if (!m_testPattern) {
        pthread_mutex_unlock(&m_testLock);
        return;
    }

    m_testPattern->OverlayTestData(channelData);

    pthread_mutex_unlock(&m_testLock);
}

/*
 *
 */
int ChannelTester::Testing(void) {
    return m_testPattern ? 1 : 0;
}

/*
 *
 */
std::string ChannelTester::GetConfig(void) {
    if (!m_testPattern)
        return std::string("{ \"enabled\": 0 }");
    return m_configStr;
}
