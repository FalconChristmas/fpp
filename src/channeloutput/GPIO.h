/*
 *   GPIO Pin Channel Output driver for Falcon Player (FPP)
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

#ifndef _GPIO_H
#define _GPIO_H

#include "ChannelOutputBase.h"

class GPIOOutput : public ChannelOutputBase {
  public:
	GPIOOutput(unsigned int startChannel, unsigned int channelCount);
	~GPIOOutput();

	int Init(char *configStr);
	int Close(void);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

  private:
	int m_GPIOPin;
	int m_invertOutput;
	int m_softPWM;

};

#endif
