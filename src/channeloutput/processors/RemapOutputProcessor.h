#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "OutputProcessor.h"

class RemapOutputProcessor : public OutputProcessor {
public:
    RemapOutputProcessor(const Json::Value& config);
    RemapOutputProcessor(int src, int dst, int count, int loop, int reverse);
    virtual ~RemapOutputProcessor();

    virtual void ProcessData(unsigned char* channelData) const override;

    virtual OutputProcessorType getType() const override { return REMAP; }

    int getSourceChannel() const { return sourceChannel; }
    int getDestChannel() const { return destChannel; }
    int getCount() const { return count; }
    int getLoops() const { return loops; }
    int getReverse() const { return reverse; }

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

protected:
    int sourceChannel;
    int destChannel;
    int count;
    int loops;
    int reverse;
};
