#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <pthread.h>

#include "MediaOutputBase.h"
#include "MediaOutputStatus.h"

extern MediaOutputBase* mediaOutput;
extern pthread_mutex_t mediaOutputLock;
extern MediaOutputStatus mediaOutputStatus;

void InitMediaOutput(void);
void CleanupMediaOutput(void);

bool MatchesRunningMediaFilename(const char* filename);
int OpenMediaOutput(const char* filename);
int StartMediaOutput(const char* filename);
void UpdateMasterMediaPosition(const char* filename, float seconds);
void CloseMediaOutput();

MediaOutputBase* CreateMediaOutput(const std::string& mediaFilename, const std::string& videoOut);

/* If try, filename will be updated with the media filename */
bool HasVideoForMedia(std::string& filename);

bool IsExtensionVideo(const std::string& ext);
bool IsExtensionAudio(const std::string& ext);

//volume control
void setVolume(int volume);
int getVolume(void);
