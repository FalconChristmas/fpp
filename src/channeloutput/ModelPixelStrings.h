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

#include <mutex>
#include <vector>
#include "fpp-json-fwd.h"

#include "PixelString.h"
#include "ThreadedChannelOutput.h"
#include "overlays/PixelOverlayModel.h"

class ModelPixelStringsOutput : public ThreadedChannelOutput {
public:
    ModelPixelStringsOutput(unsigned int startChannel, unsigned int channelCount);
    ~ModelPixelStringsOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual void PrepData(unsigned char* channelData) override;
    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    std::string modelName;
    PixelOverlayModel* model = nullptr;

    // PrepData runs on the output thread and RawSendData on this output's
    // worker, so the frame being filled and the frame being handed to the model
    // have to be different allocations.  PrepData fills prepBuffer and swaps the
    // two under bufferLock; the worker reads buffer under the same lock.
    unsigned char* buffer = nullptr;
    unsigned char* prepBuffer = nullptr;
    std::mutex bufferLock;

    std::vector<PixelString*> strings;
};
