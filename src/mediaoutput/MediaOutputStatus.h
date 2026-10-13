#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include <atomic>
#include <string>

class MediaOutputStatus {
public:
    // Set from GStreamer's bus thread when media ends or fails, read by the
    // playlist on the main loop.
    std::atomic<int> status{ 0 };
    int secondsElapsed;
    int subSecondsElapsed;
    int secondsRemaining;
    int subSecondsRemaining;
    int minutesTotal;
    int secondsTotal;
    float mediaSeconds;
    std::string output;
    bool mediaLoading = false;

    MediaOutputStatus() = default;
    // Copyable as it was before status became atomic (slots reset theirs by
    // assigning a fresh one).
    MediaOutputStatus(const MediaOutputStatus& o) { *this = o; }
    MediaOutputStatus& operator=(const MediaOutputStatus& o) {
        status = o.status.load();
        secondsElapsed = o.secondsElapsed;
        subSecondsElapsed = o.subSecondsElapsed;
        secondsRemaining = o.secondsRemaining;
        subSecondsRemaining = o.subSecondsRemaining;
        minutesTotal = o.minutesTotal;
        secondsTotal = o.secondsTotal;
        mediaSeconds = o.mediaSeconds;
        output = o.output;
        mediaLoading = o.mediaLoading;
        return *this;
    }
};
