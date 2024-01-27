#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2024 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include <map>
#include <list>

#include "ChannelOutput.h"

class ControlChannelOutput : public ChannelOutput {
public:

    ControlChannelOutput(unsigned int startChannel, unsigned int channelCount);
    ~ControlChannelOutput();

    virtual int Init(Json::Value config) override;

    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override {
        addRange(m_startChannel, m_startChannel);
    }

private:
    std::map<uint8_t, std::list<std::string>> presets;
    uint8_t lastValue = 0;
};