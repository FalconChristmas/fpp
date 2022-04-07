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

#include <string>

class MediaDetails {
public:
    MediaDetails();
    ~MediaDetails();

    std::string title;
    std::string artist;
    std::string album;
    int year;
    std::string comment;
    int track;
    std::string genre;

    int length;
    int seconds;
    int minutes;
    int lengthMS;

    int bitrate;
    int sampleRate;
    int channels;

    void ParseMedia(const char* mediaFilename);
    void Clear();

    static MediaDetails INSTANCE;

private:
};
