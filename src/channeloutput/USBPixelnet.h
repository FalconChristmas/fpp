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
class USBPixelnetOutput : public ThreadedChannelOutput, public SerialChannelOutput {
public:
    USBPixelnetOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~USBPixelnetOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:
    enum DongleType {
        PIXELNET_DVC_UNKNOWN,
        PIXELNET_DVC_LYNX,
        PIXELNET_DVC_OPEN
    };

    unsigned char m_rawData[4104]; // Sized to a multiple of 8 bytes
    int m_outputPacketSize;        // Header size + 4096 data bytes
    unsigned char* m_outputData;
    unsigned char* m_pixelnetData;
    DongleType m_dongleType;
};
