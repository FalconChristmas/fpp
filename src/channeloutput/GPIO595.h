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

#include "ThreadedChannelOutput.h"
#include "util/GPIOUtils.h"

class GPIO595Output : public ThreadedChannelOutput {
public:
    GPIO595Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~GPIO595Output();

    virtual int Init(Json::Value config) override;

    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override {
        addRange(m_startChannel, m_startChannel + m_channelCount - 1);
    }

private:
    const PinCapabilities* m_clockPin;
    const PinCapabilities* m_dataPin;
    const PinCapabilities* m_latchPin;
};
