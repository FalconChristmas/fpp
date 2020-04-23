#pragma once
/*
 *   Raspberry Pi rpi_ws281x handler for Falcon Player (FPP)
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

extern "C" {
#include "../../external/rpi_ws281x/clk.h"
#include "../../external/rpi_ws281x/gpio.h"
#include "../../external/rpi_ws281x/dma.h"
#include "../../external/rpi_ws281x/pwm.h"
#include "../../external/rpi_ws281x/ws2811.h"
}

#include <vector>

#include "ThreadedChannelOutputBase.h"
#include "PixelString.h"

class RPIWS281xOutput : public ThreadedChannelOutputBase {
  public:
	RPIWS281xOutput(unsigned int startChannel, unsigned int channelCount);
	virtual ~RPIWS281xOutput();

	virtual int Init(Json::Value config) override;
	virtual int Close(void) override;

	virtual void PrepData(unsigned char *channelData) override;
	virtual int  RawSendData(unsigned char *channelData) override;

	virtual void DumpConfig(void) override;

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;

  private:
	void SetupCtrlCHandler(void);

	int          m_ledstringNumber;
	int          m_string1GPIO;
	int          m_string2GPIO;
	int          m_pixels;

	std::vector<PixelString*> m_strings;
};

