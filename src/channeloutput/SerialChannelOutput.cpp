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

#include "../Warnings.h"
#include "../config.h"
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
}

bool SerialChannelOutput::setupSerialPort(Json::Value& config, int baud, const char* mode) {
    if (config.isMember("device")) {
        m_deviceName = config["device"].asString();
    }
    if (m_deviceName == "UNKNOWN") {
        LogErr(VB_CHANNELOUT, "Invalid Config.  Unknown device.\n");
        WarningHolder::AddWarning("SerialChannelOutput: Invalid Config.  Unknown device.");
        return false;
    }
    const PinCapabilities* pin = nullptr;
#ifdef HAS_CAPEUTILS
    Json::Value v = CapeUtils::INSTANCE.getCapeInfo();
    if (v.isMember("tty-labels")) {
        Json::Value ttyLabels = v["tty-labels"];
        if (ttyLabels.isMember(m_deviceName)) {
            std::string pinName = ttyLabels[m_deviceName].asString();
            const PinCapabilities* pin = PinCapabilities::getPinByName(pinName).ptr();
            if (pin) {
                std::string nd = pin->uart.substr(0, pin->uart.find("-"));
                LogDebug(VB_CHANNELOUT, "SerialChannelOutput::setupSerialPort:  Using cape mapping from %s to %s\n", m_deviceName.c_str(), nd.c_str());
                m_deviceName = nd;
            }
        }
    }
#endif
    if (pin == nullptr) {
        pin = PinCapabilities::getPinByName(m_deviceName + "-tx").ptr();
    }
    if (pin) {
        pin->configPin("uart");
    }
    m_deviceName = "/dev/" + m_deviceName;
    m_fd = SerialOpen(m_deviceName.c_str(), baud, mode);
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
