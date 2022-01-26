#pragma once
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

#include "UDPOutput.h"
#include <list>

#define TWINKLY_PORT 7777

class TwinklyOutputData : public UDPOutputData {
public:
    explicit TwinklyOutputData(const Json::Value& config);
    virtual ~TwinklyOutputData();

    virtual bool IsPingable() override { return true; }
    virtual void PrepareData(unsigned char* channelData, UDPOutputMessages& msgs) override;
    virtual void DumpConfig() override;

    virtual const std::string& GetOutputTypeString() const override;
    virtual void GetRequiredChannelRange(int& min, int& max) override;

    virtual void StartingOutput() override;
    virtual void StoppingOutput() override;
    
    
    Json::Value callRestAPI(bool isPost, const std::string &path, const std::string &body);
    
    int port = 1;
    int portCount = 1;

    sockaddr_in twinklyAddress;

    struct iovec* twinklyIovecs = nullptr;
    uint8_t** twinklyBuffers = nullptr;
    
    uint8_t authTokenBytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    std::string authToken = "";
};
