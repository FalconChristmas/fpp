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

class OverrideZeroOutputProcessor : public OutputProcessor {
public:
    OverrideZeroOutputProcessor(const Json::Value& config);
    virtual ~OverrideZeroOutputProcessor();

    virtual void ProcessData(unsigned char* channelData) const override;

    virtual OutputProcessorType getType() const override { return OVERRIDEZERO; }

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override {
        addRange(start, start + count - 1);
    }

protected:
    int start;
    int count;
    unsigned char value;
};
