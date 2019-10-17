/*
 *   Hill320 Channel Output driver for Falcon Player (FPP)
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

#ifndef _HILL320_H
#define _HILL320_H

#include "ThreadedChannelOutputBase.h"
#include "util/I2CUtils.h"

class Hill320Output : public ThreadedChannelOutputBase {
  public:
	Hill320Output(unsigned int startChannel, unsigned int channelCount);
	~Hill320Output();

	int Init(Json::Value config);
	int Close(void);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) {
        addRange(m_startChannel, m_startChannel + m_channelCount - 1);
    }

  private:
    I2CUtils *i2c;
	int m_boxCount;
};

#endif
