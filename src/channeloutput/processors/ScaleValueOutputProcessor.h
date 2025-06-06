#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "OutputProcessor.h"

class ScaleValueOutputProcessor : public OutputProcessor {
public:
    ScaleValueOutputProcessor(const Json::Value& config);
    virtual ~ScaleValueOutputProcessor();

    virtual void ProcessData(unsigned char* channelData) const override;

    virtual OutputProcessorType getType() const override { return SCALE; }

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override {
        addRange(start, start + count - 1);
    }

protected:
    int start;
    int count;
    float scale;
    std::string model;
    unsigned char table[256];
};
