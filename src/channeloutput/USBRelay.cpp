/*
 *   USB Relay handler for Falcon Player (FPP)
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
#include "USBRelay.h"




extern "C" {
    USBRelayOutput *createOutputUSBRelay(unsigned int startChannel,
                                         unsigned int channelCount) {
        return new USBRelayOutput(startChannel, channelCount);
    }
}
/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
USBRelayOutput::USBRelayOutput(unsigned int startChannel,
	unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_deviceName(""),
	m_fd(-1),
	m_subType(RELAY_DVC_UNKNOWN),
	m_relayCount(0)
{
	LogDebug(VB_CHANNELOUT, "USBRelayOutput::USBRelayOutput(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
USBRelayOutput::~USBRelayOutput()
{
	LogDebug(VB_CHANNELOUT, "USBRelayOutput::~USBRelayOutput()\n");
}

void USBRelayOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + m_relayCount - 1);
}

/*
 *
 */
int USBRelayOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "USBRelayOutput::Init(JSON)\n");

	std::string subType = config["subType"].asString();

	if (subType == "Bit")
		m_subType = RELAY_DVC_BIT;
	else if (subType == "ICStation")
		m_subType = RELAY_DVC_ICSTATION;

	m_deviceName = config["device"].asString();
	m_relayCount = config["channelCount"].asInt();

	if ((m_deviceName == "") ||
		(m_subType == RELAY_DVC_UNKNOWN))
	{
		LogErr(VB_CHANNELOUT, "Invalid Config, missing device or invalid type\n");
		return 0;
	}

	m_deviceName.insert(0, "/dev/");

	LogInfo(VB_CHANNELOUT, "Opening %s for USB Relay output\n",
		m_deviceName.c_str());

	m_fd = SerialOpen(m_deviceName.c_str(), 9600, "8N1");

	if (m_fd < 0)
	{
		LogErr(VB_CHANNELOUT, "Error %d opening %s: %s\n",
			errno, m_deviceName.c_str(), strerror(errno));
	}

	if (m_subType == RELAY_DVC_ICSTATION) {
		// Send Initialization Sequence
		char c_init = 0x50;
		char c_reply = 0x00;
		char c_open = 0x51;

		sleep(1);
		write(m_fd, &c_init, 1);
		usleep(500000);

		bool foundICS = false;
		int res = 0;
		res = read(m_fd, &c_reply, 1);
		if (res == 0) {
			LogWarn(VB_CHANNELOUT, "Did not receive a response byte from ICstation relay, unable to confirm number of relays.  Using configuration value from UI\n");
		} else if (c_reply == 0xAB) {
			LogInfo(VB_CHANNELOUT, "Found a 4-channel ICStation relay module\n");
			m_relayCount = 4;
			foundICS = true;
		} else if (c_reply == 0xAC) {
			LogInfo(VB_CHANNELOUT, "Found a 8-channel ICStation relay module\n");
			m_relayCount = 8;
			foundICS = true;
		} else if (c_reply == 0xAD) {
			LogInfo(VB_CHANNELOUT, "Found a 2-channel ICStation relay module\n");
			m_relayCount = 2;
			foundICS = true;
		} else {
			LogWarn(VB_CHANNELOUT, "Warning: ICStation USB Relay response of 0x%02x doesn't match "
				"known values.  Unable to detect number of relays.\n", c_reply);
		}

		if (foundICS)
			write(m_fd, &c_open, 1);
	}

	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int USBRelayOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "USBRelayOutput::Close()\n");

	SerialClose(m_fd);
	m_fd = -1;

	return ChannelOutputBase::Close();
}

/*
 *
 */
int USBRelayOutput::SendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "USBRelayOutput::RawSendData(%p)\n", channelData);

	char out       = 0x00;
	int  shiftBits = 0;

	for (int i = 0; i < m_relayCount; i++)
	{
		if ((i > 0) && ((i % 8) == 0))
		{
			// Write out previous byte
			write(m_fd, &out, 1);
			out = 0x00;
			shiftBits = 0;
		}

		out |= (channelData[i] ? 1 : 0) << shiftBits;
		shiftBits++;
	}

	// Write out any unwritten bits
	if (shiftBits)
		write(m_fd, &out, 1);

	return m_relayCount;
}

/*
 *
 */
void USBRelayOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "USBRelayOutput::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    Device Filename   : %s\n", m_deviceName.c_str());
	LogDebug(VB_CHANNELOUT, "    fd                : %d\n", m_fd);

	ChannelOutputBase::DumpConfig();
}

