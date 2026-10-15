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

#include "fpp-json.h"

#include "../Warnings.h"
#include "../config.h"
#include "../log.h"
#include <unistd.h>

#include "SerialChannelOutput.h"
#include "serialutil.h"
#include "../util/GPIOUtils.h"

#ifdef HAS_CAPEUTILS
#include "../non-gpl/CapeUtils/CapeUtils.h"
#endif

SerialChannelOutput::SerialChannelOutput() :
    m_deviceName("UNKNOWN"),
    m_fd(-1) {
}

SerialChannelOutput::~SerialChannelOutput() {
    // an output whose Init() failed after the port opened is deleted without
    // Close(); the fd would leak and keep its TIOCEXCL lock, so the next
    // attempt to open the port fails as "in use"
    if (m_fd >= 0) {
        closeSerialPort();
    }
    if (!pinName.empty()) {
        PinCapabilities::getPinByName(pinName).releasePin();
    }
}

bool SerialChannelOutput::setupSerialPort(Json::Value& config, int baud, const char* mode) {
    if (config.isMember("device")) {
        m_deviceName = config["device"].asString();
    }
    if (m_deviceName == "UNKNOWN") {
        LogErr(VB_CHANNELOUT, "Invalid Config.  Unknown device.\n");
        WarningHolder::AddWarning(26, "SerialChannelOutput: Invalid Config.  Unknown device.");
        return false;
    }
    std::string desc = "";
    if (config.isMember("subType")) {
        desc = config["subType"].asString();
    } else if (config.isMember("type")) {
        desc = config["type"].asString();
    }

    const PinCapabilities* pin = nullptr;
#ifdef HAS_CAPEUTILS
    Json::Value v = CapeUtils::INSTANCE.getCapeInfo();
    if (v.isMember("tty-labels")) {
        Json::Value ttyLabels = v["tty-labels"];
        if (ttyLabels.isMember(m_deviceName)) {
            std::string pinName = ttyLabels[m_deviceName].asString();
            pin = PinCapabilities::getPinByName(pinName).ptr();
            if (pin && !pin->uart.ends_with("-tx")) {
                // The label names the header pin the cape drives its line
                // driver from.  On another board the same pin can be only a
                // UART's receive line (P1-10 is UART2 TX on a PocketBeagle but
                // UART6 RX on a PocketBeagle 2), and opening that UART would
                // send the data out of some other pin while this one sits idle.
                // Permanent, like the other config errors here: only a
                // different board or cape changes it.
                std::string w = desc + ": " + m_deviceName + " is wired to " + pinName +
                                ", which has no serial transmit on this board";
                LogErr(VB_CHANNELOUT, "%s\n", w.c_str());
                WarningHolder::AddWarning(26, w);
                return false;
            }
            if (pin) {
                std::string nd = pin->uart.substr(0, pin->uart.find("-"));
                LogDebug(VB_CHANNELOUT, "SerialChannelOutput::setupSerialPort:  Using cape mapping from %s to %s\n", m_deviceName.c_str(), nd.c_str());
                m_deviceName = nd;
                this->pinName = pinName;
            }
        }
    }
#endif
    if (pin == nullptr) {
        pin = PinCapabilities::getPinByName(m_deviceName + "-tx").ptr();
        if (!pin) {
            pin = PinCapabilities::getPinByUART(m_deviceName + "-tx").ptr();
        }
        if (pin) {
            this->pinName = pin->name;
        }
    }
    if (desc.empty()) {
        desc = m_deviceName;
    } else {
        desc = desc + "-" + m_deviceName;
    }
    m_deviceName = "/dev/" + m_deviceName;
    // checked before the pin is muxed: SerialOpen() refuses the console too,
    // but by then this would already have put console output on the pin
    if (SerialIsConsole(m_deviceName.c_str())) {
        LogErr(VB_CHANNELOUT, "%s: is the system console, not opening it\n", m_deviceName.c_str());
        WarningHolder::AddWarning(26, desc + ": " + m_deviceName + " is the system console and cannot be used for serial output");
        return false;
    }
    if (pin) {
        pin->configPin("uart", true, desc);
    }
    m_fd = SerialOpen(m_deviceName.c_str(), baud, mode, true, pin == nullptr, desc.c_str());
    if (m_fd < 0) {
        LogErr(VB_CHANNELOUT, "Could not open serial port %s\n", m_deviceName.c_str());
        WarningHolder::AddWarning(26, desc + ": could not open " + m_deviceName + ". Is the device connected?");
        return false;
    }
    return true;
}

int SerialChannelOutput::closeSerialPort() {
    LogDebug(VB_CHANNELOUT, "SerialChannelOutput::closeSerialPort()\n");
    SerialClose(m_fd);
    m_fd = -1;
    return 1;
}

void SerialChannelOutput::dumpSerialConfig(void) {
    LogDebug(VB_CHANNELOUT, "    Device Name: %s\n", m_deviceName.c_str());
    LogDebug(VB_CHANNELOUT, "    fd         : %d\n", m_fd);
}
