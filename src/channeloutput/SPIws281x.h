/*
 *   WS281x SPI handler for Falcon Player (FPP)
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

#ifndef _SPIWS281x_H
#define _SPIWS281x_H

#include "ChannelOutputBase.h"

#define SPIWS281x_SPEED 2857142  //May need to be 3,800,000 for some models
#define SPIWS281x_MAX_CHANNELS  341

// each pair of WS2812 bits is sent as 1 spi byte
// looks up 2 bits of led data to get the spi pattern to send
// A ws281x '0' is sent as 1000
// A ws281x '1' is sent as 1100
// A ws281x 'reset/latch' is sent as 3 bytes of 00000000
uint8_t bitpair_to_byte[] = {
    0b10001000,
    0b10001100,
    0b11001000,
    0b11001100,
};

class SPIws281xOutput : public ChannelOutputBase {
  public:
	SPIws281xOutput(unsigned int startChannel, unsigned int channelCount);
	~SPIws281xOutput();

	int Init(char *configStr);

	int Close(void);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

  private:
  int            m_fd;
	int            m_port;
};

#endif
