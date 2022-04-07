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

#define DDP_PORT 4048

#define DDP_PUSH_FLAG 0x01
#define DDP_TIMECODE_FLAG 0x10

class DDPOutputData : public UDPOutputData {
public:
    explicit DDPOutputData(const Json::Value& config);
    virtual ~DDPOutputData();

    virtual bool IsPingable() override { return true; }
    virtual void PrepareData(unsigned char* channelData, UDPOutputMessages& msgs) override;
    virtual void DumpConfig() override;

    virtual const std::string& GetOutputTypeString() const override;

    char sequenceNumber;

    sockaddr_in ddpAddress;
    int pktCount;

    struct iovec* ddpIovecs = nullptr;
    unsigned char** ddpBuffers = nullptr;
};
