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

    Json::Value callRestAPI(bool isPost, const std::string& path, const std::string& body);
    void verifyToken();
    void authenticate();

    int port = 1;
    int portCount = 1;

    sockaddr_in twinklyAddress;

    struct iovec* twinklyIovecs = nullptr;
    uint8_t** twinklyBuffers = nullptr;

    uint8_t authTokenBytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    std::string authToken = "";
    uint32_t reauthCount = 0;
};
