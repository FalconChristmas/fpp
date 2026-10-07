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

#include <mutex>

#include "MediaOutputBase.h"
#include "MediaOutputStatus.h"

extern MediaOutputBase* mediaOutput;
extern std::mutex mediaOutputLock;
extern MediaOutputStatus mediaOutputStatus;

void InitMediaOutput(void);
void CleanupMediaOutput(void);
// Bring the primary audio output up now and keep it up for at least holdMs,
// so media started within that window doesn't pay its start-up time.  A no-op
// where the output has nothing to warm (anything but PipeWire).
void PrewarmMediaOutput(int holdMs);

bool MatchesRunningMediaFilename(const std::string& filename);
int OpenMediaOutput(const std::string& filename);
int StartMediaOutput(const std::string& filename, int msTime = 0);
void UpdateMasterMediaPosition(const std::string& filename, float seconds);
void CloseMediaOutput();

MediaOutputBase* CreateMediaOutput(const std::string& mediaFilename, const std::string& videoOut, int streamSlot = 1);

// "--Default--" or "" -> the VideoOutput setting, or the display when none is
// configured (an HDMI connector on Linux, the video window on macOS), else
// "--Disabled--". Anything else is returned unchanged.
std::string ResolveVideoOutput(const std::string& requested);

/* If try, filename will be updated with the media filename */
bool HasVideoForMedia(std::string& filename);

bool IsExtensionVideo(const std::string& ext);
bool IsExtensionAudio(const std::string& ext);

// volume control
void setVolume(int volume);
int getVolume(void);
