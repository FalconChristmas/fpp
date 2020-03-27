/*
 *   USB DMX handler for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "settings.h"
#include "serialutil.h"
#include "USBDMX.h"

#define DMX_MAX_CHANNELS 512


extern "C" {
    USBDMXOutput *createUSBDMXOutput(unsigned int startChannel,
                                               unsigned int channelCount) {
        return new USBDMXOutput(startChannel, channelCount);
    }
}
/////////////////////////////////////////////////////////////////////////////

USBDMXOutput::USBDMXOutput(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_dongleType(DMX_DVC_UNKNOWN),
	m_deviceName("UNKNOWN"),
	m_fd(-1)
{
	LogDebug(VB_CHANNELOUT, "USBDMXOutput::USBDMXOutput(%u, %u)\n",
		startChannel, channelCount);

	m_useDoubleBuffer = 1;
    //DMX protocol requires data to be sent at least every 250ms
    m_maxWait = 250;
    m_dataOffset = 1;
    memset(m_outputData, 0, sizeof(m_outputData));
}

USBDMXOutput::~USBDMXOutput()
{
	LogDebug(VB_CHANNELOUT, "USBDMXOutput::~USBDMXOutput()\n");
}
void USBDMXOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

int USBDMXOutput::Init(Json::Value config) {
	LogDebug(VB_CHANNELOUT, "USBDMXOutput::Init()\n");
    if (config.isMember("device")) {
        m_deviceName = config["device"].asString();
    }
    if (config.isMember("type")) {
        std::string type = config["type"].asString();
        if (type == "DMX-Open") {
            LogDebug(VB_CHANNELOUT, "Treating device as DMX-Open compatible\n");
            m_dongleType = DMX_DVC_OPEN;
        } else if (type == "DMX-Pro") {
            LogDebug(VB_CHANNELOUT, "Treating device as DMX-Pro compatible\n");
            m_dongleType = DMX_DVC_PRO;
        }
    }

	if ((m_deviceName == "UNKNOWN") ||
		(m_dongleType == DMX_DVC_UNKNOWN)) {
		LogErr(VB_CHANNELOUT, "Invalid Config.  Unknown device or type.\n");
		return 0;
	}
    m_deviceName = "/dev/" + m_deviceName;

    if (m_dongleType == DMX_DVC_OPEN) {
        m_fd = SerialOpen(m_deviceName.c_str(), 250000, "8N2");
    } else if (m_dongleType == DMX_DVC_PRO) {
        m_fd = SerialOpen(m_deviceName.c_str(), 115200, "8N1");
    }

    if (m_fd < 0) {
        LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
               errno, m_deviceName.c_str(), strerror(errno));
        return 0;
    }

	if (m_dongleType == DMX_DVC_OPEN) {
		if (SerialResetRTS(m_fd) != 0) {
            LogErr(VB_CHANNELOUT, "Error %d resetting RTS on %s: %s\n",
                   errno, m_deviceName.c_str(), strerror(errno));
            return 0;
        }
        m_dataLen = m_channelCount + 1;
    } else if (m_dongleType == DMX_DVC_PRO) {
		m_outputData[0] = 0x7E;
		m_outputData[1] = 0x06;
		m_outputData[2] =  (m_channelCount + 1)       & 0xFF;
		m_outputData[3] = ((m_channelCount + 1) >> 8) & 0xFF;
		m_outputData[4] = 0x00;
        m_dataOffset = 5;

		m_outputData[m_channelCount + 5] = 0xE7;
        m_dataLen = m_channelCount + 6;
	}
	
	return ThreadedChannelOutputBase::Init(config);
}

int USBDMXOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "USBDMXOutput::Close()\n");

	SerialClose(m_fd);

	return ThreadedChannelOutputBase::Close();
}

int USBDMXOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "USBDMXOutput::RawSendData(%p)\n", channelData);
    memcpy(m_outputData + m_dataOffset, channelData, m_channelCount);
    WaitTimedOut();
	return m_channelCount;
}
void USBDMXOutput::WaitTimedOut() {
    if (m_dongleType == DMX_DVC_OPEN) {
        SerialSendBreak(m_fd, 200);
        usleep(20);
    }
    write(m_fd, m_outputData, m_dataLen);
}


void USBDMXOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "USBDMXOutput::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Dongle Type: %s\n",
		m_dongleType == DMX_DVC_PRO ? "Pro" :
			m_dongleType == DMX_DVC_OPEN ? "Open" : "UNKNOWN");
	LogDebug(VB_CHANNELOUT, "    Device Name: %s\n", m_deviceName.c_str());
	LogDebug(VB_CHANNELOUT, "    fd         : %d\n", m_fd);

    ThreadedChannelOutputBase::DumpConfig();
}

