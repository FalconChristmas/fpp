/*
 *   WS281x SPI handler for Falcon Player (FPP)
 *      Developed by Bill Porter (madsci1016)  www.billporter.info
 *        Using example by penfold42
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

class SPIws281xOutput : public ChannelOutputBase {
  public:
	SPIws281xOutput(unsigned int startChannel, unsigned int channelCount);
	~SPIws281xOutput();

	int Init(char *configStr);

	int Close(void);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

  private:
#if defined(SPIWS281x_USE_IOCTL)        
  int            m_fd;
#endif
	int            m_port;
  int            m_size;
  uint8_t*       bufq;
};

#endif
