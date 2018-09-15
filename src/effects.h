/*
 *   Effects handler for Falcon Player (FPP)
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

// Effect Sequence file format and header definition
#ifndef EFFECTS_H_
#define EFFECTS_H_

#include <stdio.h>

//typedef struct eseqheader {
//} eSeqHeader;

typedef struct fppeffect {
	char     *name;
	FILE     *fp;
	int       stepSize;
	int       modelSize;
	int       startChannel;
	int       loop;
	int       background;
} FPPeffect;

extern FPPeffect *effects[];

int  GetRunningEffects(char *msg, char **result);
int  IsEffectRunning(void);
int  InitEffects(void);
void CloseEffects(void);
int  StartEffect(const char *effectName, int startChannel, int loop = 0);
int  StopEffect(const char *effectName);
int  StopEffect(int effectID);
void StopAllEffects(void);
int  OverlayEffects(char *channelData);

#endif
