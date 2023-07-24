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

#include "fpp-pch.h"

#include <fcntl.h>
#include <termios.h>

#include "../Warnings.h"
#include "../log.h"


#include "uDMX.h"

#include <libusb-1.0/libusb.h>

#define DMX_MAX_CHANNELS 512

#define UDMX_SHARED_VENDOR 0x16C0  /* VOTI */
#define UDMX_SHARED_PRODUCT 0x05DC /* Obdev's free shared PID */

#define UDMX_AVLDIY_D512_CLONE_VENDOR 0x03EB  /* Atmel Corp. (Clone VID)*/
#define UDMX_AVLDIY_D512_CLONE_PRODUCT 0x8888 /* Clone PID */
#define UDMX_SET_CHANNEL_RANGE 0x0002         /* Command to set n channel values */

#include "Plugin.h"
class UDMXPlugin : public FPPPlugins::Plugin, public FPPPlugins::ChannelOutputPlugin {
public:
    UDMXPlugin() :
        FPPPlugins::Plugin("UDMX") {
    }
    virtual ChannelOutput* createChannelOutput(unsigned int startChannel, unsigned int channelCount) override {
        return new UDMXOutput(startChannel, channelCount);
    }
};

extern "C" {
FPPPlugins::Plugin* createPlugin() {
    return new UDMXPlugin();
}
}

/////////////////////////////////////////////////////////////////////////////

UDMXOutput::UDMXOutput(unsigned int startChannel, unsigned int channelCount) :
    ThreadedChannelOutput(startChannel, channelCount) {
    LogDebug(VB_CHANNELOUT, "UDMXOutput::UDMXOutput(%u, %u)\n",
             startChannel, channelCount);

    m_useDoubleBuffer = 1;
    //DMX protocol requires data to be sent at least every 250ms
    m_maxWait = 250;
    m_dataOffset = 1;
    memset(m_outputData, 0, sizeof(m_outputData));

    if (libusb_init(&m_ctx) != 0) {
        LogErr(VB_CHANNELOUT, "Error Starting LibUSB\n");
        WarningHolder::AddWarning("USBDMX: Error Starting LibUSB");
        return;
    }

    libusb_device** devices = nullptr;
    ssize_t count = libusb_get_device_list(m_ctx, &devices);
    for (ssize_t i = 0; i < count; i++) {
        libusb_device* dev = devices[i];

        libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            continue;
        }

        if (isValidDevice(&desc)) {
            m_device = dev;
        }
    }
}

UDMXOutput::~UDMXOutput() {
    LogDebug(VB_CHANNELOUT, "UDMXOutput::~UDMXOutput()\n");
}
void UDMXOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

int UDMXOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "UDMXOutput::Init()\n");

    m_dataLen = m_channelCount + 1;

    if (nullptr != m_device && nullptr == m_handle) {
        int ret = libusb_open(m_device, &m_handle);
        if (ret < 0) {
            LogErr(VB_CHANNELOUT, "Error Opening USB Device (bus %d, device %d)",
                   libusb_get_bus_number(m_device), libusb_get_device_address(m_device));

            WarningHolder::AddWarning("USBDMX: Error Opening USB Device");
            return 0;
        }
    }

    if (nullptr == m_handle) {
        LogErr(VB_CHANNELOUT, "USBDMX: Error Opening uDMX Device\n");
        WarningHolder::AddWarning("USBDMX: Error Opening uDMX Device");
        return 0;
    }
    return ThreadedChannelOutput::Init(config);
}

int UDMXOutput::Close(void) {
    LogDebug(VB_CHANNELOUT, "UDMXOutput::Close()\n");

    if (nullptr != m_device && nullptr != m_handle) {
        libusb_close(m_handle);
    }

    m_handle = nullptr;

    return ThreadedChannelOutput::Close();
}

int UDMXOutput::RawSendData(unsigned char* channelData) {
    LogExcess(VB_CHANNELOUT, "UDMXOutput::RawSendData(%p)\n", channelData);
    memcpy(m_outputData + m_dataOffset, channelData, m_channelCount);
    WaitTimedOut();
    return m_channelCount;
}
void UDMXOutput::WaitTimedOut() {
    if (nullptr != m_device && nullptr != m_handle) {
        libusb_control_transfer(m_handle,
                                LIBUSB_REQUEST_TYPE_VENDOR |
                                    LIBUSB_RECIPIENT_INTERFACE |
                                    LIBUSB_ENDPOINT_OUT,
                                UDMX_SET_CHANNEL_RANGE,       /* Command */
                                m_dataLen,                    /* Number of channels to set */
                                0,                            /* Starting index */
                                (unsigned char*)m_outputData, /* Values to set */
                                m_dataLen,                    /* Size of values */
                                500);                         /* Timeout 500ms */
    }
}

void UDMXOutput::DumpConfig(void) {
    LogDebug(VB_CHANNELOUT, "UDMXOutput::DumpConfig()\n");
    ThreadedChannelOutput::DumpConfig();
}

bool UDMXOutput::isValidDevice(const struct libusb_device_descriptor* desc) const {
    if (nullptr == desc) {
        return false;
    }

    if (desc->idVendor != UDMX_SHARED_VENDOR &&
        desc->idVendor != UDMX_AVLDIY_D512_CLONE_VENDOR) {
        return false;
    }

    if (desc->idProduct != UDMX_SHARED_PRODUCT &&
        desc->idProduct != UDMX_AVLDIY_D512_CLONE_PRODUCT) {
        return false;
    }

    return true;
}
