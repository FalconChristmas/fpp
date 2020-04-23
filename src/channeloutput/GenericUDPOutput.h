#pragma once
/*
 *   Generic Binary IP Channel Output driver for Falcon Player (FPP)
 *
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
#include "ChannelOutputBase.h"


class GenericUDPOutput : public ChannelOutputBase {
public:
    GenericUDPOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~GenericUDPOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;
    virtual int SendData(unsigned char *channelData) override;
    
    
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;
private:
};
