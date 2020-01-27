/*
 *   E131 Defines for Falcon Player (FPP)
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

#ifndef _E131_DEFS_H
#define _E131_DEFS_H


#define E131_HEADER_LENGTH    126
#define MAX_STEPS_OUT_OF_SYNC 2

#define E131_DEST_PORT        5568

#define E131_UNIVERSE_INDEX   113
#define E131_SEQUENCE_INDEX   111
#define E131_COUNT_INDEX      123
#define E131_START_CODE       125
#define E131_PRIORITY_INDEX   108

#define E131_RLP_COUNT_INDEX       16
#define E131_FRAMING_COUNT_INDEX   38
#define E131_DMP_COUNT_INDEX       115

#define E131_VECTOR_INDEX                21
#define E131_EXTENDED_PACKET_TYPE_INDEX  43
#define VECTOR_E131_EXTENDED_SYNCHRONIZATION  0x1
#define VECTOR_ROOT_E131_DATA       0x4
#define VECTOR_ROOT_E131_EXTENDED   0x8

#endif
