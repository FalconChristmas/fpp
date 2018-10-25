/*
 *   MAX7219 Matrix Channel Output driver for Falcon Player (FPP)
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

/*
 * This Channel Output is for SPI-attached MAX7219 chips connected to
 * 8x8 LED panels.  The chips/panels may be daisy-chained to provide
 * wider displays.
 *
 * Sample channeloutputs.json config
 *
 * {
 *       "channelOutputs": [
 *               {
 *                       "type": "MAX7219Matrix",
 *                       "enabled": 1,
 *                       "startChannel": 1,
 *                       "channelCount": 128,
 *                       "panels": 2,
 *                       "channelsPerPixel": 1
 *               }
 *       ]
 * }
 *
 */

#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "MAX7219Matrix.h"
#include "log.h"

#ifdef USEWIRINGPI
#   include "wiringPi.h"
#   include "wiringPiSPI.h"
#else
#   define pinMode(a, b)
#   define digitalWrite(a, b)
#   define wiringPiSPISetup(a,b)    1
#   define wiringPiSPIDataRW(a,b,c) 1
#endif

#define MAX7219_DECODE_MODE 0x09
#define MAX7219_SHUTDOWN    0x0C
#define MAX7219_BRIGHTNESS  0x0A
#define MAX7219_SCAN_LIMIT  0x0B
#define MAX7219_TEST        0x0F

/*
 *
 */
MAX7219MatrixOutput::MAX7219MatrixOutput(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
	m_pinCS(8)
{
	LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::MAX7219MatrixOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = 8 * 64 * 3; // Eight 8x8 panels with 3 colors per pixel
}

/*
 *
 */
MAX7219MatrixOutput::~MAX7219MatrixOutput()
{
	LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::~MAX7219MatrixOutput()\n");

	Close();
}

/*
 *
 */
int MAX7219MatrixOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::Init(JSON)\n");

	if (wiringPiSPISetup(0, 1000000) < 0) {
		LogErr(VB_CHANNELOUT, "Unable to open spidev0.0\n");
		return 0;
	}

	m_panels = config["panels"].asInt();

	pinMode(m_pinCS, OUTPUT);

	usleep(50000);

	WriteCommand(MAX7219_DECODE_MODE, 0x00);  // Use LED Matrix, not BCD Mode
	WriteCommand(MAX7219_BRIGHTNESS, 0x00);   // Lowest brightness
	WriteCommand(MAX7219_SCAN_LIMIT, 0x07);   // Use all 8 columns
	WriteCommand(MAX7219_SHUTDOWN, 0x01);     // (1 == On, 0 == Off)
	WriteCommand(MAX7219_TEST, 0x00);         // Turn Off Test mode

	return ChannelOutputBase::Init(config);
}

void MAX7219MatrixOutput::GetRequiredChannelRange(int &min, int & max) {
    min = m_startChannel;
    max = min + (9 * m_panels) - 1;
}

/*
 *
 */
int MAX7219MatrixOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::Close()\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
int MAX7219MatrixOutput::WriteCommand(uint8_t cmd, uint8_t value)
{
	uint8_t c;
	uint8_t v;
	uint8_t data[256];
	int bytes = 0;

	for (int p = 0; p < m_panels; p++)
	{
		data[bytes++] = cmd;
		data[bytes++] = value;
	}

	digitalWrite(m_pinCS, 0);
	wiringPiSPIDataRW(0, data, bytes);
	digitalWrite(m_pinCS, 1);
}

/*
 *
 */
int MAX7219MatrixOutput::SendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "MAX7219MatrixOutput::SendData(%p)\n",
		channelData);

	uint8_t data[256];

	int c = 0;
	for (int i = 1; i < 9; i++) {
        int bytes = 0;

		c = (i * m_panels) - 1;

		for (int p = 0; p < m_panels; p++) {
			data[bytes++] = i;
			data[bytes++] = ReverseBitsInByte(channelData[c--]);
		}

		digitalWrite(m_pinCS, 0);
		wiringPiSPIDataRW(0, data, bytes);
		digitalWrite(m_pinCS, 1);
	}

	return m_channelCount;
}

/*
 *
 */
void MAX7219MatrixOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::DumpConfig()\n");

	ChannelOutputBase::DumpConfig();
}

