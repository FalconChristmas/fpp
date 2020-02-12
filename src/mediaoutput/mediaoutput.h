/*
 *   Media handler for Falcon Player (FPP)
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

#ifndef _MEDIAOUTPUT_H
#define _MEDIAOUTPUT_H

#include <pthread.h>

#include "MediaOutputBase.h"
#include "MediaOutputStatus.h"

extern MediaOutputBase   *mediaOutput;
extern pthread_mutex_t    mediaOutputLock;
extern MediaOutputStatus  mediaOutputStatus;

void InitMediaOutput(void);
void CleanupMediaOutput(void);

bool MatchesRunningMediaFilename(const char *filename);
int  OpenMediaOutput(const char *filename);
int  StartMediaOutput(const char *filename);
void UpdateMasterMediaPosition(const char *filename, float seconds);
void CloseMediaOutput();

MediaOutputBase *CreateMediaOutput(const std::string &mediaFilename, const std::string &videoOut);

/* If try, filename will be updated with the media filename */
bool HasVideoForMedia(std::string &filename);

bool IsExtensionVideo(const std::string &ext);
bool IsExtensionAudio(const std::string &ext);

//volume control
void setVolume(int volume);
int  getVolume(void);
#endif /* _MEDIAOUTPUT_H */

