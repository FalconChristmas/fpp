/*
 *   Pixelnet USB handler for Falcon Player (FPP)
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

#ifndef _USBPIXELNET_H
#define _USBPIXELNET_H

#include <string>

#include "ChannelOutputBase.h"

class USBPixelnetOutput : public ChannelOutputBase {
  public:
	USBPixelnetOutput(unsigned int startChannel, unsigned int channelCount);
	~USBPixelnetOutput();

	int Init(char *configStr);
	int Close(void);

	int RawSendData(unsigned char *channelData);

	void DumpConfig(void);

  private:
	enum DongleType {
		PIXELNET_DVC_UNKNOWN,
		PIXELNET_DVC_LYNX,
		PIXELNET_DVC_OPEN
	};

	std::string    m_deviceName;
	unsigned char  m_rawData[4104];  // Sized to a multiple of 8 bytes
	int            m_outputPacketSize;  // Header size + 4096 data bytes
	unsigned char *m_outputData;
	unsigned char *m_pixelnetData;
	int            m_fd;
	DongleType     m_dongleType;
};

#endif /* _USBPIXELNET_H */
