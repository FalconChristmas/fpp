/*
 *   MCP23017 Channel Output driver for Falcon Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

/*
 * This driver requires a MCP23017 I2C chip with the following connections:
 *
 * MCP Pin      Connection
 * ----------   ------------------------
 *   9 (VDD)  - Pi Pin 4 (5V)
 *  10 (VSS)  - Pi Pin 6 (Ground)
 *  12 (SCL)  - Pi Pin 5 (SCL)
 *  13 (SDA)  - Pi Pin 3 (SDA)
 *  18 (Reset)- Pi Pin 4 (5V)
 *  15 (A0)   - Pi Pin 6 (Ground) (for 0x20 hex, 32 int device ID)
 *  16 (A1)   - Pi Pin 6 (Ground) (this can be configured in channelouputs.json)
 *  17 (A2)   - Pi Pin 6 (Ground) (if more than one MCP23017 is attached)
 *
 */

/*
 * Sample channeloutputs.json config
 *
 * {
 *       "channelOutputs": [
 *               {
 *                       "type": "MCP23017",
 *                       "enabled": 1,
 *                       "deviceID": 32,
 *                       "startChannel": 1,
 *                       "channelCount": 16
 *               }
 *       ]
 * }
 *
 */

#include <stdlib.h>

#include "common.h"
#include "MCP23017.h"
#include "log.h"

#define MCP23x17_IOCON      0x0A
#define MCP23x17_IODIRA     0x00
#define MCP23x17_IODIRB     0x01
#define MCP23x17_GPIOA      0x12
#define MCP23x17_GPIOB      0x13

#define IOCON_INIT          0x20

#ifdef USEWIRINGPI
#   include "wiringPiI2C.h"
#else
#   define wiringPiI2CSetup(x) 1
#   define wiringPiI2CWriteReg8(x, y, z) 1
#endif

#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)  \
  (byte & 0x80 ? 1 : 0), \
  (byte & 0x40 ? 1 : 0), \
  (byte & 0x20 ? 1 : 0), \
  (byte & 0x10 ? 1 : 0), \
  (byte & 0x08 ? 1 : 0), \
  (byte & 0x04 ? 1 : 0), \
  (byte & 0x02 ? 1 : 0), \
  (byte & 0x01 ? 1 : 0) 

/*
 *
 */
MCP23017Output::MCP23017Output(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_fd(-1)
{
	LogDebug(VB_CHANNELOUT, "MCP23017Output::MCP23017Output(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = 16;
}

/*
 *
 */
MCP23017Output::~MCP23017Output()
{
	LogDebug(VB_CHANNELOUT, "MCP23017Output::~MCP23017Output()\n");
}

/*
 *
 */
int MCP23017Output::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "MCP23017Output::Init(JSON)\n");

	m_deviceID = config["deviceID"].asInt();

	m_fd = wiringPiI2CSetup(0x20);

	if (m_fd < 0)
	{
		LogErr(VB_CHANNELOUT, "Error opening I2C device for MCP23017 output\n");
		return 0;
	}

	// Initialize
	wiringPiI2CWriteReg8(m_fd, MCP23x17_IOCON, IOCON_INIT);

	// Enable all pins for output
	wiringPiI2CWriteReg8(m_fd, MCP23x17_IODIRA, 0b00000000);
	wiringPiI2CWriteReg8(m_fd, MCP23x17_IODIRB, 0b00000000);

	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int MCP23017Output::Close(void)
{
	LogDebug(VB_CHANNELOUT, "MCP23017Output::Close()\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
int MCP23017Output::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "MCP23017Output::RawSendData(%p)\n", channelData);

	unsigned char *c = channelData;
	int bank = 0;
	int bankbox = 0;
	int byte1 = 0;
	int byte2 = 0;
	int ch = 0;

	for (int x = 0; ch < m_channelCount; x++, ch++)
	{
		if (*(c++))
		{
			if (x < 8)
				byte1 |= 0x1 << x;
			else
				byte2 |= 0x1 << (x-8);
		}
	}

	LogExcess(VB_CHANNELOUT,
		"Byte1: 0b" BYTETOBINARYPATTERN ", Byte2: 0b" BYTETOBINARYPATTERN "\n",
		BYTETOBINARY(byte1), BYTETOBINARY(byte2));

	// Set lower 8 data byte
	wiringPiI2CWriteReg8(m_fd, MCP23x17_GPIOA, byte1);

	// Set upper 8 data bits
	wiringPiI2CWriteReg8(m_fd, MCP23x17_GPIOB, byte2);


	return m_channelCount;
}

/*
 *
 */
void MCP23017Output::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "MCP23017Output::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    deviceID: %d\n", m_deviceID);
	LogDebug(VB_CHANNELOUT, "    fd      : %d\n", m_fd);

	ChannelOutputBase::DumpConfig();
}

