/*
 *   Pixelnet USB handler for Falcon Player (FPP)
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
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "log.h"
#include "serialutil.h"
#include "USBPixelnet.h"

extern "C" {
    USBPixelnetOutput *createUSBPixelnetOutput(unsigned int startChannel,
                                               unsigned int channelCount) {
        return new USBPixelnetOutput(startChannel, channelCount);
    }
}
/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
USBPixelnetOutput::USBPixelnetOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_deviceName(""),
	m_outputData(NULL),
	m_pixelnetData(NULL),
	m_fd(-1),
	m_dongleType(PIXELNET_DVC_UNKNOWN)
{
	LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::USBPixelnetOutput(%u, %u)\n",
		startChannel, channelCount);

	m_useDoubleBuffer = 1;
}

/*
 *
 */
USBPixelnetOutput::~USBPixelnetOutput()
{
	LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::~USBPixelnetOutput()\n");
}
int USBPixelnetOutput::Init(Json::Value config) {
	LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::Init()\n");
    if (config.isMember("device")) {
        m_deviceName = config["device"].asString();
    }
    if (config.isMember("type")) {
        std::string type = config["type"].asString();
        if (type == "Pixelnet-Lynx") {
            LogDebug(VB_CHANNELOUT, "Treating device as Pixelnet-Lynx compatible\n");
            m_dongleType = PIXELNET_DVC_LYNX;
        } else if (type == "Pixelnet-Open") {
            LogDebug(VB_CHANNELOUT, "Treating device as Pixelnet-Open compatible\n");
            m_dongleType = PIXELNET_DVC_OPEN;
        }
    }

    if ((m_deviceName == "") ||
        (m_dongleType == PIXELNET_DVC_UNKNOWN)) {
        LogErr(VB_CHANNELOUT, "Invalid Config.  Unknown device or type.\n");
        return 0;
    }
    m_deviceName = "/dev/" + m_deviceName;
    
	LogInfo(VB_CHANNELOUT, "Opening %s for Pixelnet output\n",
		m_deviceName.c_str());

	if (m_dongleType == PIXELNET_DVC_LYNX)
		m_fd = SerialOpen(m_deviceName.c_str(), 115200, "8N1");
	else if (m_dongleType == PIXELNET_DVC_OPEN)
		m_fd = SerialOpen(m_deviceName.c_str(), 1000000, "8N2");

	if (m_fd < 0)
	{
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, m_deviceName.c_str(), strerror(errno));
	}

	if (m_dongleType == PIXELNET_DVC_LYNX) {
		m_outputData    = m_rawData + 7;
		m_pixelnetData  = m_outputData + 1;

		m_outputData[0] = '\xAA';

		m_outputPacketSize = 4097;
	} else if (m_dongleType == PIXELNET_DVC_OPEN) {
		m_outputData    = m_rawData + 2;
		m_pixelnetData  = m_outputData + 6;

		m_outputData[0] = '\xAA';
		m_outputData[1] = '\x55';
		m_outputData[2] = '\x55';
		m_outputData[3] = '\xAA';
		m_outputData[4] = '\x15';
		m_outputData[5] = '\x5D';

		m_outputPacketSize = 4102;
	}

	return ThreadedChannelOutputBase::Init(config);
}


void USBPixelnetOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
int USBPixelnetOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::Close()\n");

	SerialClose(m_fd);
	m_fd = -1;

	return ThreadedChannelOutputBase::Close();
}

/*
 *
 */
int USBPixelnetOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "USBPixelnetOutput::RawSendData(%p)\n", channelData);

	memcpy(m_pixelnetData, channelData, m_channelCount);

	// 0xAA is start of Pixelnet packet, so convert 0xAA (170) to 0xAB (171)
	unsigned char *dptr = m_pixelnetData;
	unsigned char *eptr = dptr + m_channelCount;
	for( ; dptr < eptr; dptr++ ) {
		if (*dptr == 0xAA)
			*dptr = 0xAB;
	}

	// Send Header and Pixelnet Data
	write(m_fd, m_outputData, m_outputPacketSize);

	return m_channelCount;
}

/*
 *
 */
void USBPixelnetOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "USBPixelnetOutput::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Device Filename   : %s\n", m_deviceName.c_str());
	LogDebug(VB_CHANNELOUT, "    fd                : %d\n", m_fd);
	LogDebug(VB_CHANNELOUT, "    Output Packet Size: %d\n", m_outputPacketSize);

	ThreadedChannelOutputBase::DumpConfig();
}

