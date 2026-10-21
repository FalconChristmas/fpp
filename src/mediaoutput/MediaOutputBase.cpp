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

#include "fpp-pch.h"

#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>

#include <cmath> 

#include "../log.h"
#include "../settings.h"

#include "MediaOutputBase.h"

MediaOutputBase::MediaOutputBase(void) {
    LogDebug(VB_MEDIAOUT, "MediaOutputBase::MediaOutputBase()\n");

    if (getFPPmode() != REMOTE_MODE) {
        mediaOffsetMS = getSettingInt("globalMediaOffset", 0);
        mediaOffset = (float)mediaOffsetMS * 0.001;
    }
}

MediaOutputBase::~MediaOutputBase() {
    LogDebug(VB_MEDIAOUT, "MediaOutputBase::~MediaOutputBase()\n");
    Close();
}

/*
 *
 */
int MediaOutputBase::Start(int msTime) {
    return 0;
}

/*
 *
 */
int MediaOutputBase::Stop(void) {
    return 0;
}

/*
 *
 */
int MediaOutputBase::Process(void) {
    return 0;
}

/*
 *
 */
int MediaOutputBase::Close(void) {
    LogDebug(VB_MEDIAOUT, "MediaOutputBase::Close\n");

    return 1;
}

/*
 *
 */
int MediaOutputBase::AdjustSpeed(float masterPos) {
    return 1;
}

/*
 *
 */
void MediaOutputBase::SetVolume(int volume) {
}

/*
 *
 */
int MediaOutputBase::IsPlaying(void) {
    return 0;
}

void MediaOutputBase::setMediaElapsed(float curtime, float remaining) {
    float secs = curtime;
    float rem = remaining;
    if (mediaOffset > 0) {
        secs -= mediaOffset;
    } else {
        rem -= mediaOffset;
    }
    if (secs < 0.0) {
        secs = 0.0;
    }
    if (rem < 0.0) {
        rem = 0.0;
    }
    float elapsed, elapsedSub, remain, remainSub;
    elapsedSub = std::modf(secs, &elapsed) * 100;
    remainSub = std::modf(rem, &remain) * 100;

    std::lock_guard<std::mutex> lock(MediaOutputStatus::FieldsLock());
    m_mediaOutputStatus->mediaSeconds = secs;
    m_mediaOutputStatus->secondsElapsed = elapsed;
    m_mediaOutputStatus->subSecondsElapsed = elapsedSub;
    m_mediaOutputStatus->secondsRemaining = remain;
    m_mediaOutputStatus->subSecondsRemaining = remainSub;
}
