/*
 *   Hill320 Channel Output driver for Falcon Player (FPP)
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
 *  10 (VSS)  - Pi Pin 6 (Ground) AND DB25 Parallel Port Pin 25
 *  12 (SCL)  - Pi Pin 5 (SCL)
 *  13 (SDA)  - Pi Pin 3 (SDA)
 *  18 (Reset)- Pi Pin 4 (5V)
 *  15 (A0)   - Pi Pin 6 (Ground)
 *  16 (A1)   - Pi Pin 6 (Ground)
 *  17 (A2)   - Pi Pin 6 (Ground)
 *
 *   1 (GPB0) - DB25 Parallel Port Pin 1
 *   2 (GPB1) - DB25 Parallel Port Pin 14
 *  21 (GPA0) - DB25 Parallel Port Pin 2
 *  22 (GPA1) - DB25 Parallel Port Pin 3
 *  23 (GPA2) - DB25 Parallel Port Pin 4
 *  24 (GPA3) - DB25 Parallel Port Pin 5
 *  25 (GPA4) - DB25 Parallel Port Pin 6
 *  26 (GPA5) - DB25 Parallel Port Pin 7
 *  27 (GPA6) - DB25 Parallel Port Pin 8
 *  28 (GPA7) - DB25 Parallel Port Pin 9
 */

#include <stdlib.h>

#include "common.h"
#include "Hill320.h"
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
Hill320Output::Hill320Output(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_fd(-1)
{
	LogDebug(VB_CHANNELOUT, "Hill320Output::Hill320Output(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = 320;

	m_boxCount = m_channelCount / 8;
}

/*
 *
 */
Hill320Output::~Hill320Output()
{
	LogDebug(VB_CHANNELOUT, "Hill320Output::~Hill320Output()\n");
}

/*
 *
 */
int Hill320Output::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "Hill320Output::Init(JSON)\n");

	m_fd = wiringPiI2CSetup(0x20);

	if (m_fd < 0)
	{
		LogErr(VB_CHANNELOUT, "Error opening I2C device for Hill320 output\n");
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
int Hill320Output::Close(void)
{
	LogDebug(VB_CHANNELOUT, "Hill320Output::Close()\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
int Hill320Output::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "Hill320Output::RawSendData(%p)\n", channelData);

	unsigned char *c = channelData;
	int bank = 0;
	int bankbox = 0;
	int byte = 0;
	int ch = 0;

	for (int box = 1; box <= m_boxCount; box++)
	{
		byte = 0;
		for (int x = 0; x < 8 && ch < m_channelCount; x++, ch++)
		{
			if (*(c++))
			{
				byte |= 0x1 << x;
			}
		}

		if (box <= 8)
		{
			bank    = 8;
			bankbox = box - 1;
		}
		else if (box <= 16)
		{
			bank    = 16;
			bankbox = box - 9;
		}
		else if (box <= 24)
		{
			bank    = 32;
			bankbox = box - 17;
		}
		else if (box <= 32)
		{
			bank    = 64;
			bankbox = box - 25;
		}
		else if (box <= 40)
		{
			bank    = 128;
			bankbox = box - 33;
		}

		LogExcess(VB_CHANNELOUT,
			"Box: %d, Bank: 0x%02x, BankBox: 0x%02x, Byte: 0b"
			BYTETOBINARYPATTERN "\n",
			box, bank, bankbox, BYTETOBINARY(byte));

		// Set data byte
		wiringPiI2CWriteReg8(m_fd, MCP23x17_GPIOA, byte);

		// Set C0 & C1 HIGH
		wiringPiI2CWriteReg8(m_fd, MCP23x17_GPIOB, 0b00000011);

		// Set C0 LOW and C1 HIGH
		wiringPiI2CWriteReg8(m_fd, MCP23x17_GPIOB, 0b00000010);

		// Set bank information
		wiringPiI2CWriteReg8(m_fd, MCP23x17_GPIOA, bankbox + bank);

		// Set C0 & C1 LOW
		wiringPiI2CWriteReg8(m_fd, MCP23x17_GPIOB, 0b00000000);

		// Set C0 LOW and C1 HIGH
		wiringPiI2CWriteReg8(m_fd, MCP23x17_GPIOB, 0b00000010);
	}

	return m_channelCount;
}

/*
 *
 */
void Hill320Output::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "Hill320Output::DumpConfig()\n");

	LogDebug(VB_CHANNELOUT, "    fd      : %d\n", m_fd);
	LogDebug(VB_CHANNELOUT, "    BoxCount: %d\n", m_boxCount);

	ChannelOutputBase::DumpConfig();
}

