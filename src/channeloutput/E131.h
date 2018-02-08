/*
 *   E131 output handler for Falcon Pi Player (FPP)
 *
 *   Copyright (C) 2013 the Falcon Pi Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#ifndef _E131_H
#define _E131_H

#include "channeloutput.h"

#define MAX_UNIVERSE_COUNT    512
#define E131_HEADER_LENGTH    126
#define MAX_STEPS_OUT_OF_SYNC 2

#define E131_DEST_PORT        5568
#define E131_SOURCE_PORT      58301

#define E131_UNIVERSE_INDEX   113
#define E131_SEQUENCE_INDEX   111
#define E131_COUNT_INDEX      123
#define E131_PRIORITY_INDEX   108

#define E131_RLP_COUNT_INDEX       16
#define E131_FRAMING_COUNT_INDEX   38
#define E131_DMP_COUNT_INDEX       115

void LoadUniversesFromFile();

/* Prototypes for helpers in E131.c */
void ShowDiff(void);
void LoadUniversesFromFile();
void UniversesPrint();

/* Expose our interface */
extern FPPChannelOutput E131Output;

#endif
