/*
 *   Generic Serial handler for Falcon Player (FPP)
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
#include "GenericSerial.h"

/////////////////////////////////////////////////////////////////////////////
extern "C" {
    GenericSerialOutput *createGenericSerialOutput(unsigned int startChannel,
                                               unsigned int channelCount) {
        return new GenericSerialOutput(startChannel, channelCount);
    }
}
/*
 *
 */
GenericSerialOutput::GenericSerialOutput(unsigned int startChannel, unsigned int channelCount)
  : ThreadedChannelOutputBase(startChannel, channelCount),
	m_deviceName("UNKNOWN"),
	m_fd(-1),
	m_speed(9600),
	m_headerSize(0),
	m_footerSize(0),
	m_packetSize(0),
	m_data(NULL)
{
	LogDebug(VB_CHANNELOUT, "GenericSerialOutput::GenericSerialOutput(%u, %u)\n",
		startChannel, channelCount);

	m_useDoubleBuffer = 1;
}

/*
 *
 */
GenericSerialOutput::~GenericSerialOutput()
{
	LogDebug(VB_CHANNELOUT, "GenericSerialOutput::~GenericSerialOutput()\n");
}

int GenericSerialOutput::Init(Json::Value config) {
	LogDebug(VB_CHANNELOUT, "GenericSerialOutput::Init()\n");

    if (config.isMember("device")) {
        m_deviceName = config["device"].asString();
        LogDebug(VB_CHANNELOUT, "Using %s for Generic Serial output\n",
            m_deviceName.c_str());
    }
    if (config.isMember("speed")) {
        m_speed = config["speed"].asInt();
    }
    if (config.isMember("footer")) {
        m_footer = config["footer"].asString();
        m_footerSize = m_header.length();
    }
    if (config.isMember("header")) {
        m_header = config["header"].asString();
        m_headerSize = m_header.length();
    }
	m_packetSize = m_headerSize + m_channelCount + m_footerSize;

	m_data = new char[m_packetSize + 1];
	if (!m_data) {
		LogErr(VB_CHANNELOUT, "Could not allocate channel data buffer\n");
		return 0;
	}

	if (m_headerSize)
		memcpy(m_data, m_header.c_str(), m_headerSize);

	if (m_footerSize)
		memcpy(m_data + m_headerSize + m_channelCount,
			m_footer.c_str(), m_footerSize);

	m_deviceName.insert(0, "/dev/");

	m_fd = SerialOpen(m_deviceName.c_str(), m_speed, "8N1");

	if (m_fd < 0) {
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, m_deviceName.c_str(), strerror(errno));
		return 0;
	}

	return ThreadedChannelOutputBase::Init(config);
}

void GenericSerialOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_channelCount - 1);
}

/*
 *
 */
int GenericSerialOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "GenericSerialOutput::Close()\n");

	SerialClose(m_fd);

	delete [] m_data;

	return ThreadedChannelOutputBase::Close();
}

/*
 *
 */
int GenericSerialOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "GenericSerialOutput::RawSendData(%p)\n", channelData);

	memcpy(m_data + m_headerSize, channelData, m_channelCount);

	if ((logLevel & LOG_EXCESSIVE) && (logMask & VB_CHANNELDATA))
		HexDump("Generic Serial", m_data, m_headerSize + 16);

	write(m_fd, m_data, m_packetSize);

	return m_channelCount;
}

/*
 *
 */
void GenericSerialOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "GenericSerialOutput::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Device Name: %s\n", m_deviceName.c_str());
	LogDebug(VB_CHANNELOUT, "    fd         : %d\n", m_fd);
	LogDebug(VB_CHANNELOUT, "    Port Speed : %d\n", m_speed);
	LogDebug(VB_CHANNELOUT, "    Header Size: %d\n", m_headerSize);
	LogDebug(VB_CHANNELOUT, "    Header     : '%s'\n", m_header.c_str());
	LogDebug(VB_CHANNELOUT, "    Footer Size: %d\n", m_footerSize);
	LogDebug(VB_CHANNELOUT, "    Footer     : '%s'\n", m_footer.c_str());
	LogDebug(VB_CHANNELOUT, "    Packet Size: %d\n", m_packetSize);

	ThreadedChannelOutputBase::DumpConfig();
}

