#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "UDPOutput.h"
#include <list>

#define KINET_PORT 6038

class KiNetOutputData : public UDPOutputData {
public:
    explicit KiNetOutputData(const Json::Value& config);
    virtual ~KiNetOutputData();

    virtual bool IsPingable() override { return true; }
    virtual void PrepareData(unsigned char* channelData, UDPOutputMessages& msgs) override;
    virtual void DumpConfig() override;

    virtual const std::string& GetOutputTypeString() const override;
    virtual void GetRequiredChannelRange(int& min, int& max) override;

    int port = 1;
    int portCount = 1;

    sockaddr_in kinetAddress;

    struct iovec* kinetIovecs = nullptr;
    unsigned char** kinetBuffers = nullptr;
};
