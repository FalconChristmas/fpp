#pragma once
/*
 *   E131 / ArtNet Universe structure definition for Falcon Player (FPP)
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


#define E131_TYPE_MULTICAST   0
#define E131_TYPE_UNICAST     1
#define ARTNET_TYPE_BROADCAST 2
#define ARTNET_TYPE_UNICAST   3

typedef struct {
	uint32_t           active;
	uint32_t           universe;
	uint32_t           size;
	uint32_t           startAddress;
	uint32_t           type;
	char               unicastAddress[16];
	uint32_t           bytesReceived;
	uint32_t           packetsReceived;
    uint32_t           errorPackets;
    uint32_t           lastSequenceNumber;
	uint32_t           priority;
} UniverseEntry;
