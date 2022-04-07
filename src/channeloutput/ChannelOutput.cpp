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

#include <sys/time.h>
#include <errno.h>

#include "ChannelOutput.h"

ChannelOutput::ChannelOutput(unsigned int startChannel,
                             unsigned int channelCount) :
    m_outputType(""),
    m_startChannel(startChannel),
    m_channelCount(channelCount) {
}

ChannelOutput::~ChannelOutput() {
    LogDebug(VB_CHANNELOUT, "ChannelOutput::~ChannelOutput()\n");
}

int ChannelOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "ChannelOutput::Init(JSON)\n");
    m_outputType = config["type"].asString();
    if (m_channelCount == -1)
        m_channelCount = 0;

    DumpConfig();
    return 1;
}

int ChannelOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "ChannelOutput::Close()\n");
    return 1;
}

void ChannelOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "ChannelOutput::DumpConfig()\n");

    LogDebug(VB_CHANNELOUT, "    Output Type      : %s\n", m_outputType.c_str());
    LogDebug(VB_CHANNELOUT, "    Start Channel    : %u\n", m_startChannel + 1);
    LogDebug(VB_CHANNELOUT, "    Channel Count    : %u\n", m_channelCount);
}

void ChannelOutput::ConvertToCSV(Json::Value config, char* configStr) {
    Json::Value::Members memberNames = config.getMemberNames();

    configStr[0] = '\0';

    if (!config.isMember("type")) {
        strcpy(configStr, "");
        return;
    }

    if (config.isMember("enabled"))
        strcat(configStr, config["enabled"].asString().c_str());
    else
        strcat(configStr, "0");

    strcat(configStr, ",");

    strcat(configStr, config["type"].asString().c_str());
    strcat(configStr, ",");

    if (config.isMember("startChannel"))
        strcat(configStr, config["startChannel"].asString().c_str());
    else
        strcat(configStr, "1");

    strcat(configStr, ",");

    if (config.isMember("channelCount"))
        strcat(configStr, config["channelCount"].asString().c_str());
    else
        strcat(configStr, "1");

    std::string key;
    int first = 1;
    for (int i = 0; i < memberNames.size(); i++) {
        key = memberNames[i];

        if (first) {
            strcat(configStr, ",");
            first = 0;
        } else
            strcat(configStr, ";");

        strcat(configStr, key.c_str());
        strcat(configStr, "=");
        strcat(configStr, config[key].asString().c_str());
    }
}
