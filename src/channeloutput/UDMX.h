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

#include "ThreadedChannelOutput.h"

class UDMXOutput : public ThreadedChannelOutput {
public:
    UDMXOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~UDMXOutput();

    virtual int Init(Json::Value config) override;

    virtual int Close(void) override;

    virtual int RawSendData(unsigned char* channelData) override;
    virtual void WaitTimedOut() override;

    virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;

private:

    char m_outputData[513];
    int m_dataOffset;
    int m_dataLen;

    struct libusb_context* m_ctx{nullptr};
    struct libusb_device* m_device{nullptr};
    //struct libusb_device_descriptor *m_descriptor{nullptr};
    struct libusb_device_handle* m_handle{nullptr};
    #pragma endregion 

    bool isValidDevice(const struct libusb_device_descriptor* desc) const;
};
