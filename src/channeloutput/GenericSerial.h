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

#define GENERICSERIAL_MAX_CHANNELS 2048

class GenericSerialOutput : public ThreadedChannelOutput, SerialChannelOutput {
public:
    GenericSerialOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~GenericSerialOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    int m_speed;
    int m_headerSize;
    std::string m_header;
    int m_footerSize;
    std::string m_footer;
    int m_packetSize;
    char* m_data;
};
