/*
 *   DDP Channel Output driver for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
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

#ifndef _DDPOUTPUT_H
#define _DDPOUTPUT_H

#include <list>
#include "UDPOutput.h"

#define DDP_PORT 4048

#define DDP_PUSH_FLAG 0x01
#define DDP_TIMECODE_FLAG 0x10


class DDPOutputData : public UDPOutputData {
public:
    explicit DDPOutputData(const Json::Value &config);
    virtual ~DDPOutputData();
    
    virtual bool IsPingable() override { return true; }
    virtual void PrepareData(unsigned char *channelData, UDPOutputMessages &msgs) override;
    virtual void DumpConfig() override;
    
    virtual const std::string &GetOutputTypeString() const override;

    char          sequenceNumber;
    
    sockaddr_in   ddpAddress;
    int           pktCount;
    
    struct iovec *ddpIovecs = nullptr;
    unsigned char **ddpBuffers = nullptr;
};


#endif
