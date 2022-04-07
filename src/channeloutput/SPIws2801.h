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
#include "util/SPIUtils.h"

class SPIws2801Output : public ThreadedChannelOutput {
public:
    SPIws2801Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~SPIws2801Output();

    virtual int Init(Json::Value config) override;

    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    SPIUtils* m_spi;
    int m_port;
    int m_pi36;
    unsigned char* m_pi36Data;
    int m_pi36DataSize;
};
