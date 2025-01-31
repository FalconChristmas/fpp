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

#include <string>

#include "SerialChannelOutput.h"
#include "ThreadedChannelOutput.h"

class USBDMXOutput : public ThreadedChannelOutput, public SerialChannelOutput {
public:
    USBDMXOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~USBDMXOutput();

    virtual int Init(Json::Value config) override;

    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;
    virtual void WaitTimedOut() override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    enum DongleType {
        DMX_DVC_UNKNOWN,
        DMX_DVC_PRO,
        DMX_DVC_OPEN
    };

    DongleType m_dongleType;

    char m_outputData[513 + 6];
    int m_dataOffset;
    int m_dataLen;
};
