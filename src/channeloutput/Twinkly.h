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
#include "fpp-json-fwd.h"
#include <atomic>
#include <list>
#include <mutex>

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

    // Stages a freshly issued token for the output thread to pick up. Returns
    // false if it could not be decoded, in which case nothing was changed.
    bool applyAuthToken(const std::string& token);

    // Ends an authentication attempt and starts the one that arrived while it
    // was running, if any.
    void finishAuth();

    std::string getAuthToken();
    void setAuthToken(const std::string& token);

    int port = 1;
    int portCount = 1;

    sockaddr_in twinklyAddress;

    struct iovec* twinklyIovecs = nullptr;
    uint8_t** twinklyBuffers = nullptr;

    // The authentication chain runs on the main loop (CurlManager/Timers) while
    // PrepareData(), StartingOutput() and StoppingOutput() run on the channel
    // output thread, so everything they share is either guarded by authLock or
    // an atomic.
    std::mutex authLock;
    uint8_t authTokenBytes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 }; // staged token, guarded by authLock
    std::string authToken = "";                             // guarded by authLock
    uint32_t reauthCount = 0;                               // output thread only
    std::atomic<bool> authInFlight{ false };
    std::atomic<bool> authPending{ false };   // a request came in while one was in flight
    std::atomic<bool> tokenPending{ false };  // a staged token is waiting for the output thread
    std::atomic<bool> outputStarted{ false }; // false once StoppingOutput() has run
};
