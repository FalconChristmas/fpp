/*
 *   DDP Channel Output driver for Falcon Player (FPP)
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
 *
 *   ***********************************************************************
 *
 *   This Channel Output implements the data packet portion of the DDP
 *   protocol.  Discovery is not currently supported. Data packets are
 *   sent to port 4048 UDP or TCP. This implementation uses UDP.
 *
 *   The full DPP spec is currently located at http://www.3waylabs.com/ddp/
 *
 *   The data packet format is as follows: (mostly copied from above URL)
 *
 *   byte 0:     Flags: V V x T S R Q P
 *               V V:    2-bits for protocol version number (01 for this file)
 *               x:      reserved
 *               T:      timecode field added to end of header
 *                       if T & P are set, Push at specified time
 *               S:      Storage.  If set, data from Storage, not data-field.
 *               R:      Reply flag, marks reply to Query packet.
 *                       always set when any packet is sent by a Display.
 *                       if Reply, Q flag is ignored.
 *               Q:      Query flag, requests len data from ID at offset
 *                       (no data sent). if clear, is a Write buffer packet
 *               P:      Push flag, for display synchronization, or marks
 *                       last packet of Reply
 *
 *   byte  1:    Flags: x x x x n n n n
 *               x:      reserved for future use (set to zero)
 *               nnnn:   sequence number from 1-15, or zero if not used.
 *                       a sender can send duplicate packets with the same
 *                       sequence number and DDP header for redundancy.
 *                       a receiver can ignore duplicates received back-to-back.
 *                       the sequence number is ignored if zero.
 *
 *   byte  2:    data type: S x t t t b b b
 *               S:      0 = standard types, 1 = custom
 *               x:      reserved for future use (set to zero)
 *               ttt:    type, 0 = greyscale, 1 = rgb, 2 = hsl?
 *               bbb:    bits per pixel:
 *                       0=1, 1=4, 2=8, 3=16, 4=24, 5=32, 6=48, 7=64
 *
 *   byte  3:    Source or Destination ID
 *               0 = reserved
 *               1 = default output device
 *               2=249 custom IDs, (possibly defined via JSON config) ?????
 *               246 = JSON control (read/write)
 *               250 = JSON config  (read/write)
 *               251 = JSON status  (read only)
 *               254 = DMX transit
 *               255 = all devices
 *
 *   byte  4-7:  data offset in bytes (in units based on data-type.
 *               ie: RGB=3 bytes=1 unit) or bytes??  32-bit number, MSB first
 *
 *   byte  8-9:  data length in bytes (size of data field when writing)
 *               16-bit number, MSB first
 *               for Queries, this specifies size of data to read,
 *                   no data field follows header.
 *
 *  if T flag, header extended 4 bytes for timecode field (not counted in
 *  data length)
 *
 *  byte 10-13: timecode
 *
 *  if no T flag, data starts at byte 10, otherwise byte 14
 *
 *  byte 10 or 14: start of data
 *
 */

#include "common.h"
#include "DDP.h"
#include "log.h"
#include "Sequence.h"

/*
 *
 */
DDPOutput::DDPOutput(unsigned int startChannel, unsigned int channelCount)
  : ChannelOutputBase(startChannel, channelCount)
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::DDPOutput(%u, %u)\n",
		startChannel, channelCount);

	// Set any max channels limit if necessary
	m_maxChannels = FPPD_MAX_CHANNELS;
}

/*
 *
 */
DDPOutput::~DDPOutput()
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::~DDPOutput()\n");
}

/*
 *
 */
int DDPOutput::Init(Json::Value config)
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::Init(JSON)\n");

	// Call the base class' Init() method, do not remove this line.
	return ChannelOutputBase::Init(config);
}

/*
 *
 */
int DDPOutput::Close(void)
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::Close()\n");

	return ChannelOutputBase::Close();
}

/*
 *
 */
int DDPOutput::RawSendData(unsigned char *channelData)
{
	LogExcess(VB_CHANNELOUT, "DDPOutput::RawSendData(%p)\n", channelData);

	HexDump("DDPOutput::RawSendData", channelData, m_channelCount);

	return m_channelCount;
}

/*
 *
 */
void DDPOutput::DumpConfig(void)
{
	LogDebug(VB_CHANNELOUT, "DDPOutput::DumpConfig()\n");

	// Call the base class' DumpConfig() method, do not remove this line.
	ChannelOutputBase::DumpConfig();
}

