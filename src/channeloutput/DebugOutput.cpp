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

#include "fpp-pch.h"
#include "DebugOutput.h"



extern "C" {
    DebugOutput *createOutputDebug(unsigned int startChannel,
                                   unsigned int channelCount) {
        return new DebugOutput(startChannel, channelCount);
    }
}

/*
 *
 */
DebugOutput::DebugOutput(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount)
{
	LogDebug(VB_CHANNELOUT, "DebugOutput::DebugOutput(%u, %u)\n",
		startChannel, channelCount);
}

/*
 *
 */
DebugOutput::~DebugOutput()
{
	LogDebug(VB_CHANNELOUT, "DebugOutput::~DebugOutput()\n");
}

/*
 *
 */
int DebugOutput::Init(Json::Value v)
{
	LogDebug(VB_CHANNELOUT, "DebugOutput::Init()\n");

	// Call the base class' Init() method, do not remove this line.
	return ChannelOutputBase::Init(v);
}

/*
 *
 */
int DebugOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "DebugOutput::Close()\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
int DebugOutput::SendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "DebugOutput::RawSendData(%p)\n", channelData);

	HexDump("DebugOutput::RawSendData", channelData, m_channelCount);

	return m_channelCount;
}

/*
 *
 */
void DebugOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "DebugOutput::DumpConfig()\n");

	// Call the base class' DumpConfig() method, do not remove this line.
	ChannelOutputBase::DumpConfig();
}

