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

#define MAX7219_DECODE_MODE 0x09
#define MAX7219_SHUTDOWN    0x0C
#define MAX7219_BRIGHTNESS  0x0A
#define MAX7219_SCAN_LIMIT  0x0B
#define MAX7219_TEST        0x0F


extern "C" {
    MAX7219MatrixOutput *createMAX7219MatrixOutput(unsigned int startChannel,
                                         unsigned int channelCount) {
        return new MAX7219MatrixOutput(startChannel, channelCount);
    }
}
/*
 *
 */
MAX7219MatrixOutput::MAX7219MatrixOutput(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount),
    m_pinCS(8), m_csPin(nullptr), m_spi(nullptr)
{
	LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::MAX7219MatrixOutput(%u, %u)\n",
		startChannel, channelCount);

	m_maxChannels = 8 * 64 * 3; // Eight 8x8 panels with 3 colors per pixel
    m_csPin = PinCapabilities::getPinByGPIO(8).ptr();
}

/*
 *
 */
MAX7219MatrixOutput::~MAX7219MatrixOutput()
{
	LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::~MAX7219MatrixOutput()\n");

	Close();
    if (m_spi) {
        delete m_spi;
    }
}

/*
 *
 */
int MAX7219MatrixOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "MAX7219MatrixOutput::Init(JSON)\n");

    m_spi = new SPIUtils(0, 1000000);
    if (!m_spi->isOk()) {
		LogErr(VB_CHANNELOUT, "SPIUtils setup failed\n");
		return 0;
	}

	m_panels = config["panels"].asInt();

    m_csPin->configPin("gpio", true);

	usleep(50000);

	WriteCommand(MAX7219_DECODE_MODE, 0x00);  // Use LED Matrix, not BCD Mode
	WriteCommand(MAX7219_BRIGHTNESS, 0x00);   // Lowest brightness
	WriteCommand(MAX7219_SCAN_LIMIT, 0x07);   // Use all 8 columns
	WriteCommand(MAX7219_SHUTDOWN, 0x01);     // (1 == On, 0 == Off)
	WriteCommand(MAX7219_TEST, 0x00);         // Turn Off Test mode

	return ChannelOutputBase::Init(config);
}

void MAX7219MatrixOutput::GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
    addRange(m_startChannel, m_startChannel + (9 * m_panels) - 1);
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
void MAX7219MatrixOutput::WriteCommand(uint8_t cmd, uint8_t value)
{
	uint8_t c;
	uint8_t v;
	uint8_t data[256];
	int bytes = 0;

	for (int p = 0; p < m_panels; p++) {
		data[bytes++] = cmd;
		data[bytes++] = value;
	}

    m_csPin->setValue(0);
    m_spi->xfer(data, nullptr, bytes);
    m_csPin->setValue(1);
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

        m_csPin->setValue(0);
        m_spi->xfer(data, nullptr, bytes);
        m_csPin->setValue(1);
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

