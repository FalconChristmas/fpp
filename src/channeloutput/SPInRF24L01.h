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

#include "ChannelOutput.h"

class SPInRF24L01PrivData;

class SPInRF24L01Output : public ChannelOutput {
public:
    SPInRF24L01Output(unsigned int startChannel, unsigned int channelCount);
    virtual ~SPInRF24L01Output();

    virtual int Init(Json::Value config) override;

    virtual int Close(void) override;

    virtual int SendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    SPInRF24L01PrivData* data;
};
