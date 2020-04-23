#pragma once
/*
 *   Debugging Channel Output driver for Falcon Player (FPP)
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

#include "ChannelOutputBase.h"

/*
 * This DebugOutput Channel Output driver is provided for debugging
 * purposes and as a template which can be used when creating new
 * Channel Output drivers for FPP.  It reimpliments the minimal
 * methods necessary from the ChannelOutputBase class which is is
 * derived from.
 */

class DebugOutput : public ChannelOutputBase {
  public:
	DebugOutput(unsigned int startChannel, unsigned int channelCount);
	virtual ~DebugOutput();

    virtual int Init(Json::Value config) override;
    
	// Close the derived class.  This method must also call the
	// base class Close() method.
	virtual int Close(void) override;

	// Main routine to send channel data out
	virtual int SendData(unsigned char *channelData) override;

	// Dump the config variables for debugging.  This method must
	// also call the base class DumpConfig() method.
	virtual void DumpConfig(void) override;
    
    virtual void GetRequiredChannelRanges(const std::function<void(int, int)> &addRange) override {
        addRange(m_startChannel, m_startChannel + m_channelCount - 1);
    }

};
