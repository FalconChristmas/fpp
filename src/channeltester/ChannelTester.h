#pragma once
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

#include <string>

#include <pthread.h>

#include "TestPatternBase.h"

#include <httpserver.hpp>

class ChannelTester : public httpserver::http_resource {
public:
    ChannelTester();
    virtual ~ChannelTester();

    int SetupTest(const std::string configStr);
    int SetupTest(const Json::Value& config);
    void StopTest();

    void OverlayTestData(char* channelData);
    int Testing(void);

    std::string GetConfig(void);

    void RegisterCommands();
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request& req) override;
    virtual HTTP_RESPONSE_CONST std::shared_ptr<httpserver::http_response> render_POST(const httpserver::http_request& req) override;

    static ChannelTester INSTANCE;

private:
    TestPatternBase* m_testPattern;
    pthread_mutex_t m_testLock;
    std::string m_configStr;
};
