/*
 *   Debugging Channel Output driver for Falcon Player (FPP)
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

#include "common.h"
#include "DebugOutput.h"
#include "log.h"
#include "sequence.h"

/*
 *
 */
DebugOutput::DebugOutput(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount)
{
	LogDebug(VB_CHANNELOUT, "DebugOutput::DebugOutput(%u, %u)\n",
		startChannel, channelCount);

	// Set any max channels limit if necessary
	m_maxChannels = FPPD_MAX_CHANNELS;
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
int DebugOutput::Init(char *configStr)
{
	LogDebug(VB_CHANNELOUT, "DebugOutput::Init('%s')\n", configStr);

	// Call the base class' Init() method, do not remove this line.
	return ChannelOutputBase::Init(configStr);
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
int DebugOutput::RawSendData(unsigned char *channelData)
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

