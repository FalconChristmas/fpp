/*
 *   Raspberry Pi rpi_ws281x handler for Falcon Player (FPP)
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

#ifndef _RPI_WS281X_H
#define _RPI_WS281X_H

extern "C" {
#include "../../external/rpi_ws281x/clk.h"
#include "../../external/rpi_ws281x/gpio.h"
#include "../../external/rpi_ws281x/dma.h"
#include "../../external/rpi_ws281x/pwm.h"
#include "../../external/rpi_ws281x/ws2811.h"
}

#include "ChannelOutputBase.h"

#define RPIWS281X_MAX_CHANNELS  1200

class RPIWS281xOutput : public ChannelOutputBase {
  public:
	RPIWS281xOutput(unsigned int startChannel, unsigned int channelCount);
	~RPIWS281xOutput();

	int Init(char *configStr);

	int Close(void);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

  private:
	void SetupCtrlCHandler(void);

	int          m_string1GPIO;
	int          m_string1Pixels;
	std::string  m_string1ColorOrder;
	int          m_string2GPIO;
	int          m_string2Pixels;
	std::string  m_string2ColorOrder;
};

#endif
