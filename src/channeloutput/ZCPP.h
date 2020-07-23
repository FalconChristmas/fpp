#pragma once
/*
 *   ZCPP Channel Output driver for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2020 the Falcon Player Developers
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

#include <list>
#include "UDPOutput.h"

#define ZCPP_PORT 30005

// Packet Type Codes
#define ZCPP_TYPE_DISCOVERY 0x00
#define ZCPP_TYPE_DISCOVERY_RESPONSE 0x01
#define ZCPP_TYPE_CONFIG 0x0A
#define ZCPP_TYPE_EXTRA_DATA 0x0B
#define ZCPP_TYPE_QUERY_CONFIG 0x0C
#define ZCPP_TYPE_QUERY_CONFIG_RESPONSE 0x0D
#define ZCPP_TYPE_DATA 0x14
#define ZCPP_TYPE_SYNC 0x15

#define ZCPP_CURRENT_PROTOCOL_VERSION 0x00

#define ZCPP_DATA_FLAG_FIRST 0x40
#define ZCPP_DATA_FLAG_LAST 0x80


class ZCPPOutputData : public UDPOutputData {
public:
    explicit ZCPPOutputData(const Json::Value &config);
    virtual ~ZCPPOutputData();
    
    virtual bool IsPingable() override { return true; }
    virtual void PrepareData(unsigned char *channelData, UDPOutputMessages &msgs) override;
    virtual void DumpConfig() override;
    
    virtual const std::string &GetOutputTypeString() const override;

    char          sequenceNumber;
    int           priority;
    
    sockaddr_in   zcppAddress;
    int           pktCount;
    
    struct iovec *zcppIovecs = nullptr;
    unsigned char **zcppBuffers = nullptr;
};
