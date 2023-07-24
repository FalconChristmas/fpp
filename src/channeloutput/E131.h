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

#include <netinet/in.h>
#include <sys/uio.h>
#include <vector>

#include "UDPOutput.h"
#include "../e131defs.h"

class E131OutputData : public UDPOutputData {
public:
    explicit E131OutputData(const Json::Value& config);
    virtual ~E131OutputData();

    virtual bool IsPingable() override;

    virtual void PrepareData(unsigned char* channelData, UDPOutputMessages& msgs) override;

    virtual void DumpConfig() override;
    virtual void GetRequiredChannelRange(int& min, int& max) override;

    virtual const std::string& GetOutputTypeString() const override;

    int universe;
    int universeCount;
    int priority;

    std::vector<sockaddr_in> e131Addresses;
    std::vector<struct iovec> e131Iovecs;
    std::vector<unsigned char*> e131Headers;
};
