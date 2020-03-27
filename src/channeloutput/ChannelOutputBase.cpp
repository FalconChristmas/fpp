/*
 *   ChannelOutputBase class for Falcon Player (FPP)
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

#include <vector>

#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "ChannelOutputBase.h"
#include "common.h"
#include "log.h"

ChannelOutputBase::ChannelOutputBase(unsigned int startChannel,
	 unsigned int channelCount)
  : m_outputType(""),
	m_startChannel(startChannel),
	m_channelCount(channelCount)
{
}

ChannelOutputBase::~ChannelOutputBase()
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::~ChannelOutputBase()\n");
}

int ChannelOutputBase::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Init(JSON)\n");
	m_outputType = config["type"].asString();
    if (m_channelCount == -1)
        m_channelCount = 0;

    DumpConfig();
    return 1;
}

int ChannelOutputBase::Close(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::Close()\n");
	return 1;
}

void ChannelOutputBase::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "ChannelOutputBase::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Output Type      : %s\n", m_outputType.c_str());
	LogDebug(VB_CHANNELOUT, "    Start Channel    : %u\n", m_startChannel + 1);
	LogDebug(VB_CHANNELOUT, "    Channel Count    : %u\n", m_channelCount);
}


void ChannelOutputBase::ConvertToCSV(Json::Value config, char *configStr) {
    Json::Value::Members memberNames = config.getMemberNames();
    
    configStr[0] = '\0';
    
    if (!config.isMember("type"))
    {
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
    for (int i = 0; i < memberNames.size(); i++)
    {
        key = memberNames[i];
        
        if (first)
        {
            strcat(configStr, ",");
            first = 0;
        }
        else
            strcat(configStr, ";");
        
        strcat(configStr, key.c_str());
        strcat(configStr, "=");
        strcat(configStr, config[key].asString().c_str());
    }
}
