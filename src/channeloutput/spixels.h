/*
 *   spixels library Channel Output for Falcon Player (FPP)
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

#ifndef _SPIXELS_H
#define _SPIXELS_H

#include <vector>

#include "../../external/spixels/include/led-strip.h"

#include "ThreadedChannelOutputBase.h"
#include "PixelString.h"

using namespace spixels;

class SpixelsOutput : public ThreadedChannelOutputBase {
  public:
	SpixelsOutput(unsigned int startChannel, unsigned int channelCount);
	~SpixelsOutput();

	int Init(Json::Value config);
	int Close(void);

	void PrepData(unsigned char *channelData);
	int  RawSendData(unsigned char *channelData);

	void DumpConfig(void);

	virtual void GetRequiredChannelRange(int &min, int & max);

  private:
	MultiSPI    *m_spi;

	std::vector<LEDStrip*>    m_strips;
	std::vector<PixelString*> m_strings;
};

#endif
