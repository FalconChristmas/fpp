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
// Serializes DRM work on the display cards' shared master fds (see
// mediaoutput/DrmCards.h) -- the KMS framebuffer outputs' flips and modesets,
// and media pipelines taking leases -- and the KMS page-flip bookkeeping.  A
// flip holds it for most of a refresh, so take it only for DRM work, and never
// take mediaOutputLock while holding it.
extern std::mutex drmMasterLock;
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

// Media prepared ahead of the Start() that plays it (GStreamerOutput::
// Prepare()), so that the next video in a playlist starts without building
// its pipeline or decoding its first frame -- the gap a looping video showed
// at every pass.  A prepare is only a guess at what plays next: whatever
// decides what really does claims it, and anything else prepared for the
// slot is thrown away.  At most one per slot.
//
// Prepare: on a worker, so it never holds up the caller.  A no-op if the
// same is already prepared or being prepared, and for anything that isn't
// prepared (see GStreamerOutput::Prepare()).
void PrepareMediaOutput(const std::string& mediaFilename, const std::string& videoOut, int streamSlot);
// The output prepared for exactly this (waiting for a prepare still under
// way), or nullptr.  Anything else prepared for the slot is discarded.
MediaOutputBase* ClaimPreparedMediaOutput(const std::string& mediaFilename, const std::string& videoOut, int streamSlot);
// Throw away what is prepared for the slot, or for every slot (0).
void DiscardPreparedMediaOutput(int streamSlot = 0);
// A MultiSync remote: the master expects to play `filename` next
// (SYNC_PKT_PREPARE).  Prepare what this remote would play for it, as
// OpenMediaOutput() would choose it.  No-op outside remote mode.
void PrepareRemoteMediaOutput(const std::string& filename);

// "--Default--" or "" -> the VideoOutput setting, or the display when none is
// configured (an HDMI connector on Linux, the video window on macOS), else
// "--Disabled--". Anything else is returned unchanged.
std::string ResolveVideoOutput(const std::string& requested);

/* If try, filename will be updated with the media filename */
bool HasVideoForMedia(std::string& filename);

bool IsExtensionVideo(const std::string& ext);
bool IsExtensionAudio(const std::string& ext);

// The ALSA card this boot plays on: AudioOutput, unless boot fell back to
// another card (the selected HDMI has no display, or the card is missing), in
// which case fppinit recorded the fallback in /run/fppd/audio-output.json
// while leaving AudioOutput as the user's choice.  Use this where the card
// that is actually playing matters (mixer, sink latency); use AudioOutput
// where the user's choice does (the missing-card warning, the settings UI).
std::string GetEffectiveAudioOutput();

// volume control
void setVolume(int volume);
int getVolume(void);
// The linear gain a master volume (0-100) stands for: the cube of the
// fraction, as pactl's percentage is.  For an output the master reaches
// without pactl (the AES67 and Opus RTP senders), so the same number still
// means the same loudness there.
double masterVolumeGain(int volume);
