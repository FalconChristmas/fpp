/*
 *   Generic Serial handler for Falcon Player (FPP)
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

#ifndef _GENERICSERIAL_H
#define _GENERICSERIAL_H

#include <string>

#include "ThreadedChannelOutputBase.h"

#define GENERICSERIAL_MAX_CHANNELS 2048

class GenericSerialOutput : public ThreadedChannelOutputBase {
  public:
	GenericSerialOutput(unsigned int startChannel, unsigned int channelCount);
	virtual ~GenericSerialOutput();

    virtual int Init(Json::Value config) override;
	virtual int Close(void) override;

	virtual int RawSendData(unsigned char *channelData) override;

	virtual void DumpConfig(void) override;
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override;

  private:
	std::string m_deviceName;
	int         m_fd;
	int         m_speed;
	int         m_headerSize;
	std::string m_header;
	int         m_footerSize;
	std::string m_footer;
	int         m_packetSize;
	char       *m_data;
};

#endif /* #ifdef _GENERICSERIAL_H */
