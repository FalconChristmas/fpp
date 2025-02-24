#ifndef _USBRELAY_H
#define _USBRELAY_H

#include "ChannelOutput.h"
// Remove #include "SerialChannelOutput.h" since serialutil.h handles serial ops

typedef enum {
    RELAY_DVC_UNKNOWN = 0,
    RELAY_DVC_BIT,
    RELAY_DVC_ICSTATION,
    RELAY_DVC_LCUS1
} RelayDeviceSubType;

class USBRelayOutput : public ChannelOutput {
public:
    USBRelayOutput(unsigned int startChannel, unsigned int channelCount);
    virtual ~USBRelayOutput();

    virtual int Init(Json::Value config) override;
    virtual int Close(void) override;
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) override;
    virtual int SendData(unsigned char* channelData) override;
    virtual void DumpConfig(void) override;

private:
    RelayDeviceSubType m_subType;
    int m_relayCount;
    int m_fd;  // Add file descriptor for serial port, since SerialChannelOutput isn’t providing it
};

#endif