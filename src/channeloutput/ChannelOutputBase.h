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

#ifndef _CHANNELOUTPUTBASE_H
#define _CHANNELOUTPUTBASE_H

#include <string>
#include <vector>
#include <functional>

#include <jsoncpp/json/json.h>


class ChannelOutputBase {
  public:
	ChannelOutputBase(unsigned int startChannel = 1,
		unsigned int channelCount = 1);
	virtual ~ChannelOutputBase();

	unsigned int  ChannelCount(void) { return m_channelCount; }
	unsigned int  StartChannel(void) { return m_startChannel; }

    virtual int   Init(Json::Value config);
    virtual int   Close(void);
    
    virtual void  PrepData(unsigned char *channelData) {}
	virtual int   SendData(unsigned char *channelData) = 0;


    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) = 0;

  protected:
	virtual void  DumpConfig(void);
    virtual void  ConvertToCSV(Json::Value config, char *configStr);

	std::string      m_outputType;
	unsigned int     m_startChannel;
	unsigned int     m_channelCount;
};

#endif /* #ifndef _CHANNELOUTPUTBASE_H */
