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
#include <mutex>
#include <string>

class MediaOutputStatus {
public:
    // The timing fields are written by whoever owns the output -- the
    // playlist on the main loop, a command's thread -- and read for status by
    // the API threads, and a resume or a slot's reset writes them from another
    // thread again.  Writes and those cross-thread reads hold this.  A leaf:
    // held only to copy or write fields, never across a call.
    static std::mutex& FieldsLock() {
        static std::mutex m;
        return m;
    }

    // Set from GStreamer's bus thread when media ends or fails, read by the
    // playlist on the main loop.
    std::atomic<int> status{ 0 };
    // Zero until the output first reports: Play Media's private status is a
    // member of a heap object, and Media Slot Status read it before then.
    int secondsElapsed = 0;
    int subSecondsElapsed = 0;
    int secondsRemaining = 0;
    int subSecondsRemaining = 0;
    int minutesTotal = 0;
    int secondsTotal = 0;
    float mediaSeconds = 0.0f;
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
