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

#include <cstdint>
#include <string>

#include "../Warnings.h"
#include "../log.h"


#include "UDMX.h"

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
        WarningHolder::AddWarning(28, "USBDMX: Error Starting LibUSB");
        return;
    }
}

UDMXOutput::~UDMXOutput() {
    LogDebug(VB_CHANNELOUT, "UDMXOutput::~UDMXOutput()\n");

    if (nullptr != m_device) {
        libusb_unref_device(m_device);
        m_device = nullptr;
    }
    if (nullptr != m_ctx) {
        libusb_exit(m_ctx);
        m_ctx = nullptr;
    }
}

std::string UDMXOutput::GetDevicePath(struct libusb_device* dev) const {
    // Matches the Linux sysfs device name, e.g. "1-1.2" (bus-port.port).
    std::string path = std::to_string(libusb_get_bus_number(dev));
    uint8_t ports[8];
    int n = libusb_get_port_numbers(dev, ports, sizeof(ports));
    for (int i = 0; i < n; i++) {
        path += (i == 0) ? "-" : ".";
        path += std::to_string(ports[i]);
    }
    return path;
}

std::string UDMXOutput::GetDeviceSerial(struct libusb_device* dev,
                                        const struct libusb_device_descriptor* desc) const {
    if (desc->iSerialNumber == 0) {
        return "";
    }
    // Reading the serial string descriptor requires briefly opening the device.
    libusb_device_handle* h = nullptr;
    if (libusb_open(dev, &h) != 0 || nullptr == h) {
        return "";
    }
    unsigned char buf[256] = { 0 };
    int r = libusb_get_string_descriptor_ascii(h, desc->iSerialNumber, buf, sizeof(buf) - 1);
    libusb_close(h);
    if (r <= 0) {
        return "";
    }
    return std::string(reinterpret_cast<char*>(buf));
}

void UDMXOutput::FindDevice(const std::string& wanted) {
    if (nullptr == m_ctx || nullptr != m_device) {
        return;
    }

    libusb_device** devices = nullptr;
    ssize_t count = libusb_get_device_list(m_ctx, &devices);
    libusb_device* firstMatch = nullptr;

    for (ssize_t i = 0; i < count; i++) {
        libusb_device* dev = devices[i];

        libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(dev, &desc) < 0) {
            continue;
        }
        if (!isValidDevice(&desc)) {
            continue;
        }

        if (nullptr == firstMatch) {
            firstMatch = dev;
        }

        if (wanted.empty()) {
            // No specific device requested (legacy configs) - first match wins.
            continue;
        }

        std::string serial = GetDeviceSerial(dev, &desc);
        std::string path = GetDevicePath(dev);
        if (wanted == serial || wanted == path) {
            LogDebug(VB_CHANNELOUT, "UDMX Device Found (serial '%s', path '%s')\n",
                     serial.c_str(), path.c_str());
            m_device = dev;
            break;
        }
    }

    if (nullptr == m_device && wanted.empty()) {
        // Backward compatible: auto-select the first uDMX found.
        m_device = firstMatch;
    }

    if (nullptr != m_device) {
        libusb_ref_device(m_device);
    }
    libusb_free_device_list(devices, 1);
}
void UDMXOutput::GetRequiredChannelRanges(const std::function<void(int, int)>& addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

int UDMXOutput::Init(Json::Value config) {
    LogDebug(VB_CHANNELOUT, "UDMXOutput::Init()\n");

    std::string wantedDevice = config.get("device", "").asString();
    FindDevice(wantedDevice);

    if (nullptr == m_device) {
        if (wantedDevice.empty()) {
            LogErr(VB_CHANNELOUT, "No UDMX Device Found\n");
            WarningHolder::AddWarning(28, "USBDMX: No UDMX Device Found");
        } else {
            LogErr(VB_CHANNELOUT, "Configured UDMX Device '%s' Not Found\n", wantedDevice.c_str());
            WarningHolder::AddWarning(28, "USBDMX: Configured uDMX Device '" + wantedDevice + "' Not Found");
        }
        return 0;
    }

    if (nullptr == m_handle) {
        int ret = libusb_open(m_device, &m_handle);
        if (ret < 0) {
            LogErr(VB_CHANNELOUT, "Error Opening USB Device (bus %d, device %d)",
                   libusb_get_bus_number(m_device), libusb_get_device_address(m_device));

            WarningHolder::AddWarning(28, "USBDMX: Error Opening USB Device");
            return 0;
        }
    }

    if (nullptr == m_handle) {
        LogErr(VB_CHANNELOUT, "USBDMX: Error Opening uDMX Device\n");
        WarningHolder::AddWarning(28, "USBDMX: Error Opening uDMX Device");
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
                                (uint8_t)LIBUSB_REQUEST_TYPE_VENDOR |
                                    (uint8_t)LIBUSB_RECIPIENT_INTERFACE |
                                    (uint8_t)LIBUSB_ENDPOINT_OUT,
                                UDMX_SET_CHANNEL_RANGE,                            /* Command */
                                m_channelCount,                                    /* Number of channels to set */
                                0,                                                 /* Starting index */
                                (unsigned char*)(m_outputData + m_dataOffset),    /* Values to set */
                                m_channelCount,                                    /* Size of values */
                                500);                                              /* Timeout 500ms */
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
