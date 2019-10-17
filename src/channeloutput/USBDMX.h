/*
 *   USB DMX handler for Falcon Player (FPP)
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

#ifndef _USBDMX_H
#define _USBDMX_H

#include <string>

#include "ThreadedChannelOutputBase.h"

class USBDMXOutput : public ThreadedChannelOutputBase {
  public:
	USBDMXOutput(unsigned int startChannel, unsigned int channelCount);
	~USBDMXOutput();

    virtual int Init(Json::Value config);
	int Init(char *configStr);

	int Close(void);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange);

  private:
	int RawSendDataOpen(unsigned char *channelData);
	int RawSendDataPro(unsigned char *channelData);

    enum DongleType {
		DMX_DVC_UNKNOWN,
		DMX_DVC_PRO,
		DMX_DVC_OPEN
	};

	DongleType m_dongleType;

	std::string m_deviceName;
	int         m_fd;
	char        m_outputData[513];
	char        m_dmxHeader[5];
	char        m_dmxFooter[1];
};

#endif /* #ifdef _USBDMX_H */
